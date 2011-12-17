/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
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
*  Last modified: Sat, 17 Dec 2011 17:19:11 +0100                         *
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

#include <stone/library/ressource.h>


struct sa_ressource_private {
	pthread_mutex_t lock;
};

static int sa_ressource_private_free(struct sa_ressource * res);
static int sa_ressource_private_lock(struct sa_ressource * res);
static int sa_ressource_private_trylock(struct sa_ressource * res);
static void sa_ressource_private_unlock(struct sa_ressource * res);

struct sa_ressource_ops sa_ressource_private_ops = {
	.free    = sa_ressource_private_free,
	.lock    = sa_ressource_private_lock,
	.trylock = sa_ressource_private_trylock,
	.unlock  = sa_ressource_private_unlock,
};


int sa_ressource_lock(int nb_res, struct sa_ressource * res1, struct sa_ressource * res2, ...) {
	struct sa_ressource ** res = calloc(nb_res, sizeof(struct sa_ressource *));
	res[0] = res1;
	res[1] = res2;

	int i, failed = 0;
	va_list resX;
	va_start(resX, res2);

	for (i = 2; i < nb_res; i++)
		res[i] = va_arg(resX, struct sa_ressource *);

	va_end(resX);

	for (i = 0; i < nb_res && !failed; i++)
		failed = res[i]->ops->trylock(res[i]);

	if (failed) {
		int j;
		for (i--, j = 0; j < i; j++)
			res[j]->ops->unlock(res[j]);
	}

	free(res);

	return failed;
}

struct sa_ressource * sa_ressource_new() {
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	struct sa_ressource_private * self = malloc(sizeof(struct sa_ressource_private));
	pthread_mutex_init(&self->lock, &attr);

	struct sa_ressource * res = malloc(sizeof(struct sa_ressource));
	res->ops = &sa_ressource_private_ops;
	res->data = self;
	res->locked = 0;

	pthread_mutexattr_destroy(&attr);
	return res;
}

int sa_ressource_private_free(struct sa_ressource * res) {
	struct sa_ressource_private * self = res->data;

	if (pthread_mutex_trylock(&self->lock) == EBUSY)
		return 1;

	pthread_mutex_unlock(&self->lock);
	pthread_mutex_destroy(&self->lock);

	free(self);
	free(res);

	return 0;
}

int sa_ressource_private_lock(struct sa_ressource * res) {
	struct sa_ressource_private * self = res->data;
	int failed = pthread_mutex_lock(&self->lock);
	if (!failed)
		res->locked = 1;
	return failed;
}

int sa_ressource_private_trylock(struct sa_ressource * res) {
	struct sa_ressource_private * self = res->data;
	int failed = pthread_mutex_trylock(&self->lock);
	if (!failed)
		res->locked = 1;
	return failed;
}

void sa_ressource_private_unlock(struct sa_ressource * res) {
	struct sa_ressource_private * self = res->data;
	if (!pthread_mutex_unlock(&self->lock))
		res->locked = 0;
}

