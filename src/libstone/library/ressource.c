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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 11 Feb 2013 13:48:13 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
#include <errno.h>
// pthread_mutex_destroy, pthread_mutex_init, pthread_mutex_trylock,
// pthread_mutex_unlock, pthread_mutexattr_destroy, pthread_mutexattr_init,
// pthread_mutexattr_settype
#include <pthread.h>
// va_arg, va_end, va_start
#include <stdarg.h>
// calloc, free, malloc
#include <stdlib.h>
// gettimeofday
#include <sys/time.h>

#include <libstone/library/ressource.h>


struct st_ressource_private {
	pthread_mutex_t lock;
};

static int st_ressource_private_free(struct st_ressource * res);
static int st_ressource_private_lock(struct st_ressource * res);
static int st_ressource_private_timed_lock(struct st_ressource * res, unsigned int timeout);
static int st_ressource_private_try_lock(struct st_ressource * res);
static void st_ressource_private_unlock(struct st_ressource * res);

static struct st_ressource_ops st_ressource_private_ops = {
	.free       = st_ressource_private_free,
	.lock       = st_ressource_private_lock,
	.timed_lock = st_ressource_private_timed_lock,
	.try_lock   = st_ressource_private_try_lock,
	.unlock     = st_ressource_private_unlock,
};


int st_ressource_lock(int nb_res, struct st_ressource * res1, struct st_ressource * res2, ...) {
	struct st_ressource ** res = calloc(nb_res, sizeof(struct st_ressource *));
	res[0] = res1;
	res[1] = res2;

	int i, failed = 0;
	va_list resX;
	va_start(resX, res2);

	for (i = 2; i < nb_res; i++)
		res[i] = va_arg(resX, struct st_ressource *);

	va_end(resX);

	for (i = 0; i < nb_res && !failed; i++)
		failed = res[i]->ops->try_lock(res[i]);

	if (failed) {
		int j;
		for (i--, j = 0; j < i; j++)
			res[j]->ops->unlock(res[j]);
	}

	free(res);

	return failed;
}

struct st_ressource * st_ressource_new(bool recursive_lock) {
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	if (recursive_lock)
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	else
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_FAST_NP);

	struct st_ressource_private * self = malloc(sizeof(struct st_ressource_private));
	pthread_mutex_init(&self->lock, &attr);

	struct st_ressource * res = malloc(sizeof(struct st_ressource));
	res->ops = &st_ressource_private_ops;
	res->data = self;
	res->locked = 0;

	pthread_mutexattr_destroy(&attr);
	return res;
}

static int st_ressource_private_free(struct st_ressource * res) {
	if (res == NULL)
		return 2;

	struct st_ressource_private * self = res->data;

	if (pthread_mutex_trylock(&self->lock) == EBUSY)
		return 1;

	pthread_mutex_unlock(&self->lock);
	pthread_mutex_destroy(&self->lock);

	free(self);
	free(res);

	return 0;
}

static int st_ressource_private_lock(struct st_ressource * res) {
	if (res == NULL)
		return 2;

	struct st_ressource_private * self = res->data;
	int failed = pthread_mutex_lock(&self->lock);
	if (!failed)
		res->locked++;
	return failed;
}

static int st_ressource_private_timed_lock(struct st_ressource * res, unsigned int timeout) {
	if (res == NULL)
		return 2;

	struct timeval now;
	struct timespec ts_timeout;
	gettimeofday(&now, NULL);
	ts_timeout.tv_sec = now.tv_sec + timeout / 1000;
	ts_timeout.tv_nsec = now.tv_usec * 1000 + timeout % 1000;

	if (ts_timeout.tv_nsec > 1000000) {
		ts_timeout.tv_nsec -= 1000000;
		ts_timeout.tv_sec++;
	}

	struct st_ressource_private * self = res->data;
	int failed = pthread_mutex_timedlock(&self->lock, &ts_timeout);
	if (!failed)
		res->locked = 1;

	return failed;
}

static int st_ressource_private_try_lock(struct st_ressource * res) {
	if (res == NULL)
		return 2;

	struct st_ressource_private * self = res->data;
	int failed = pthread_mutex_trylock(&self->lock);
	if (!failed)
		res->locked = 1;
	return failed;
}

static void st_ressource_private_unlock(struct st_ressource * res) {
	if (res == NULL)
		return;

	struct st_ressource_private * self = res->data;
	if (!pthread_mutex_unlock(&self->lock))
		res->locked--;
}

