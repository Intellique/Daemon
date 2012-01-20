/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Fri, 20 Jan 2012 15:12:00 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
#include <pthread.h>
// free, malloc, realloc
#include <stdlib.h>
#include <sys/time.h>

#include <stone/log.h>
#include <stone/threadpool.h>

struct st_threadpool_thread {
	pthread_t thread;
	pthread_mutex_t lock;
	pthread_cond_t wait;

	void (*function)(void * arg);
	void * arg;

	volatile enum {
		st_threadpool_state_exited,
		st_threadpool_state_running,
		st_threadpool_state_waiting,
	} state;
};

static void * st_threadpool_work(void * arg);

static struct st_threadpool_thread ** st_threadpool_threads = 0;
static unsigned int st_threadpool_nb_threads = 0;
static pthread_mutex_t st_threadpool_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;


void st_threadpool_run(void (*function)(void * arg), void * arg) {
	pthread_mutex_lock(&st_threadpool_lock);
	unsigned int i;
	for (i = 0; i < st_threadpool_nb_threads; i++) {
		struct st_threadpool_thread * th = st_threadpool_threads[i];

		if (th->state == st_threadpool_state_waiting) {
			pthread_mutex_lock(&th->lock);
			th->function = function;
			th->arg = arg;
			th->state = st_threadpool_state_running;
			pthread_cond_signal(&th->wait);
			pthread_mutex_unlock(&th->lock);

			pthread_mutex_unlock(&st_threadpool_lock);

			return;
		}
	}

	for (i = 0; i < st_threadpool_nb_threads; i++) {
		struct st_threadpool_thread * th = st_threadpool_threads[i];

		if (th->state == st_threadpool_state_exited) {
			th->function = function;
			th->arg = arg;
			th->state = st_threadpool_state_running;

			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

			pthread_create(&th->thread, &attr, st_threadpool_work, th);

			pthread_mutex_unlock(&st_threadpool_lock);

			pthread_attr_destroy(&attr);

			return;
		}
	}

	st_threadpool_threads = realloc(st_threadpool_threads, (st_threadpool_nb_threads + 1) * sizeof(struct st_threadpool_thread *));
	struct st_threadpool_thread * th = st_threadpool_threads[st_threadpool_nb_threads] = malloc(sizeof(struct st_threadpool_thread));
	st_threadpool_nb_threads++;

	th->function = function;
	th->arg = arg;
	th->state = st_threadpool_state_running;

	pthread_mutex_init(&th->lock, 0);
	pthread_cond_init(&th->wait, 0);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_create(&th->thread, &attr, st_threadpool_work, th);

	pthread_mutex_unlock(&st_threadpool_lock);

	pthread_attr_destroy(&attr);
}

void * st_threadpool_work(void * arg) {
	struct st_threadpool_thread * th = arg;

	st_log_write_all(st_log_level_debug, st_log_type_daemon, "Starting new thread #%ld to function: %p with parameter: %p", th->thread, th->function, th->arg);

	do {
		th->function(th->arg);

		st_log_write_all(st_log_level_debug, st_log_type_daemon, "Thread #%ld is going to sleep", th->thread);

		pthread_mutex_lock(&th->lock);

		th->function = 0;
		th->arg = 0;
		th->state = st_threadpool_state_waiting;

		struct timeval now;
		struct timespec timeout;
		gettimeofday(&now, 0);
		timeout.tv_sec = now.tv_sec + 60;
		timeout.tv_nsec = now.tv_usec * 1000;

		pthread_cond_timedwait(&th->wait, &th->lock, &timeout);

		if (th->state != st_threadpool_state_running)
			th->state = st_threadpool_state_exited;

		pthread_mutex_unlock(&th->lock);

		if (th->state == st_threadpool_state_running)
			st_log_write_all(st_log_level_debug, st_log_type_daemon, "Restarting thread #%ld to function: %p with parameter: %p", th->thread, th->function, th->arg);

	} while (th->state == st_threadpool_state_running);

	st_log_write_all(st_log_level_debug, st_log_type_daemon, "Thread #%ld is dead", th->thread);

	return 0;
}

