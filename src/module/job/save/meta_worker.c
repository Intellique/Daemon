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
*  Last modified: Sat, 08 Dec 2012 20:57:19 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// scandir
#include <dirent.h>
// pthread_cond_destroy, pthread_cond_init, pthread_cond_signal, pthread_cond_wait
// pthread_mutex_destroy, pthread_mutex_init, pthread_mutex_lock, pthread_mutex_lock
#include <pthread.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// lstat
#include <sys/stat.h>
// lstat
#include <sys/types.h>
// access, lstat
#include <unistd.h>

#include <libstone/thread_pool.h>
#include <libstone/util/file.h>

#include "save.h"

struct st_job_save_meta_worker_private {
	struct st_linked_list_file {
		const struct st_job_selected_path * selected_path;
		char * file;
		struct st_linked_list_file * next;
	} * volatile first_file, * volatile last_file;

	volatile bool stop;

	pthread_mutex_t lock;
	pthread_cond_t wait;
};

static void st_job_save_meta_worker_add_file(struct st_job_save_meta_worker * worker, const struct st_job_selected_path * selected_path, const char * path);
static void st_job_save_meta_worker_free(struct st_job_save_meta_worker * worker);
static void st_job_save_meta_worker_wait(struct st_job_save_meta_worker * worker);
static void st_job_save_meta_worker_work(void * arg);

static struct st_job_save_meta_worker_ops st_job_save_meta_worker_ops = {
	.add_file = st_job_save_meta_worker_add_file,
	.free     = st_job_save_meta_worker_free,
	.wait     = st_job_save_meta_worker_wait,
};


ssize_t st_job_save_compute_size(const char * path) {
	if (path == NULL)
		return 0;

	struct stat st;
	if (lstat(path, &st))
		return 0;

	if (S_ISREG(st.st_mode))
		return st.st_size;

	if (S_ISDIR(st.st_mode)) {
		if (access(path, R_OK | X_OK))
			return 0;

		struct dirent ** dl = 0;
		int nb_files = scandir(path, &dl, st_util_file_basic_scandir_filter, 0);

		int i;
		ssize_t total = 0;
		for (i = 0; i < nb_files; i++) {
			char * subpath = 0;
			asprintf(&subpath, "%s/%s", path, dl[i]->d_name);

			total = st_job_save_compute_size(subpath);

			free(subpath);
			free(dl[i]);
		}
		free(dl);

		return total;
	}

	return 0;
}


static void st_job_save_meta_worker_add_file(struct st_job_save_meta_worker * worker, const struct st_job_selected_path * selected_path, const char * path) {
	struct st_job_save_meta_worker_private * self = worker->data;

	struct st_linked_list_file * next = malloc(sizeof(struct st_linked_list_file));
	next->selected_path = selected_path;
	next->file = strdup(path);
	next->next = NULL;

	pthread_mutex_lock(&self->lock);
	if (self->first_file == NULL)
		self->first_file = self->last_file = next;
	else
		self->last_file = self->last_file->next = next;
	pthread_cond_signal(&self->wait);
	pthread_mutex_unlock(&self->lock);
}

static void st_job_save_meta_worker_free(struct st_job_save_meta_worker * worker) {
	struct st_job_save_meta_worker_private * self = worker->data;

	pthread_mutex_destroy(&self->lock);
	pthread_cond_destroy(&self->wait);

	free(self);
	free(worker);
}

struct st_job_save_meta_worker * st_job_save_meta_worker_new(struct st_job * job) {
	struct st_job_save_meta_worker_private * self = malloc(sizeof(struct st_job_save_meta_worker_private));
	self->first_file = self->last_file = NULL;

	self->stop = false;

	pthread_mutex_init(&self->lock, NULL);
	pthread_cond_init(&self->wait, NULL);

	st_thread_pool_run2(st_job_save_meta_worker_work, self, 8);

	struct st_job_save_meta_worker * meta = malloc(sizeof(struct st_job_save_meta_worker));
	meta->ops = &st_job_save_meta_worker_ops;
	meta->data = self;

	return meta;
}

static void st_job_save_meta_worker_wait(struct st_job_save_meta_worker * worker) {
	struct st_job_save_meta_worker_private * self = worker->data;

	pthread_mutex_lock(&self->lock);
	self->stop = true;
	pthread_cond_signal(&self->wait);
	pthread_cond_wait(&self->wait, &self->lock);
	pthread_mutex_unlock(&self->lock);
}

static void st_job_save_meta_worker_work(void * arg) {
	struct st_job_save_meta_worker_private * self = arg;

	for (;;) {
		pthread_mutex_lock(&self->lock);
		if (self->stop && self->first_file == NULL)
			break;
		if (self->first_file == NULL)
			pthread_cond_wait(&self->wait, &self->lock);

		struct st_linked_list_file * files = self->first_file;
		self->first_file = self->last_file = NULL;
		pthread_mutex_unlock(&self->lock);

		while (files != NULL) {
			// compute hash of files->file

			struct st_linked_list_file * next = files->next;
			free(files->file);
			free(files);

			files = next;
		}
	}

	pthread_cond_signal(&self->wait);
	pthread_mutex_unlock(&self->lock);
}

