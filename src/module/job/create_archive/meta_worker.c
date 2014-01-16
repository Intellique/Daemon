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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Thu, 16 Jan 2014 16:39:39 +0100                            *
\****************************************************************************/

// asprintf
#define _GNU_SOURCE
// magic_file, magic_open
#include <magic.h>
// open
#include <fcntl.h>
// pthread_cond_destroy, pthread_cond_init, pthread_cond_signal, pthread_cond_wait
// pthread_mutex_destroy, pthread_mutex_init, pthread_mutex_lock, pthread_mutex_lock
#include <pthread.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// lstat, open
#include <sys/stat.h>
// lstat, open
#include <sys/types.h>
// lstat
#include <unistd.h>

#include <libstone/io.h>
#include <libstone/library/archive.h>
#include <libstone/library/media.h>
#include <libstone/log.h>
#include <libstone/thread_pool.h>
#include <libstone/util/file.h>
#include <libstone/util/hashtable.h>
#include <libstone/util/string.h>

#include "create_archive.h"

struct st_job_create_archive_meta_worker_private {
	struct st_linked_list_file {
		struct st_job_selected_path * selected_path;
		char * file;
		struct st_pool * pool;
		struct st_linked_list_file * next;
	} * volatile first_file, * volatile last_file;

	struct st_job * job;
	struct st_database_connection * connect;

	volatile bool stop;

	pthread_mutex_t lock;
	pthread_cond_t wait;

	struct st_hashtable * meta_files;

	magic_t file;
};

static void st_job_create_archive_free_meta_file(void * key, void * val);

static void st_job_create_archive_meta_worker_add_file(struct st_job_create_archive_meta_worker * worker, struct st_job_selected_path * selected_path, const char * path, struct st_pool * pool);
static void st_job_create_archive_meta_worker_free(struct st_job_create_archive_meta_worker * worker);
static void st_job_create_archive_meta_worker_wait(struct st_job_create_archive_meta_worker * worker, bool stop);
static void st_job_create_archive_meta_worker_work(void * arg);
static void st_job_create_archive_meta_worker_work2(struct st_job_create_archive_meta_worker_private * self, struct st_linked_list_file * file);

static struct st_job_create_archive_meta_worker_ops st_job_create_archive_meta_worker_ops = {
	.add_file = st_job_create_archive_meta_worker_add_file,
	.free     = st_job_create_archive_meta_worker_free,
	.wait     = st_job_create_archive_meta_worker_wait,
};


static void st_job_create_archive_free_meta_file(void * key, void * val) {
	free(key);
	st_archive_file_free(val);
}

static void st_job_create_archive_meta_worker_add_file(struct st_job_create_archive_meta_worker * worker, struct st_job_selected_path * selected_path, const char * path, struct st_pool * pool) {
	struct st_job_create_archive_meta_worker_private * self = worker->data;

	struct st_linked_list_file * next = malloc(sizeof(struct st_linked_list_file));
	next->selected_path = selected_path;
	next->file = strdup(path);
	next->pool = pool;
	next->next = NULL;

	pthread_mutex_lock(&self->lock);
	if (self->first_file == NULL)
		self->first_file = self->last_file = next;
	else
		self->last_file = self->last_file->next = next;
	pthread_cond_signal(&self->wait);
	pthread_mutex_unlock(&self->lock);
}

static void st_job_create_archive_meta_worker_free(struct st_job_create_archive_meta_worker * worker) {
	struct st_job_create_archive_meta_worker_private * self = worker->data;

	pthread_mutex_destroy(&self->lock);
	pthread_cond_destroy(&self->wait);

	st_hashtable_free(self->meta_files);

	magic_close(self->file);

	free(self);
	free(worker);
}

struct st_job_create_archive_meta_worker * st_job_create_archive_meta_worker_new(struct st_job * job, struct st_database_connection * connect) {
	struct st_job_create_archive_meta_worker_private * self = malloc(sizeof(struct st_job_create_archive_meta_worker_private));
	self->first_file = self->last_file = NULL;

	self->job = job;
	self->connect = connect;
	self->stop = false;

	pthread_mutex_init(&self->lock, NULL);
	pthread_cond_init(&self->wait, NULL);

	self->meta_files = st_hashtable_new2(st_util_string_compute_hash, st_job_create_archive_free_meta_file);

	self->file = magic_open(MAGIC_MIME_TYPE);
	magic_load(self->file, NULL);

	struct st_job_create_archive_meta_worker * meta = malloc(sizeof(struct st_job_create_archive_meta_worker));
	meta->ops = &st_job_create_archive_meta_worker_ops;
	meta->data = self;
	meta->meta_files = self->meta_files;

	char * th_name;
	asprintf(&th_name, "meta worker: %s", job->name);

	st_thread_pool_run2(th_name, st_job_create_archive_meta_worker_work, self, 8);

	free(th_name);

	return meta;
}

static void st_job_create_archive_meta_worker_wait(struct st_job_create_archive_meta_worker * worker, bool stop) {
	struct st_job_create_archive_meta_worker_private * self = worker->data;

	pthread_mutex_lock(&self->lock);
	self->stop = stop;
	pthread_cond_signal(&self->wait);
	pthread_cond_wait(&self->wait, &self->lock);
	pthread_mutex_unlock(&self->lock);
}

static void st_job_create_archive_meta_worker_work(void * arg) {
	struct st_job_create_archive_meta_worker_private * self = arg;

	for (;;) {
		pthread_mutex_lock(&self->lock);
		if (self->stop && self->first_file == NULL)
			break;

		if (self->first_file == NULL) {
			pthread_cond_signal(&self->wait);
			pthread_cond_wait(&self->wait, &self->lock);
		}

		struct st_linked_list_file * files = self->first_file;
		self->first_file = self->last_file = NULL;
		pthread_mutex_unlock(&self->lock);

		while (files != NULL) {
			st_job_create_archive_meta_worker_work2(self, files);

			struct st_linked_list_file * next = files->next;
			free(files->file);
			free(files);

			files = next;
		}
	}

	pthread_cond_signal(&self->wait);
	pthread_mutex_unlock(&self->lock);
}

static void st_job_create_archive_meta_worker_work2(struct st_job_create_archive_meta_worker_private * self, struct st_linked_list_file * f) {
	struct stat st;
	if (lstat(f->file, &st))
		return;

	struct st_archive_file * file = st_archive_file_new(&st, f->file);
	file->selected_path = f->selected_path;

	const char * mime_type = magic_file(self->file, f->file);

	if (mime_type == NULL) {
		file->mime_type = strdup("");
		st_job_add_record(self->connect, st_log_level_info, self->job, st_job_record_notif_normal, "File (%s) has not mime type", f->file);
	} else {
		file->mime_type = strdup(mime_type);
		st_job_add_record(self->connect, st_log_level_info, self->job, st_job_record_notif_normal, "Mime type of file '%s' is '%s'", f->file, mime_type);
	}

	if (S_ISREG(st.st_mode)) {
		unsigned int nb_checksums;
		char ** checksums = self->connect->ops->get_checksums_by_pool(self->connect, f->pool, &nb_checksums);

		int fd = open(f->file, O_RDONLY);
		struct st_stream_writer * writer = st_checksum_writer_new(NULL, checksums, nb_checksums, false);

		char buffer[4096];
		ssize_t nb_read;

		while (nb_read = read(fd, buffer, 4096), nb_read > 0)
			writer->ops->write(writer, buffer, nb_read);

		close(fd);
		writer->ops->close(writer);

		file->digests = st_checksum_writer_get_checksums(writer);

		writer->ops->free(writer);

		unsigned int i;
		for (i = 0; i < nb_checksums; i++)
			free(checksums[i]);
		free(checksums);
	}

	char * hash;
	asprintf(&hash, "%s:%s", f->pool->uuid, f->file);
	st_hashtable_put(self->meta_files, hash, st_hashtable_val_custom(file));
}

