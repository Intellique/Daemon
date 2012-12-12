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
*  Last modified: Wed, 12 Dec 2012 15:28:35 +0100                         *
\*************************************************************************/

// asprintf, versionsort
#define _GNU_SOURCE
// scandir
#include <dirent.h>
// open
#include <fcntl.h>
// free, malloc
#include <stdlib.h>
// asprintf
#include <stdio.h>
// strdup
#include <string.h>
// lstat, open
#include <sys/stat.h>
// lstat, open
#include <sys/types.h>
// access, lstat
#include <unistd.h>

#include <libstone/library/media.h>
#include <libstone/log.h>
#include <libstone/user.h>
#include <libstone/util/file.h>
#include <libstone/util/string.h>

#include "save.h"

static int st_job_save_archive(struct st_job * job, struct st_job_selected_path * selected_path, const char * path);
static bool st_job_save_check(struct st_job * job);
static void st_job_save_free(struct st_job * job);
static void st_job_save_init(void) __attribute__((constructor));
static void st_job_save_new_job(struct st_job * job, struct st_database_connection * db);
static int st_job_save_run(struct st_job * job);

static struct st_job_ops st_job_save_ops = {
	.check = st_job_save_check,
	.free  = st_job_save_free,
	.run   = st_job_save_run,
};

static struct st_job_driver st_job_save_driver = {
	.name        = "save",
	.new_job     = st_job_save_new_job,
	.cookie      = NULL,
	.api_level   = {
		.checksum = 0,
		.database = 0,
		.job      = STONE_JOB_API_LEVEL,
	},
};


static int st_job_save_archive(struct st_job * job, struct st_job_selected_path * selected_path, const char * path) {
	struct st_job_save_private * self = job->data;

	if (!st_util_string_check_valid_utf8(path)) {
		char * fixed_path = strdup(path);
		st_util_string_fix_invalid_utf8(fixed_path);

		st_job_add_record(self->connect, st_log_level_warning, job, "Path '%s' contains invalid utf8 characters", fixed_path);

		free(fixed_path);
		return 0;
	}

	struct stat st;
	if (lstat(path, &st)) {
		st_job_add_record(self->connect, st_log_level_warning, job, "Error while getting information about: %s", path);
		return 0;
	}

	if (S_ISSOCK(st.st_mode))
		return 0;

	self->worker->ops->add_file(self->worker, path);
	self->meta->ops->add_file(self->meta, selected_path, path);

	if (S_ISREG(st.st_mode)) {
		int fd = open(path, O_RDONLY);
		if (fd < 0) {
			st_job_add_record(self->connect, st_log_level_warning, job, "Error while opening file: '%s' because %m", path);
			return 0;
		}

		char buffer[4096];

		ssize_t nb_read = read(fd, buffer, 4096);

		while (nb_read > 0) {
			ssize_t nb_write = self->worker->ops->write(self->worker, buffer, nb_read);

			self->total_done += nb_write;
			job->done = ((float) self->total_done) / self->total_size;

			nb_read = read(fd, buffer, 4096);
		}

		if (nb_read < 0) {
			st_job_add_record(self->connect, st_log_level_error, job, "Unexpected error while reading from file '%s' because %m", path);
		}

		close(fd);

		self->worker->ops->end_file(self->worker);
	} else if (S_ISDIR(st.st_mode)) {
		if (access(path, F_OK | R_OK | X_OK)) {
			st_job_add_record(self->connect, st_log_level_warning, job, "Can't access to directory %s", path);
			return 0;
		}

		struct dirent ** dl = NULL;
		int nb_files = scandir(path, &dl, st_util_file_basic_scandir_filter, versionsort);

		int i, failed = 0;
		for (i = 0; i < nb_files; i++) {
			if (!failed) {
				char * subpath = NULL;
				asprintf(&subpath, "%s/%s", path, dl[i]->d_name);

				failed = st_job_save_archive(job, selected_path, subpath);

				free(subpath);
			}

			free(dl[i]);
		}

		free(dl);

		return failed;
	}

	return 0;
}

static bool st_job_save_check(struct st_job * job __attribute__((unused))) {
	return true;
}

static void st_job_save_free(struct st_job * job) {
	struct st_job_save_private * self = job->data;
	if (self != NULL) {
		self->connect->ops->close(self->connect);
		self->connect->ops->free(self->connect);
	}

	if (self->nb_selected_paths > 0) {
		unsigned int i;
		for (i = 0; i < self->nb_selected_paths; i++) {
			struct st_job_selected_path * p = self->selected_paths + i;
			free(p->path);
			free(p->db_data);
		}
		free(self->selected_paths);
		self->selected_paths = NULL;
		self->nb_selected_paths = 0;
	}

	free(self);
	job->data = NULL;
}

static void st_job_save_init(void) {
	st_job_register_driver(&st_job_save_driver);
}

static void st_job_save_new_job(struct st_job * job, struct st_database_connection * db) {
	struct st_job_save_private * self = malloc(sizeof(struct st_job_save_private));
	self->connect = db->config->ops->connect(db->config);

	self->selected_paths = NULL;
	self->nb_selected_paths = 0;

	self->total_done = self->total_size = 0;

	self->worker = NULL;

	job->data = self;
	job->ops = &st_job_save_ops;
}

static int st_job_save_run(struct st_job * job) {
	struct st_job_save_private * self = job->data;

	st_job_add_record(self->connect, st_log_level_info, job, "Start archive job (job name: %s), num runs %ld", job->name, job->num_runs);

	if (!job->user->can_archive) {
		st_job_add_record(self->connect, st_log_level_error, job, "Error, user (%s) cannot create archive", job->user->login);
		return 1;
	}

	self->selected_paths = self->connect->ops->get_selected_paths(self->connect, job, &self->nb_selected_paths);

	unsigned int i;
	for (i = 0; i < self->nb_selected_paths; i++) {
		struct st_job_selected_path * p = self->selected_paths + i;
		// remove double '/'
		st_util_string_delete_double_char(p->path, '/');
		// remove trailing '/'
		st_util_string_rtrim(p->path, '/');

		self->total_size += st_job_save_compute_size(p->path);
	}

	self->meta = st_job_save_meta_worker_new(job, self->connect);

	self->worker = st_job_save_single_worker(job, self->total_size, self->connect, self->meta);
	self->worker->ops->load_media(self->worker);

	for (i = 0; i < self->nb_selected_paths; i++) {
		struct st_job_selected_path * p = self->selected_paths + i;
		st_job_save_archive(job, p, p->path);
	}

	self->worker->ops->close(self->worker);
	self->meta->ops->wait(self->meta, true);

	self->worker->ops->sync_db(self->worker);

	self->worker->ops->free(self->worker);
	self->meta->ops->free(self->meta);

	return 0;
}

