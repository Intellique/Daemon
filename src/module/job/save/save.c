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
*  Last modified: Sun, 04 Nov 2012 18:47:23 +0100                         *
\*************************************************************************/

// free, malloc
#include <stdlib.h>

#include <libstone/library/media.h>
#include <libstone/log.h>
#include <libstone/user.h>
#include <libstone/util/string.h>

#include "save.h"

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
		for (i = 0; i < self->nb_selected_paths; i++)
			free(self->selected_paths[i]);
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

	self->data_worker = NULL;

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

	struct st_pool * pool = st_pool_get_by_job(job, self->connect);
	if (pool == NULL) {
		pool = job->user->pool;
		st_job_add_record(self->connect, st_log_level_info, job, "Using pool (%s) of user (%s)", pool->name, job->user->login);
	}

	self->selected_paths = self->connect->ops->get_selected_paths(self->connect, job, &self->nb_selected_paths);

	ssize_t archive_size = 0;
	unsigned int i;
	for (i = 0; i < self->nb_selected_paths; i++) {
		// remove double '/'
		st_util_string_delete_double_char(self->selected_paths[i], '/');
		// remove trailing '/'
		st_util_string_rtrim(self->selected_paths[i], '/');

		archive_size += st_job_save_compute_size(self->selected_paths[i]);
	}

	return 0;
}

