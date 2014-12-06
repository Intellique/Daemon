/****************************************************************************\
*                    ______           _      ____                            *
*                   / __/ /____  ____(_)__ _/ __ \___  ___                   *
*                  _\ \/ __/ _ \/ __/ / _ `/ /_/ / _ \/ -_)                  *
*                 /___/\__/\___/_/ /_/\_, /\____/_//_/\__/                   *
*                                      /_/                                   *
*  ------------------------------------------------------------------------  *
*  This file is a part of Storiq One                                         *
*                                                                            *
*  Storiq One is free software; you can redistribute it and/or modify        *
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

// pthread_cond_destroy, pthread_cond_init, pthread_cond_signal, pthread_cond_wait
// pthread_mutex_destroy, pthread_mutex_init, pthread_mutex_lock, pthread_mutex_lock
#include <pthread.h>
// calloc, free, malloc
#include <stdlib.h>
// memcpy
#include <string.h>

#include <libstoriqone/checksum.h>
#include <libstoriqone/thread_pool.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/io.h>

#include "checksum.h"


struct soj_stream_checksum_backend_private {
	struct so_checksum ** checksums;
	unsigned int nb_checksums;
	struct so_value * digests;
	bool computed;
};

struct soj_stream_checksum_threaded_backend_private {
	struct soj_linked_list_block {
		void * data;
		ssize_t length;
		struct soj_linked_list_block * next;
	} * volatile first_block, * volatile last_block;

	volatile bool stop;

	pthread_mutex_t lock;
	pthread_cond_t wait;

	struct soj_stream_checksum_backend * backend;
};


static struct so_value * soj_stream_checksum_backend_digest(struct soj_stream_checksum_backend * worker);
static void soj_stream_checksum_backend_finish(struct soj_stream_checksum_backend * worker);
static void soj_stream_checksum_backend_free(struct soj_stream_checksum_backend * worker);
static void soj_stream_checksum_backend_update(struct soj_stream_checksum_backend * worker, const void * buffer, ssize_t length);

static struct so_value * soj_stream_checksum_threaded_backend_digest(struct soj_stream_checksum_backend * worker);
static void soj_stream_checksum_threaded_backend_finish(struct soj_stream_checksum_backend * worker);
static void soj_stream_checksum_threaded_backend_free(struct soj_stream_checksum_backend * worker);
static void soj_stream_checksum_threaded_backend_update(struct soj_stream_checksum_backend * worker, const void * buffer, ssize_t length);
static void soj_stream_checksum_threaded_backend_work(void * arg);

static struct soj_stream_checksum_backend_ops soj_stream_checksum_backend_ops = {
	.digest = soj_stream_checksum_backend_digest,
	.finish = soj_stream_checksum_backend_finish,
	.free   = soj_stream_checksum_backend_free,
	.update = soj_stream_checksum_backend_update,
};

static struct soj_stream_checksum_backend_ops soj_stream_checksum_threaded_backend_ops = {
	.digest = soj_stream_checksum_threaded_backend_digest,
	.finish = soj_stream_checksum_threaded_backend_finish,
	.free   = soj_stream_checksum_threaded_backend_free,
	.update = soj_stream_checksum_threaded_backend_update,
};


static struct so_value * soj_stream_checksum_backend_digest(struct soj_stream_checksum_backend * worker) {
	struct soj_stream_checksum_backend_private * self = worker->data;
	return so_value_copy(self->digests, true);
}

static void soj_stream_checksum_backend_finish(struct soj_stream_checksum_backend * worker) {
	struct soj_stream_checksum_backend_private * self = worker->data;

	unsigned int i;
	for (i = 0; i < self->nb_checksums; i++) {
		struct so_checksum * ck = self->checksums[i];
		char * hash = ck->ops->digest(ck);

		so_value_hashtable_put2(self->digests, ck->driver->name, so_value_new_string(hash), true);
	}
}

static void soj_stream_checksum_backend_free(struct soj_stream_checksum_backend * worker) {
	struct soj_stream_checksum_backend_private * self = worker->data;

	unsigned int i;
	for (i = 0; i < self->nb_checksums; i++) {
		struct so_checksum * ck = self->checksums[i];
		ck->ops->free(ck);
	}
	so_value_free(self->digests);
	free(self);
}

struct soj_stream_checksum_backend * soj_stream_checksum_backend_new(struct so_value * checksums) {
	unsigned int nb_checksums = so_value_list_get_length(checksums);

	struct soj_stream_checksum_backend_private * self = malloc(sizeof(struct soj_stream_checksum_backend_private));
	self->checksums = calloc(nb_checksums, sizeof(struct st_checksum *));
	self->digests = calloc(nb_checksums, sizeof(char *));
	self->nb_checksums = nb_checksums;
	self->computed = false;

	unsigned int i;
	struct so_value_iterator * iter = so_value_list_get_iterator(checksums);
	for (i = 0; i < nb_checksums && so_value_iterator_has_next(iter); i++) {
		struct so_value * chcksum = so_value_iterator_get_value(iter, false);

		struct so_checksum_driver * driver = so_checksum_get_driver(so_value_string_get(chcksum));
		self->checksums[i] = driver->new_checksum();
	}

	struct soj_stream_checksum_backend * backend = malloc(sizeof(struct soj_stream_checksum_backend));
	backend->ops = &soj_stream_checksum_backend_ops;
	backend->data = self;

	return backend;
}

static void soj_stream_checksum_backend_update(struct soj_stream_checksum_backend * worker, const void * buffer, ssize_t length) {
	struct soj_stream_checksum_backend_private * self = worker->data;

	unsigned int i;
	for (i = 0; i < self->nb_checksums; i++) {
		struct so_checksum * ck = self->checksums[i];
		ck->ops->update(ck, buffer, length);
	}
}


static struct so_value * soj_stream_checksum_threaded_backend_digest(struct soj_stream_checksum_backend * worker) {
	struct soj_stream_checksum_threaded_backend_private * self = worker->data;
	return self->backend->ops->digest(self->backend);
}

static void soj_stream_checksum_threaded_backend_finish(struct soj_stream_checksum_backend * worker) {
	struct soj_stream_checksum_threaded_backend_private * self = worker->data;

	pthread_mutex_lock(&self->lock);
	self->stop = true;
	pthread_cond_signal(&self->wait);
	pthread_cond_wait(&self->wait, &self->lock);
	pthread_mutex_unlock(&self->lock);
}

static void soj_stream_checksum_threaded_backend_free(struct soj_stream_checksum_backend * worker) {
	struct soj_stream_checksum_threaded_backend_private * self = worker->data;
	self->backend->ops->free(self->backend);
	free(self);
	free(worker);
}

struct soj_stream_checksum_backend * soj_stream_checksum_threaded_backend_new(struct so_value * checksums) {
	struct soj_stream_checksum_threaded_backend_private * self = malloc(sizeof(struct soj_stream_checksum_threaded_backend_private));
	self->first_block = self->last_block = NULL;

	self->stop = false;

	pthread_mutex_init(&self->lock, NULL);
	pthread_cond_init(&self->wait, NULL);

	self->backend = soj_stream_checksum_backend_new(checksums);

	so_thread_pool_run2("checksum worker", soj_stream_checksum_threaded_backend_work, self, 8);

	struct soj_stream_checksum_backend * backend = malloc(sizeof(struct soj_stream_checksum_backend));
	backend->ops = &soj_stream_checksum_threaded_backend_ops;
	backend->data = self;

	return backend;
}

static void soj_stream_checksum_threaded_backend_update(struct soj_stream_checksum_backend * worker, const void * buffer, ssize_t length) {
	struct soj_stream_checksum_threaded_backend_private * self = worker->data;

	struct soj_linked_list_block * block = malloc(sizeof(struct soj_linked_list_block));
	block->data = malloc(length);
	memcpy(block->data, buffer, length);
	block->length = length;
	block->next = NULL;

	pthread_mutex_lock(&self->lock);
	if (self->first_block == NULL)
		self->first_block = self->last_block = block;
	else
		self->last_block = self->last_block->next = block;
	pthread_cond_signal(&self->wait);
	pthread_mutex_unlock(&self->lock);
}

static void soj_stream_checksum_threaded_backend_work(void * arg) {
	struct soj_stream_checksum_threaded_backend_private * self = arg;

	for (;;) {
		pthread_mutex_lock(&self->lock);
		if (self->stop && self->first_block == NULL)
			break;

		if (self->first_block == NULL)
			pthread_cond_wait(&self->wait, &self->lock);

		struct soj_linked_list_block * block = self->first_block;
		self->first_block = self->last_block = NULL;
		pthread_mutex_unlock(&self->lock);

		while (block != NULL) {
			self->backend->ops->update(self->backend, block->data, block->length);

			struct soj_linked_list_block * next = block->next;
			free(block->data);
			free(block);

			block = next;
		}
	}

	self->backend->ops->finish(self->backend);

	pthread_cond_signal(&self->wait);
	pthread_mutex_unlock(&self->lock);
}

