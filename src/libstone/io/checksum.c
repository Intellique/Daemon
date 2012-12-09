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
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sun, 09 Dec 2012 16:09:46 +0100                         *
\*************************************************************************/

// pthread_cond_destroy, pthread_cond_init, pthread_cond_signal, pthread_cond_wait
// pthread_mutex_destroy, pthread_mutex_init, pthread_mutex_lock, pthread_mutex_lock
#include <pthread.h>
// bool
#include <stdbool.h>
// calloc, free, malloc
#include <stdlib.h>
// memcpy, strdup
#include <string.h>

#include <libstone/checksum.h>
#include <libstone/io.h>
#include <libstone/thread_pool.h>

struct st_stream_checksum_private {
	struct st_stream_writer * out;
	bool closed;

	struct st_stream_checksum_backend {
		struct st_stream_checksum_backend_ops {
			char ** (*digest)(struct st_stream_checksum_backend * worker);
			void (*finish)(struct st_stream_checksum_backend * worker);
			void (*free)(struct st_stream_checksum_backend * worker);
			void (*update)(struct st_stream_checksum_backend * worker, const void * buffer, ssize_t length);
		} * ops;
		void * data;
	} * worker;
};

struct st_stream_checksum_backend_private {
	struct st_checksum ** checksums;
    char ** digests;
    unsigned int nb_checksums;
	bool computed;
};

struct st_stream_checksum_threaded_backend_private {
	struct st_linked_list_block {
		void * data;
		ssize_t length;
		struct st_linked_list_block * next;
	} * volatile first_block, * volatile last_block;

	volatile bool stop;

	pthread_mutex_t lock;
	pthread_cond_t wait;

	struct st_stream_checksum_backend * backend;
};

static int st_stream_checksum_close(struct st_stream_writer * sfw);
static void st_stream_checksum_free(struct st_stream_writer * sfw);
static ssize_t st_stream_checksum_get_available_size(struct st_stream_writer * sfw);
static ssize_t st_stream_checksum_get_block_size(struct st_stream_writer * sfw);
static int st_stream_checksum_last_errno(struct st_stream_writer * sfw);
static ssize_t st_stream_checksum_position(struct st_stream_writer * sfw);
static ssize_t st_stream_checksum_write(struct st_stream_writer * sfw, const void * buffer, ssize_t length);

static char ** st_stream_checksum_backend_digest(struct st_stream_checksum_backend * worker);
static void st_stream_checksum_backend_finish(struct st_stream_checksum_backend * worker);
static void st_stream_checksum_backend_free(struct st_stream_checksum_backend * worker);
static struct st_stream_checksum_backend * st_stream_checksum_backend_new(char ** checksums, unsigned int nb_checksums);
static void st_stream_checksum_backend_update(struct st_stream_checksum_backend * worker, const void * buffer, ssize_t length);

static char ** st_stream_checksum_threaded_backend_digest(struct st_stream_checksum_backend * worker);
static void st_stream_checksum_threaded_backend_finish(struct st_stream_checksum_backend * worker);
static void st_stream_checksum_threaded_backend_free(struct st_stream_checksum_backend * worker);
static struct st_stream_checksum_backend * st_stream_checksum_threaded_backend_new(char ** checksums, unsigned int nb_checksums);
static void st_stream_checksum_threaded_backend_update(struct st_stream_checksum_backend * worker, const void * buffer, ssize_t length);
static void st_stream_checksum_threaded_backend_work(void * arg);

static struct st_stream_writer_ops st_stream_checksum_ops = {
	.close              = st_stream_checksum_close,
	.free               = st_stream_checksum_free,
	.get_available_size = st_stream_checksum_get_available_size,
	.get_block_size     = st_stream_checksum_get_block_size,
	.last_errno         = st_stream_checksum_last_errno,
	.position           = st_stream_checksum_position,
	.write              = st_stream_checksum_write,
};

static struct st_stream_checksum_backend_ops st_stream_checksum_backend_ops = {
	.digest = st_stream_checksum_backend_digest,
	.finish = st_stream_checksum_backend_finish,
	.free   = st_stream_checksum_backend_free,
	.update = st_stream_checksum_backend_update,
};

static struct st_stream_checksum_backend_ops st_stream_checksum_threaded_backend_ops = {
	.digest = st_stream_checksum_threaded_backend_digest,
	.finish = st_stream_checksum_threaded_backend_finish,
	.free   = st_stream_checksum_threaded_backend_free,
	.update = st_stream_checksum_threaded_backend_update,
};


static int st_stream_checksum_close(struct st_stream_writer * sfw) {
	struct st_stream_checksum_private * self = sfw->data;

	if (self->closed)
		return 0;

	int failed = 0;
	if (self->out != NULL)
		failed = self->out->ops->close(self->out);

	if (!failed) {
		self->closed = true;
		self->worker->ops->finish(self->worker);
	}

	return failed;
}

static void st_stream_checksum_free(struct st_stream_writer * sfw) {
	struct st_stream_checksum_private * self = sfw->data;

	if (!self->closed)
		self->out->ops->close(self->out);

	if (self->out != NULL)
		self->out->ops->free(self->out);

	self->worker->ops->free(self->worker);
	free(self);
	free(sfw);
}

static ssize_t st_stream_checksum_get_available_size(struct st_stream_writer * sfw) {
	struct st_stream_checksum_private * self = sfw->data;
	if (self->out != NULL)
		return self->out->ops->get_available_size(self->out);
	return 0;
}

static ssize_t st_stream_checksum_get_block_size(struct st_stream_writer * sfw) {
	struct st_stream_checksum_private * self = sfw->data;
	if (self->out != NULL)
		return self->out->ops->get_block_size(self->out);
	return 0;
}

static int st_stream_checksum_last_errno(struct st_stream_writer * sfw) {
	struct st_stream_checksum_private * self = sfw->data;
	if (self->out != NULL)
		return self->out->ops->last_errno(self->out);
	return 0;
}

static ssize_t st_stream_checksum_position(struct st_stream_writer * sfw) {
	struct st_stream_checksum_private * self = sfw->data;
	if (self->out != NULL)
		return self->out->ops->position(self->out);
	return 0;
}

static ssize_t st_stream_checksum_write(struct st_stream_writer * sfw, const void * buffer, ssize_t length) {
	struct st_stream_checksum_private * self = sfw->data;

	if (self->closed)
		return -1;

	ssize_t nb_write = length;
	if (self->out != NULL)
		nb_write = self->out->ops->write(self->out, buffer, length);

	if (nb_write > 0)
		self->worker->ops->update(self->worker, buffer, nb_write);

	return nb_write;
}


static char ** st_stream_checksum_backend_digest(struct st_stream_checksum_backend * worker) {
	struct st_stream_checksum_backend_private * self = worker->data;

	char ** digest = calloc(self->nb_checksums, sizeof(char *));
	unsigned int i;
	for (i = 0; i < self->nb_checksums; i++)
		digest[i] = strdup(self->digests[i]);

	return digest;
}

static void st_stream_checksum_backend_finish(struct st_stream_checksum_backend * worker) {
	struct st_stream_checksum_backend_private * self = worker->data;

	unsigned int i;
	for (i = 0; i < self->nb_checksums; i++) {
		struct st_checksum * ck = self->checksums[i];
		self->digests[i] = ck->ops->digest(ck);
	}
}

static void st_stream_checksum_backend_free(struct st_stream_checksum_backend * worker) {
	struct st_stream_checksum_backend_private * self = worker->data;

	unsigned int i;
	for (i = 0; i < self->nb_checksums; i++) {
		struct st_checksum * ck = self->checksums[i];
		ck->ops->free(ck);

		free(self->digests[i]);
	}

	free(self->checksums);
	free(self->digests);

	free(self);
	free(worker);
}

static struct st_stream_checksum_backend * st_stream_checksum_backend_new(char ** checksums, unsigned int nb_checksums) {
	struct st_stream_checksum_backend_private * self = malloc(sizeof(struct st_stream_checksum_backend_private));
	self->checksums = calloc(nb_checksums, sizeof(struct st_checksum *));
	self->digests = calloc(nb_checksums, sizeof(char *));
	self->nb_checksums = nb_checksums;
	self->computed = false;

	unsigned int i;
	for (i = 0; i < nb_checksums; i++) {
		struct st_checksum_driver * driver = st_checksum_get_driver(checksums[i]);
		self->checksums[i] = driver->new_checksum();
	}

	struct st_stream_checksum_backend * backend = malloc(sizeof(struct st_stream_checksum_backend));
	backend->ops = &st_stream_checksum_backend_ops;
	backend->data = self;

	return backend;
}

static void st_stream_checksum_backend_update(struct st_stream_checksum_backend * worker, const void * buffer, ssize_t length) {
	struct st_stream_checksum_backend_private * self = worker->data;

	unsigned int i;
	for (i = 0; i < self->nb_checksums; i++) {
		struct st_checksum * ck = self->checksums[i];
		ck->ops->update(ck, buffer, length);
	}
}


static char ** st_stream_checksum_threaded_backend_digest(struct st_stream_checksum_backend * worker) {
	struct st_stream_checksum_threaded_backend_private * self = worker->data;
	return self->backend->ops->digest(self->backend);
}

static void st_stream_checksum_threaded_backend_finish(struct st_stream_checksum_backend * worker) {
	struct st_stream_checksum_threaded_backend_private * self = worker->data;

	pthread_mutex_lock(&self->lock);
	self->stop = true;
	pthread_cond_signal(&self->wait);
	pthread_cond_wait(&self->wait, &self->lock);
	pthread_mutex_unlock(&self->lock);
}

static void st_stream_checksum_threaded_backend_free(struct st_stream_checksum_backend * worker) {
	struct st_stream_checksum_threaded_backend_private * self = worker->data;
	self->backend->ops->free(self->backend);
	free(self);
	free(worker);
}

static struct st_stream_checksum_backend * st_stream_checksum_threaded_backend_new(char ** checksums, unsigned int nb_checksums) {
	struct st_stream_checksum_threaded_backend_private * self = malloc(sizeof(struct st_stream_checksum_threaded_backend_private));
	self->first_block = self->last_block = NULL;

	self->stop = false;

	pthread_mutex_init(&self->lock, NULL);
	pthread_cond_init(&self->wait, NULL);

	self->backend = st_stream_checksum_backend_new(checksums, nb_checksums);

	st_thread_pool_run2(st_stream_checksum_threaded_backend_work, self, 8);

	struct st_stream_checksum_backend * backend = malloc(sizeof(struct st_stream_checksum_backend));
	backend->ops = &st_stream_checksum_threaded_backend_ops;
	backend->data = self;

	return backend;
}

static void st_stream_checksum_threaded_backend_update(struct st_stream_checksum_backend * worker, const void * buffer, ssize_t length) {
	struct st_stream_checksum_threaded_backend_private * self = worker->data;

	struct st_linked_list_block * block = malloc(sizeof(struct st_linked_list_block));
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

static void st_stream_checksum_threaded_backend_work(void * arg) {
	struct st_stream_checksum_threaded_backend_private * self = arg;

	for (;;) {
		pthread_mutex_lock(&self->lock);
		if (self->stop && self->first_block == NULL)
			break;
		if (self->first_block == NULL)
			pthread_cond_wait(&self->wait, &self->lock);

		struct st_linked_list_block * block = self->first_block;
		self->first_block = self->last_block = NULL;
		pthread_mutex_unlock(&self->lock);

		while (block != NULL) {
			self->backend->ops->update(self->backend, block->data, block->length);

			struct st_linked_list_block * next = block->next;
			free(block->data);
			free(block);

			block = next;
		}
	}

	self->backend->ops->finish(self->backend);

	pthread_cond_signal(&self->wait);
	pthread_mutex_unlock(&self->lock);
}


char ** st_checksum_writer_get_checksums(struct st_stream_writer * sfw) {
	struct st_stream_checksum_private * self = sfw->data;
	return self->worker->ops->digest(self->worker);
}

struct st_stream_writer * st_checksum_writer_new(struct st_stream_writer * stream, char ** checksums, unsigned int nb_checksums, bool thread_helper) {
	struct st_stream_checksum_private * self = malloc(sizeof(struct st_stream_checksum_private));
	self->out = stream;
	self->closed = false;

	if (thread_helper)
		self->worker = st_stream_checksum_threaded_backend_new(checksums, nb_checksums);
	else
		self->worker = st_stream_checksum_backend_new(checksums, nb_checksums);

	struct st_stream_writer * writer = malloc(sizeof(struct st_stream_writer));
	writer->ops = &st_stream_checksum_ops;
	writer->data = self;

	return writer;
}

