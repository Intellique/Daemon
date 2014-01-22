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
*  Last modified: Thu, 16 Jan 2014 16:39:09 +0100                            *
\****************************************************************************/

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
// time
#include <time.h>
// access, lstat
#include <unistd.h>

#include <libstone/format.h>
#include <libstone/host.h>
#include <libstone/io.h>
#include <libstone/library/media.h>
#include <libstone/log.h>
#include <libstone/script.h>
#include <libstone/user.h>
#include <libstone/util/file.h>
#include <libstone/util/hashtable.h>
#include <libstone/util/string.h>

#include <libjob-create-archive.chcksum>

#include "create_archive.h"

static int st_job_create_archive_archive(struct st_job * job, struct st_job_selected_path * selected_path, const char * path);
static bool st_job_create_archive_check(struct st_job * job);
static void st_job_create_archive_free(struct st_job * job);
static void st_job_create_archive_init(void) __attribute__((constructor));
static void st_job_create_archive_new_job(struct st_job * job, struct st_database_connection * db);
static void st_job_create_archive_on_error(struct st_job * job);
static void st_job_create_archive_post_run(struct st_job * job);
static bool st_job_create_archive_pre_run(struct st_job * j);
static int st_job_create_archive_run(struct st_job * job);

static struct st_job_ops st_job_create_archive_ops = {
	.check    = st_job_create_archive_check,
	.free     = st_job_create_archive_free,
	.on_error = st_job_create_archive_on_error,
	.post_run = st_job_create_archive_post_run,
	.pre_run  = st_job_create_archive_pre_run,
	.run      = st_job_create_archive_run,
};

static struct st_job_driver st_job_create_archive_driver = {
	.name        = "create-archive",
	.new_job     = st_job_create_archive_new_job,
	.cookie      = NULL,
	.api_level   = {
		.checksum = 0,
		.database = 0,
		.job      = STONE_JOB_API_LEVEL,
	},
	.src_checksum   = STONE_JOB_CREATEARCHIVE_SRCSUM,
};


static int st_job_create_archive_archive(struct st_job * job, struct st_job_selected_path * selected_path, const char * path) {
	struct st_job_create_archive_private * self = job->data;

	if (!st_util_string_check_valid_utf8(path)) {
		char * fixed_path = strdup(path);
		st_util_string_fix_invalid_utf8(fixed_path);

		st_job_add_record(self->connect, st_log_level_warning, job, st_job_record_notif_normal, "Path '%s' contains invalid utf8 characters", fixed_path);

		free(fixed_path);
		return 1;
	}

	struct stat st;
	if (lstat(path, &st)) {
		st_job_add_record(self->connect, st_log_level_warning, job, st_job_record_notif_important, "Error while getting information about: %s", path);
		return 1;
	}

	if (S_ISSOCK(st.st_mode))
		return 0;

	int failed = self->worker->ops->add_file(self->worker, path);
	if (failed) {
		if (failed < 0)
			st_job_add_record(self->connect, st_log_level_warning, job, st_job_record_notif_important, "Error while adding file: '%s'", path);
		return failed;
	}

	self->meta->ops->add_file(self->meta, selected_path, path, self->pool);

	if (S_ISREG(st.st_mode)) {
		int fd = open(path, O_RDONLY);
		if (fd < 0) {
			st_job_add_record(self->connect, st_log_level_warning, job, st_job_record_notif_important, "Error while opening file: '%s' because %m", path);
			return -1;
		}

		char buffer[4096];

		ssize_t nb_read;
		while (nb_read = read(fd, buffer, 4096), nb_read > 0 && !failed) {
			ssize_t nb_write = self->worker->ops->write(self->worker, buffer, nb_read);

			if (nb_write < 0)
				failed = -2;

			float done = self->total_done += nb_write;
			job->done = 0.02 + done * 0.96 / self->total_size;
		}

		if (nb_read < 0) {
			st_job_add_record(self->connect, st_log_level_error, job, st_job_record_notif_important, "Unexpected error while reading from file '%s' because %m", path);
			failed = -1;
		}

		close(fd);

		if (!failed)
			self->worker->ops->end_file(self->worker);
	} else if (S_ISDIR(st.st_mode)) {
		if (access(path, F_OK | R_OK | X_OK)) {
			st_job_add_record(self->connect, st_log_level_warning, job, st_job_record_notif_important, "Can't access to directory %s", path);
			return 1;
		}

		struct dirent ** dl = NULL;
		int nb_files = scandir(path, &dl, st_util_file_basic_scandir_filter, versionsort);

		int i;
		for (i = 0; i < nb_files; i++) {
			if (failed >= 0 && job->db_status != st_job_status_stopped) {
				char * subpath = NULL;
				asprintf(&subpath, "%s/%s", path, dl[i]->d_name);

				failed = st_job_create_archive_archive(job, selected_path, subpath);

				free(subpath);
			}

			free(dl[i]);
		}

		free(dl);
	}

	return failed;
}

static bool st_job_create_archive_check(struct st_job * job) {
	job->sched_status = st_job_status_error;
	job->repetition = 0;
	return false;
}

static void st_job_create_archive_free(struct st_job * job) {
	struct st_job_create_archive_private * self = job->data;
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

	self->worker->ops->free(self->worker);
	self->meta->ops->free(self->meta);

	free(self);
	job->data = NULL;
}

static void st_job_create_archive_init(void) {
	st_job_register_driver(&st_job_create_archive_driver);
}

static void st_job_create_archive_new_job(struct st_job * job, struct st_database_connection * db) {
	struct st_job_create_archive_private * self = malloc(sizeof(struct st_job_create_archive_private));
	self->connect = db->config->ops->connect(db->config);

	self->nb_selected_paths = 0;
	self->selected_paths = self->connect->ops->get_selected_paths(self->connect, job, &self->nb_selected_paths);
	self->pool = st_pool_get_by_job(job, self->connect);

	self->total_done = self->total_size = 0;

	unsigned int i;
	for (i = 0; i < self->nb_selected_paths; i++) {
		struct st_job_selected_path * p = self->selected_paths + i;
		// remove double '/'
		st_util_string_delete_double_char(p->path, '/');
		// remove trailing '/'
		st_util_string_rtrim(p->path, '/');

		ssize_t size = st_format_get_size(p->path, true, self->pool->format);
		if (size >= 0)
			self->total_size += size;
		else
			st_job_add_record(self->connect, st_log_level_error, job, st_job_record_notif_important, "Error while computing size of '%s'", p->path);
	}

	self->worker = NULL;

	job->data = self;
	job->ops = &st_job_create_archive_ops;
}

static void st_job_create_archive_on_error(struct st_job * job) {
	struct st_job_create_archive_private * self = job->data;

	if (self->connect->ops->get_nb_scripts(self->connect, job->driver->name, st_script_type_on_error, self->pool) == 0)
		return;

	json_t * pool = json_object();
	json_object_set_new(pool, "uuid", json_string(self->pool->uuid));
	json_object_set_new(pool, "name", json_string(self->pool->name));

	json_t * archive = json_object();
	json_object_set_new(archive, "name", json_string(job->name));

	unsigned int i;
	json_t * paths = json_array();
	for (i = 0; i < self->nb_selected_paths; i++) {
		struct st_job_selected_path * p = self->selected_paths + i;
		json_array_append_new(paths, json_string(p->path));
	}
	json_object_set_new(archive, "paths", paths);
	json_object_set_new(archive, "pool", pool);

	if (job->meta != NULL) {
		json_error_t error;
		json_t * meta = json_loads(job->meta, 0, &error);
		if (meta != NULL)
			json_object_set_new(archive, "metadatas", meta);
		else
			json_object_set_new(archive, "metadatas", json_null());
	} else
		json_object_set_new(archive, "metadatas", json_null());

	json_t * sdata = json_object();
	json_object_set_new(sdata, "archive", archive);
	json_object_set_new(sdata, "host", st_host_get_info());
	json_object_set_new(sdata, "job", json_object());

	json_t * returned_data = st_script_run(self->connect, job, job->driver->name, st_script_type_on_error, self->pool, sdata);

	json_decref(returned_data);
	json_decref(sdata);
}

static void st_job_create_archive_post_run(struct st_job * job) {
	struct st_job_create_archive_private * self = job->data;

	if (self->connect->ops->get_nb_scripts(self->connect, job->driver->name, st_script_type_post, self->pool) == 0)
		return;

	json_t * data = self->worker->ops->post_run(self->worker);

	json_t * returned_data = st_script_run(self->connect, job, job->driver->name, st_script_type_post, self->pool, data);

	json_decref(returned_data);
	json_decref(data);
}

static bool st_job_create_archive_pre_run(struct st_job * job) {
	struct st_job_create_archive_private * self = job->data;

	if (self->connect->ops->get_nb_scripts(self->connect, job->driver->name, st_script_type_pre, self->pool) == 0)
		return true;

	json_t * pool = json_object();
	json_object_set_new(pool, "uuid", json_string(self->pool->uuid));
	json_object_set_new(pool, "name", json_string(self->pool->name));

	json_t * archive = json_object();
	json_object_set_new(archive, "name", json_string(job->name));

	unsigned int i;
	json_t * paths = json_array();
	for (i = 0; i < self->nb_selected_paths; i++) {
		struct st_job_selected_path * p = self->selected_paths + i;
		json_array_append_new(paths, json_string(p->path));
	}
	json_object_set_new(archive, "paths", paths);
	json_object_set_new(archive, "pool", pool);

	if (job->meta != NULL) {
		json_error_t error;
		json_t * meta = json_loads(job->meta, 0, &error);
		if (meta != NULL)
			json_object_set_new(archive, "metadatas", meta);
		else
			json_object_set_new(archive, "metadatas", json_null());
	} else
		json_object_set_new(archive, "metadatas", json_null());

	json_t * sdata = json_object();
	json_object_set_new(sdata, "archive", archive);
	json_object_set_new(sdata, "host", st_host_get_info());
	json_object_set_new(sdata, "job", json_object());

	json_t * returned_data = st_script_run(self->connect, job, job->driver->name, st_script_type_pre, self->pool, sdata);
	bool sr = st_io_json_should_run(returned_data);

	json_decref(returned_data);
	json_decref(sdata);

	return sr;
}

static int st_job_create_archive_run(struct st_job * job) {
	struct st_job_create_archive_private * self = job->data;

	st_job_add_record(self->connect, st_log_level_info, job, st_job_record_notif_important, "Start archive job (named: %s), num runs %ld", job->name, job->num_runs);

	if (!job->user->can_archive) {
		st_job_add_record(self->connect, st_log_level_error, job, st_job_record_notif_important, "Error, user (%s) cannot create archive", job->user->login);
		return 1;
	}

	char bufsize[32];
	st_util_file_convert_size_to_string(self->total_size, bufsize, 32);
	st_job_add_record(self->connect, st_log_level_info, job, st_job_record_notif_normal, "Will archive %s", bufsize);

	if (job->db_status == st_job_status_stopped) {
		st_job_add_record(self->connect, st_log_level_warning, job, st_job_record_notif_important, "Job stopped by user request");
		return 0;
	}

	self->meta = st_job_create_archive_meta_worker_new(job, self->connect);
	self->worker = st_job_create_archive_single_worker(job, self->total_size, self->connect, self->meta);

	bool ok = false;
	if (job->db_status != st_job_status_stopped) {
		job->done = 0.01;
		ok = self->worker->ops->load_media(self->worker);
	}

	int failed = 0;
	if (job->db_status != st_job_status_stopped && ok) {
		job->done = 0.02;

		unsigned int i;
		for (i = 0; i < self->nb_selected_paths && job->db_status != st_job_status_stopped && failed >= 0; i++) {
			struct st_job_selected_path * p = self->selected_paths + i;
			st_job_add_record(self->connect, st_log_level_info, job, st_job_record_notif_normal, "Archiving file: %s", p->path);
			failed = st_job_create_archive_archive(job, p, p->path);
		}

		bool stopped = job->db_status == st_job_status_stopped;

		self->worker->ops->close(self->worker);
		self->meta->ops->wait(self->meta, true);

		job->done = 0.98;
		st_job_add_record(self->connect, st_log_level_info, job, st_job_record_notif_normal, "Synchronize database");
		self->worker->ops->sync_db(self->worker);

		job->done = 0.99;
		st_job_add_record(self->connect, st_log_level_info, job, st_job_record_notif_normal, "Writing index file");
		self->worker->ops->write_meta(self->worker);

		if (st_hashtable_has_key(job->option, "check_archive")) {
			bool check_archive = false, quick_mode = false;
			struct st_hashtable_value ca = st_hashtable_get(job->option, "check_archive");

			if (st_hashtable_val_can_convert(&ca, st_hashtable_value_boolean))
				check_archive = st_hashtable_val_convert_to_bool(&ca);
			else if (st_hashtable_val_can_convert(&ca, st_hashtable_value_signed_integer))
				check_archive = st_hashtable_val_convert_to_signed_integer(&ca) != 0;

			if (st_hashtable_has_key(job->option, "check_archive_mode")) {
				ca = st_hashtable_get(job->option, "check_archive_mode");

				if (st_hashtable_val_can_convert(&ca, st_hashtable_value_boolean))
					quick_mode = st_hashtable_val_convert_to_bool(&ca);
				else if (st_hashtable_val_can_convert(&ca, st_hashtable_value_signed_integer))
					quick_mode = st_hashtable_val_convert_to_signed_integer(&ca) != 0;
			}

			if (check_archive) {
				time_t start_check_archive = time(NULL);
				self->worker->ops->schedule_check_archive(self->worker, start_check_archive, quick_mode);
			}
		} else
			self->worker->ops->schedule_auto_check_archive(self->worker);

		if (stopped)
			st_job_add_record(self->connect, st_log_level_warning, job, st_job_record_notif_important, "Job stopped by user request");

		job->done = 1;
		st_job_add_record(self->connect, st_log_level_info, job, st_job_record_notif_important, "Archive job (named: %s) finished", job->name);
	} else {
		self->worker->ops->close(self->worker);
		self->meta->ops->wait(self->meta, true);

		st_job_add_record(self->connect, st_log_level_warning, job, st_job_record_notif_normal, "Job stopped by user request");
		st_job_add_record(self->connect, st_log_level_info, job, st_job_record_notif_important, "Archive job (named: %s) finished with status 'stopped'", job->name);
	}

	return failed;
}

