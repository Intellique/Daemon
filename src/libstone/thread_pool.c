/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
*  it under the terms of the GNU Affero General Public License               *
*  as published by the Free Software Foundation; either version 3            *
*  of the License, or (at your option) any later version.                    *
*                                                                            *
*  This program is distributed in the hope that it will be useful,           *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
*  GNU Affero General Public License for more details.                       *
*                                                                            *
*  You should have received a copy of the GNU Affero General Public License  *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

#define _GNU_SOURCE
// open
#include <fcntl.h>
// gettext
#include <libintl.h>
// pthread_attr_destroy, pthread_attr_init, pthread_attr_setdetachstate
// pthread_cond_init, pthread_cond_signal, pthread_cond_timedwait
// pthread_create, pthread_join, pthread_mutex_init, pthread_mutex_lock
// pthread_mutex_unlock
#include <pthread.h>
// free, malloc, realloc
#include <stdlib.h>
// asprintf
#include <stdio.h>
// strdup
#include <string.h>
// setpriority
#include <sys/resource.h>
// syscall
#include <sys/syscall.h>
// open
#include <sys/stat.h>
// gettimeofday, setpriority
#include <sys/time.h>
// pid_t, open
#include <sys/types.h>
// close, getpid, syscall, write
#include <unistd.h>

#include <libstone/log.h>
#include <libstone/string.h>

#include "thread_pool.h"

struct st_thread_pool_thread {
	pthread_t thread;
	pthread_mutex_t lock;
	pthread_cond_t wait;

	char * name;
	st_thread_pool_f_v1 function;
	void * arg;
	int nice;

	volatile enum {
		st_thread_pool_state_exited,
		st_thread_pool_state_running,
		st_thread_pool_state_waiting,
	} state;
};

static struct st_thread_pool_thread ** st_thread_pool_threads = NULL;
static unsigned int st_thread_pool_nb_threads = 0;
static pthread_mutex_t st_thread_pool_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static pid_t st_thread_pool_pid = -1;

static void st_thread_pool_exit(void) __attribute__((destructor));
static void st_thread_pool_init(void) __attribute__((constructor));
static void st_thread_pool_set_name(pid_t tid, const char * name);
static void * st_thread_pool_work(void * arg);


static void st_thread_pool_exit() {
	unsigned int i;
	for (i = 0; i < st_thread_pool_nb_threads; i++) {
		struct st_thread_pool_thread * th = st_thread_pool_threads[i];

		pthread_mutex_lock(&th->lock);
		if (th->state == st_thread_pool_state_waiting)
			pthread_cond_signal(&th->wait);
		pthread_mutex_unlock(&th->lock);

		pthread_join(th->thread, NULL);

		free(th);
	}

	free(st_thread_pool_threads);
	st_thread_pool_threads = NULL;
	st_thread_pool_nb_threads = 0;
}

static void st_thread_pool_init() {
	st_thread_pool_pid = getpid();
}

__asm__(".symver st_thread_pool_run_v1, st_thread_pool_run@@LIBSTONE_1.2");
int st_thread_pool_run_v1(const char * thread_name, void (*function)(void * arg), void * arg) {
	return st_thread_pool_run2(thread_name, function, arg, 0);
}

__asm__(".symver st_thread_pool_run2_v1, st_thread_pool_run2@@LIBSTONE_1.2");
int st_thread_pool_run2_v1(const char * thread_name, void (*function)(void * arg), void * arg, int nice) {
	pthread_mutex_lock(&st_thread_pool_lock);
	unsigned int i;
	for (i = 0; i < st_thread_pool_nb_threads; i++) {
		struct st_thread_pool_thread * th = st_thread_pool_threads[i];

		if (th->state == st_thread_pool_state_waiting) {
			pthread_mutex_lock(&th->lock);

			th->name = NULL;
			if (thread_name != NULL)
				th->name = strdup(thread_name);
			th->function = function;
			th->arg = arg;
			th->nice = nice;
			th->state = st_thread_pool_state_running;

			pthread_cond_signal(&th->wait);
			pthread_mutex_unlock(&th->lock);

			pthread_mutex_unlock(&st_thread_pool_lock);

			return 0;
		}
	}

	for (i = 0; i < st_thread_pool_nb_threads; i++) {
		struct st_thread_pool_thread * th = st_thread_pool_threads[i];

		if (th->state == st_thread_pool_state_exited) {
			th->name = NULL;
			if (thread_name != NULL)
				th->name = strdup(thread_name);
			th->function = function;
			th->arg = arg;
			th->nice = nice;
			th->state = st_thread_pool_state_running;

			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

			pthread_create(&th->thread, &attr, st_thread_pool_work, th);

			pthread_mutex_unlock(&st_thread_pool_lock);

			pthread_attr_destroy(&attr);

			return 0;
		}
	}

	void * new_addr = realloc(st_thread_pool_threads, (st_thread_pool_nb_threads + 1) * sizeof(struct st_thread_pool_thread *));
	if (new_addr == NULL) {
		st_log_write(st_log_level_error, gettext("st_thread_pool_run2: error, not enought memory to start new thread"));
		return 1;
	}

	st_thread_pool_threads = new_addr;
	struct st_thread_pool_thread * th = st_thread_pool_threads[st_thread_pool_nb_threads] = malloc(sizeof(struct st_thread_pool_thread));
	st_thread_pool_nb_threads++;

	th->name = NULL;
	if (thread_name != NULL)
		th->name = strdup(thread_name);
	th->function = function;
	th->arg = arg;
	th->nice = nice;
	th->state = st_thread_pool_state_running;

	pthread_mutex_init(&th->lock, NULL);
	pthread_cond_init(&th->wait, NULL);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	pthread_create(&th->thread, &attr, st_thread_pool_work, th);

	pthread_mutex_unlock(&st_thread_pool_lock);

	pthread_attr_destroy(&attr);

	return 0;
}

static void st_thread_pool_set_name(pid_t tid, const char * name) {
	char * path;
	asprintf(&path, "/proc/%d/task/%d/comm", st_thread_pool_pid, tid);

	int fd = open(path, O_WRONLY);
	if (fd < 0) {
		free(path);
		return;
	}

	char * th_name = strdup(name);
	st_string_middle_elipsis(th_name, 15);

	write(fd, th_name, strlen(th_name) + 1);
	close(fd);
	free(path);
	free(th_name);
}

static void * st_thread_pool_work(void * arg) {
	struct st_thread_pool_thread * th = arg;

	pid_t tid = syscall(SYS_gettid);

	st_log_write(st_log_level_debug, gettext("st_thread_pool_work: starting new thread #%ld (pid: %d) to function: %p with parameter: %p"), th->thread, tid, th->function, th->arg);

	do {
		setpriority(PRIO_PROCESS, tid, th->nice);

		if (th->name != NULL)
			st_thread_pool_set_name(tid, th->name);
		else {
			char buffer[16];
			snprintf(buffer, 16, "thread %p", th->function);
			st_thread_pool_set_name(tid, buffer);
		}

		th->function(th->arg);

		st_thread_pool_set_name(tid, "idle");

		free(th->name);
		th->name = NULL;

		st_log_write(st_log_level_debug, gettext("st_thread_pool_work: thread #%ld (pid: %d) is going to sleep"), th->thread, tid);

		pthread_mutex_lock(&th->lock);

		th->function = NULL;
		th->arg = NULL;
		th->state = st_thread_pool_state_waiting;

		struct timeval now;
		struct timespec timeout;
		gettimeofday(&now, NULL);
		timeout.tv_sec = now.tv_sec + 300;
		timeout.tv_nsec = now.tv_usec * 1000;

		pthread_cond_timedwait(&th->wait, &th->lock, &timeout);

		if (th->state != st_thread_pool_state_running)
			th->state = st_thread_pool_state_exited;

		pthread_mutex_unlock(&th->lock);

		if (th->state == st_thread_pool_state_running)
			st_log_write(st_log_level_debug, gettext("st_thread_pool_work: restarting thread #%ld (pid: %d) to function: %p with parameter: %p"), th->thread, tid, th->function, th->arg);

	} while (th->state == st_thread_pool_state_running);

	st_log_write(st_log_level_debug, gettext("st_thread_pool_work: thread #%ld is dead"), th->thread);

	return NULL;
}

