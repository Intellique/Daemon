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
*  Copyright (C) 2013-2018, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// pthread_cond_destroy, pthread_cond_init, pthread_cond_signal, pthread_cond_wait
// pthread_mutex_destroy, pthread_mutex_init, pthread_mutex_lock, pthread_mutex_lock
#include <pthread.h>
// bool
#include <stdbool.h>
// calloc, free, malloc
#include <stdlib.h>
// memcpy
#include <string.h>
// bzero
#include <strings.h>
// mmap, munmap
#include <sys/mman.h>
// sysinfo
#include <sys/sysinfo.h>

#include <libstoriqone/checksum.h>
#include <libstoriqone/io.h>
#include <libstoriqone/thread_pool.h>
#include <libstoriqone/value.h>

#include "checksum.h"


struct so_io_stream_checksum_backend_private {
	struct so_checksum ** checksums;
	unsigned int nb_checksums;
	struct so_value * digests;
	bool computed;
};

struct so_io_stream_checksum_threaded_backend_private {
	struct so_io_linked_list_block {
		ssize_t block_length, data_length;
		struct so_io_linked_list_block * previous, * next;
	} * volatile first_block, * volatile last_block, * free_block;

	volatile unsigned long used;
	volatile unsigned long limit;

	volatile bool should_reset;
	volatile bool stop;

	struct so_value * checksums;

	pthread_mutex_t lock;
	pthread_cond_t wait;

	struct so_io_stream_checksum_backend * backend;
};


static struct so_value * so_io_stream_checksum_backend_digest(struct so_io_stream_checksum_backend * worker);
static void so_io_stream_checksum_backend_finish(struct so_io_stream_checksum_backend * worker);
static void so_io_stream_checksum_backend_free(struct so_io_stream_checksum_backend * worker);
static void so_io_stream_checksum_backend_reset(struct so_io_stream_checksum_backend * worker);
static void so_io_stream_checksum_backend_update(struct so_io_stream_checksum_backend * worker, const void * buffer, ssize_t length);

static void * so_io_stream_checksum_threaded_backend_cast(struct so_io_linked_list_block * block, bool base_addr);
static struct so_value * so_io_stream_checksum_threaded_backend_digest(struct so_io_stream_checksum_backend * worker);
static void so_io_stream_checksum_threaded_backend_finish(struct so_io_stream_checksum_backend * worker);
static void so_io_stream_checksum_threaded_backend_free(struct so_io_stream_checksum_backend * worker);
static void so_io_stream_checksum_threaded_backend_update(struct so_io_stream_checksum_backend * worker, const void * buffer, ssize_t length);
static void so_io_stream_checksum_threaded_backend_reset(struct so_io_stream_checksum_backend * worker);
static struct so_io_linked_list_block * so_io_stream_checksum_threaded_backend_sort(struct so_io_linked_list_block * blocks, unsigned int nb_elts);
static void so_io_stream_checksum_threaded_backend_work(void * arg);

static struct so_io_stream_checksum_backend_ops so_io_stream_checksum_backend_ops = {
	.digest = so_io_stream_checksum_backend_digest,
	.finish = so_io_stream_checksum_backend_finish,
	.free   = so_io_stream_checksum_backend_free,
	.reset  = so_io_stream_checksum_backend_reset,
	.update = so_io_stream_checksum_backend_update,
};

static struct so_io_stream_checksum_backend_ops so_io_stream_checksum_threaded_backend_ops = {
	.digest = so_io_stream_checksum_threaded_backend_digest,
	.finish = so_io_stream_checksum_threaded_backend_finish,
	.free   = so_io_stream_checksum_threaded_backend_free,
	.reset  = so_io_stream_checksum_threaded_backend_reset,
	.update = so_io_stream_checksum_threaded_backend_update,
};


static struct so_value * so_io_stream_checksum_backend_digest(struct so_io_stream_checksum_backend * worker) {
	struct so_io_stream_checksum_backend_private * self = worker->data;
	return so_value_copy(self->digests, true);
}

static void so_io_stream_checksum_backend_finish(struct so_io_stream_checksum_backend * worker) {
	struct so_io_stream_checksum_backend_private * self = worker->data;

	unsigned int i;
	for (i = 0; i < self->nb_checksums; i++) {
		struct so_checksum * ck = self->checksums[i];
		char * hash = ck->ops->digest(ck);

		so_value_hashtable_put2(self->digests, ck->driver->name, so_value_new_string(hash), true);

		free(hash);
	}
}

static void so_io_stream_checksum_backend_free(struct so_io_stream_checksum_backend * worker) {
	struct so_io_stream_checksum_backend_private * self = worker->data;

	unsigned int i;
	for (i = 0; i < self->nb_checksums; i++) {
		struct so_checksum * ck = self->checksums[i];
		ck->ops->free(ck);
	}
	free(self->checksums);
	so_value_free(self->digests);
	free(self);
	free(worker);
}

struct so_io_stream_checksum_backend * so_io_stream_checksum_backend_new(struct so_value * checksums) {
	unsigned int nb_checksums = so_value_list_get_length(checksums);

	struct so_io_stream_checksum_backend_private * self = malloc(sizeof(struct so_io_stream_checksum_backend_private));
	self->checksums = calloc(nb_checksums, sizeof(struct st_checksum *));
	self->digests = so_value_new_hashtable2();
	self->nb_checksums = nb_checksums;
	self->computed = false;

	unsigned int i;
	struct so_value_iterator * iter = so_value_list_get_iterator(checksums);
	for (i = 0; i < nb_checksums && so_value_iterator_has_next(iter); i++) {
		struct so_value * chcksum = so_value_iterator_get_value(iter, false);

		struct so_checksum_driver * driver = so_checksum_get_driver(so_value_string_get(chcksum));
		self->checksums[i] = driver->new_checksum();
	}
	so_value_iterator_free(iter);

	struct so_io_stream_checksum_backend * backend = malloc(sizeof(struct so_io_stream_checksum_backend));
	backend->ops = &so_io_stream_checksum_backend_ops;
	backend->data = self;

	return backend;
}

static void so_io_stream_checksum_backend_reset(struct so_io_stream_checksum_backend * worker) {
	struct so_io_stream_checksum_backend_private * self = worker->data;

	unsigned int i;
	for (i = 0; i < self->nb_checksums; i++) {
		struct so_checksum * ck = self->checksums[i];
		ck->ops->reset(ck);
	}
}

static void so_io_stream_checksum_backend_update(struct so_io_stream_checksum_backend * worker, const void * buffer, ssize_t length) {
	struct so_io_stream_checksum_backend_private * self = worker->data;

	unsigned int i;
	for (i = 0; i < self->nb_checksums; i++) {
		struct so_checksum * ck = self->checksums[i];
		ck->ops->update(ck, buffer, length);
	}
}


static void * so_io_stream_checksum_threaded_backend_cast(struct so_io_linked_list_block * block, bool base_addr) {
	unsigned char * ptr = (unsigned char *) (block + 1);
	if (base_addr)
		return ptr;
	else
		return ptr + block->data_length;
}

static struct so_value * so_io_stream_checksum_threaded_backend_digest(struct so_io_stream_checksum_backend * worker) {
	struct so_io_stream_checksum_threaded_backend_private * self = worker->data;
	return self->backend->ops->digest(self->backend);
}

static void so_io_stream_checksum_threaded_backend_finish(struct so_io_stream_checksum_backend * worker) {
	struct so_io_stream_checksum_threaded_backend_private * self = worker->data;

	pthread_mutex_lock(&self->lock);
	if (self->first_block != NULL)
		pthread_cond_wait(&self->wait, &self->lock);

	self->stop = true;

	pthread_cond_signal(&self->wait);
	pthread_cond_wait(&self->wait, &self->lock);
	pthread_mutex_unlock(&self->lock);
}

static void so_io_stream_checksum_threaded_backend_free(struct so_io_stream_checksum_backend * worker) {
	struct so_io_stream_checksum_threaded_backend_private * self = worker->data;
	self->backend->ops->free(self->backend);

	while (self->free_block != NULL) {
		struct so_io_linked_list_block * ptr = self->free_block;
		self->free_block = ptr->next;

		munmap(ptr, ptr->block_length + sizeof(struct so_io_linked_list_block));
	}

	free(self);
	free(worker);
}

struct so_io_stream_checksum_backend * so_io_stream_checksum_threaded_backend_new(struct so_value * checksums) {
	struct sysinfo info;
	bzero(&info, sizeof(info));
	sysinfo(&info);

	struct so_io_stream_checksum_threaded_backend_private * self = malloc(sizeof(struct so_io_stream_checksum_threaded_backend_private));
	self->first_block = self->last_block = self->free_block = NULL;
	self->used = 0;
	self->limit = info.totalram >> 6;

	self->should_reset = false;
	self->stop = false;

	self->checksums = so_value_share(checksums);

	pthread_mutex_init(&self->lock, NULL);
	pthread_cond_init(&self->wait, NULL);

	self->backend = so_io_stream_checksum_backend_new(checksums);

	so_thread_pool_run2("checksum worker", so_io_stream_checksum_threaded_backend_work, self, 8);

	struct so_io_stream_checksum_backend * backend = malloc(sizeof(struct so_io_stream_checksum_backend));
	backend->ops = &so_io_stream_checksum_threaded_backend_ops;
	backend->data = self;

	return backend;
}

static void so_io_stream_checksum_threaded_backend_reset(struct so_io_stream_checksum_backend * worker) {
	struct so_io_stream_checksum_threaded_backend_private * self = worker->data;

	pthread_mutex_lock(&self->lock);

	if (self->stop) {
		self->stop = false;

		so_thread_pool_run2("checksum worker", so_io_stream_checksum_threaded_backend_work, self, 8);
	} else {
		self->should_reset = true;
	}

	pthread_cond_signal(&self->wait);
	pthread_mutex_unlock(&self->lock);
}

static struct so_io_linked_list_block * so_io_stream_checksum_threaded_backend_sort(struct so_io_linked_list_block * blocks, unsigned int nb_elts) {
	if (nb_elts < 2)
		return blocks;

	if (nb_elts == 2) {
		struct so_io_linked_list_block * next = blocks->next;
		if (blocks->block_length > next->block_length) {
			next->next = blocks;
			blocks->previous = next;
			next->previous = blocks->next = NULL;
			return next;
		}
		return blocks;
	}

	unsigned int i, middle = nb_elts >> 1;
	struct so_io_linked_list_block * list_b = blocks;
	for (i = 0; i < middle; i++)
		list_b = list_b->next;
	list_b->previous = list_b->previous->next = NULL;

	struct so_io_linked_list_block * list_a = so_io_stream_checksum_threaded_backend_sort(blocks, middle);
	list_b = so_io_stream_checksum_threaded_backend_sort(list_b, nb_elts - middle);

	struct so_io_linked_list_block * first = NULL, * last = NULL;
	while (list_a != NULL && list_b != NULL) {
		if (list_a->block_length <= list_b->block_length) {
			struct so_io_linked_list_block * end = list_a;
			while (end->next != NULL && end->next->block_length <= list_b->block_length)
				end = end->next;

			if (first == NULL)
				first = list_a;
			else {
				last->next = list_a;
				list_a->previous = last;
			}
			last = end;

			list_a = end->next;
			end->next = NULL;
			if (list_a != NULL)
				list_a->previous = NULL;
		} else {
			struct so_io_linked_list_block * end = list_b;
			while (end->next != NULL && end->next->block_length <= list_a->block_length)
				end = end->next;

			if (first == NULL)
				first = list_b;
			else {
				last->next = list_b;
				list_b->previous = last;
			}
			last = end;

			list_b = end->next;
			end->next = NULL;
			if (list_b != NULL)
				list_b->previous = NULL;
		}
	}

	if (list_a != NULL) {
		last->next = list_a;
		list_a->previous = last;
	} else {
		last->next = list_b;
		list_b->previous = last;
	}

	return first;
}

static void so_io_stream_checksum_threaded_backend_update(struct so_io_stream_checksum_backend * worker, const void * buffer, ssize_t length) {
	struct so_io_stream_checksum_threaded_backend_private * self = worker->data;

	pthread_mutex_lock(&self->lock);

	if (self->used > 0 && self->used + length > self->limit) {
		size_t limit = self->limit >> 1;
		while (self->used > 0 && self->used + length > limit)
			pthread_cond_wait(&self->wait, &self->lock);
	}

	struct so_io_linked_list_block * block = self->last_block;
	if (block != NULL) {
		if (block->block_length - block->data_length < length)
			block = NULL;
		else if (block == self->first_block)
			self->first_block = self->last_block = NULL;
		else {
			self->last_block = block->previous;
			self->last_block->next = NULL;
			block->previous = NULL;
		}
	}

	if (block == NULL) {
		for (block = self->free_block; block != NULL; block = block->next)
			if (block->block_length >= length) {
				if (block == self->free_block) {
					self->free_block = block->next;
					block->next = NULL;
					if (self->free_block != NULL)
						self->free_block->previous = NULL;
				} else {
					block->previous->next = block->next;
					if (block->next != NULL)
						block->next->previous = block->previous;
					block->previous = block->next = NULL;
				}
				break;
			}

		pthread_mutex_unlock(&self->lock);

		if (block == NULL) {
			ssize_t block_size = 1048576;
			while (block_size < length)
				block_size += 1048576;

			block = mmap(NULL, block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			block->block_length = block_size - sizeof(struct so_io_linked_list_block);
			block->data_length = 0;
			block->previous = block->next = NULL;
		}
	} else
		pthread_mutex_unlock(&self->lock);

	memcpy(so_io_stream_checksum_threaded_backend_cast(block, false), buffer, length);
	block->data_length += length;

	pthread_mutex_lock(&self->lock);

	self->used += length;

	if (self->first_block == NULL)
		self->first_block = self->last_block = block;
	else {
		block->previous = self->last_block;
		self->last_block = self->last_block->next = block;
	}

	pthread_cond_signal(&self->wait);
	pthread_mutex_unlock(&self->lock);
}

static void so_io_stream_checksum_threaded_backend_work(void * arg) {
	struct so_io_stream_checksum_threaded_backend_private * self = arg;

	pthread_mutex_lock(&self->lock);

	for (;;) {
		if (self->stop && self->first_block == NULL)
			break;

		pthread_cond_signal(&self->wait);

		if (self->first_block == NULL && self->should_reset) {
			self->backend->ops->reset(self->backend);
			self->should_reset = false;
		}

		if (self->first_block == NULL)
			pthread_cond_wait(&self->wait, &self->lock);

		struct so_io_linked_list_block * blocks = self->first_block;
		if (blocks == NULL)
			continue;

		self->first_block = self->last_block = NULL;

		pthread_mutex_unlock(&self->lock);

		struct so_io_linked_list_block * ptr;
		unsigned int nb_blocks;
		for (ptr = blocks, nb_blocks = 0; ptr != NULL; ptr = ptr->next, nb_blocks++)
			self->backend->ops->update(self->backend, so_io_stream_checksum_threaded_backend_cast(ptr, true), ptr->data_length);

		// sort blocks
		if (nb_blocks > 1)
			blocks = so_io_stream_checksum_threaded_backend_sort(blocks, nb_blocks);

		pthread_mutex_lock(&self->lock);

		for (ptr = blocks; ptr != NULL; ptr = ptr->next) {
			self->used -= ptr->data_length;
			ptr->data_length = 0;
		}

		if (self->free_block == NULL)
			self->free_block = blocks;
		else
			for (ptr = self->free_block; blocks != NULL; ptr = ptr->next) {
				if (blocks->data_length <= ptr->data_length) {
					struct so_io_linked_list_block * end = blocks;
					while (end->next != NULL && end->next->data_length <= ptr->data_length)
						end = end->next;

					struct so_io_linked_list_block * begin = blocks;
					blocks = end->next;

					end->next = ptr;
					begin->previous = ptr->previous;
					if (begin->previous != NULL)
						begin->previous->next = begin;
					ptr->previous = end;

					if (ptr == self->free_block)
						self->free_block = begin;
					if (blocks != NULL)
						blocks->previous = NULL;
				} else if (ptr->next == NULL) {
					ptr->next = blocks;
					blocks->previous = ptr;
					break;
				}
			}
	}

	self->backend->ops->finish(self->backend);

	pthread_cond_signal(&self->wait);
	pthread_mutex_unlock(&self->lock);
}
