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
*  Last modified: Fri, 17 Oct 2014 12:22:03 +0200                            *
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
#include <libstone/library/archive.h>
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
static void st_job_create_archive_new_job(struct st_job * job);
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

		st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_normal, "Path '%s' contains invalid utf8 characters", fixed_path);

		free(fixed_path);
		return 1;
	}

	struct stat st;
	if (lstat(path, &st)) {
		st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_important, "Error while getting information about: %s", path);
		return 1;
	}

	if (S_ISSOCK(st.st_mode))
		return 0;

	int failed = self->worker->ops->add_file(self->worker, path);
	if (failed) {
		if (failed < 0)
			st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_important, "Error while adding file: '%s'", path);
		return failed;
	}

	self->meta->ops->add_file(self->meta, selected_path, path, self->pool);

	if (S_ISREG(st.st_mode)) {
		int fd = open(path, O_RDONLY);
		if (fd < 0) {
			st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_important, "Error while opening file: '%s' because %m", path);
			return -1;
		}

		char buffer[4096];

		ssize_t nb_read;
		while (nb_read = read(fd, buffer, 4096), nb_read > 0 && !failed) {
			ssize_t nb_write = self->worker->ops->write(self->worker, buffer, nb_read);

			if (nb_write < 0)
				failed = -2;

			float done = self->total_done += nb_write;
			job->done = 0.02 + done * 0.95 / self->total_size;
		}

		if (nb_read < 0) {
			st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Unexpected error while reading from file '%s' because %m", path);
			failed = -1;
		}

		close(fd);

		if (!failed)
			self->worker->ops->end_file(self->worker);
	} else if (S_ISDIR(st.st_mode)) {
		if (access(path, F_OK | R_OK | X_OK)) {
			st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_important, "Can't access to directory %s", path);
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
		job->db_connect->ops->close(job->db_connect);
		job->db_connect->ops->free(job->db_connect);
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

	st_archive_free(self->archive);
	if (self->worker != NULL)
		self->worker->ops->free(self->worker);
	if (self->meta != NULL)
		self->meta->ops->free(self->meta);

	free(self);
	job->data = NULL;
}

static void st_job_create_archive_init(void) {
	st_job_register_driver(&st_job_create_archive_driver);
}

static void st_job_create_archive_new_job(struct st_job * job) {
	struct st_job_create_archive_private * self = malloc(sizeof(struct st_job_create_archive_private));
	job->db_connect = job->db_config->ops->connect(job->db_config);

	self->nb_selected_paths = 0;
	self->selected_paths = job->db_connect->ops->get_selected_paths(job->db_connect, job, &self->nb_selected_paths);
	self->pool = st_pool_get_by_job(job, job->db_connect);

	self->total_done = self->total_size = 0;

	self->archive = job->db_connect->ops->get_archive_volumes_by_job(job->db_connect, job);
	self->archive_size = 0;
	self->nb_files = 0;

	if (self->pool == NULL)
		self->pool = st_pool_get_by_archive(self->archive, job->db_connect);

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
			st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Error while computing size of '%s'", p->path);
	}

	if (self->archive == NULL) {
		self->archive = st_archive_new(job->name, job->user);
		self->archive->metadatas = strdup(job->meta);
	}

	for (i = 0; i < self->archive->nb_volumes; i++) {
		struct st_archive_volume * vol = self->archive->volumes + i;
		job->db_connect->ops->get_archive_files_by_job_and_archive_volume(job->db_connect, job, vol);
		self->archive_size += vol->size;
		self->nb_files += vol->nb_files;

		unsigned int j;
		for (j = 0; j < vol->nb_files; j++) {
			struct st_archive_files * ff = vol->files + j;
			struct st_archive_file * file = ff->file;
			job->db_connect->ops->get_checksums_of_file(job->db_connect, file);
		}
	}

	self->worker = NULL;

	job->data = self;
	job->ops = &st_job_create_archive_ops;
}

static void st_job_create_archive_on_error(struct st_job * job) {
	struct st_job_create_archive_private * self = job->data;

	if (job->db_connect->ops->get_nb_scripts(job->db_connect, job->driver->name, st_script_type_on_error, self->pool) == 0)
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

	json_t * returned_data = st_script_run(job->db_connect, job, job->driver->name, st_script_type_on_error, self->pool, sdata);

	json_decref(returned_data);
	json_decref(sdata);
}

static void st_job_create_archive_post_run(struct st_job * job) {
	struct st_job_create_archive_private * self = job->data;

	if (job->db_connect->ops->get_nb_scripts(job->db_connect, job->driver->name, st_script_type_post, self->pool) == 0) {
		job->done = 1;
		return;
	}

	json_t * data = self->worker->ops->post_run(self->worker);

	json_t * returned_data = st_script_run(job->db_connect, job, job->driver->name, st_script_type_post, self->pool, data);

	json_decref(returned_data);
	json_decref(data);

	job->done = 1;
}

static bool st_job_create_archive_pre_run(struct st_job * job) {
	struct st_job_create_archive_private * self = job->data;

	if (job->db_connect->ops->get_nb_scripts(job->db_connect, job->driver->name, st_script_type_pre, self->pool) == 0)
		return true;

	struct st_archive * archive = self->archive;
	json_t * jarchive = json_object();
	json_object_set_new(jarchive, "name", json_string(archive->name));
	json_object_set_new(jarchive, "uuid", json_string(archive->uuid));
	json_object_set_new(jarchive, "size", json_integer(self->archive_size));

	unsigned int i;
	json_t * paths = json_array();
	for (i = 0; i < self->nb_selected_paths; i++) {
		struct st_job_selected_path * p = self->selected_paths + i;
		ssize_t size = st_format_get_size(p->path, true, self->pool->format);

		json_t * path = json_object();
		json_object_set_new(path, "name", json_string(p->path));
		json_object_set_new(path, "size", json_integer(size));

		json_array_append_new(paths, path);
	}
	json_object_set_new(jarchive, "paths", paths);

	json_t * pool = json_object();
	json_object_set_new(pool, "uuid", json_string(self->pool->uuid));
	json_object_set_new(pool, "name", json_string(self->pool->name));
	json_object_set_new(jarchive, "pool", pool);

	if (job->meta != NULL) {
		json_error_t error;
		json_t * meta = json_loads(job->meta, 0, &error);

		if (meta != NULL)
			json_object_set_new(jarchive, "metadatas", meta);
		else
			json_object_set_new(jarchive, "metadatas", json_null());
	} else
		json_object_set_new(jarchive, "metadatas", json_null());

	json_t * volumes = json_array();
	unsigned int nb_files = 0;
	char * last_file = NULL;
	for (i = 0; i < archive->nb_volumes; i++) {
		json_t * volume = json_object();
		struct st_archive_volume * vol = archive->volumes + i;

		json_object_set_new(volume, "position", json_integer(i));
		json_object_set_new(volume, "size", json_integer(vol->size));
		json_object_set_new(volume, "start time", json_integer(vol->start_time));
		json_object_set_new(volume, "end time", json_integer(vol->end_time));

		json_t * checksum = json_object();
		unsigned int nb_digests, j;
		const void ** checksums = st_hashtable_keys(vol->digests, &nb_digests);
		for (j = 0; j < nb_digests; j++) {
			struct st_hashtable_value digest = st_hashtable_get(vol->digests, checksums[j]);
			json_object_set_new(checksum, checksums[j], json_string(digest.value.string));
		}
		free(checksums);
		json_object_set_new(volume, "checksums", checksum);
		if (vol->check_time > 0) {
			json_object_set_new(volume, "checksum ok", vol->check_ok ? json_true() : json_false());
			json_object_set_new(volume, "check time", json_integer(vol->check_time));
		}

		json_t * jfiles = json_array();
		for (j = 0; j < vol->nb_files; j++) {
			struct st_archive_files * files = vol->files + j;
			struct st_archive_file * file = files->file;
			json_t * jfile = json_object();

			json_object_set_new(jfile, "path", json_string(file->name));
			json_object_set_new(jfile, "type", json_string(st_archive_file_type_to_string(file->type)));
			json_object_set_new(jfile, "mime type", json_string(file->mime_type));

			json_object_set_new(jfile, "owner id", json_integer(file->ownerid));
			json_object_set_new(jfile, "owner", json_string(file->owner));
			json_object_set_new(jfile, "group id", json_integer(file->groupid));
			json_object_set_new(jfile, "group", json_string(file->group));

			char perm[10];
			st_util_file_convert_mode(perm, file->perm);
			json_object_set_new(jfile, "perm", json_string(perm));
			json_object_set_new(jfile, "size", json_integer(file->size));
			json_object_set_new(jfile, "block position", json_integer(files->position));

			unsigned int k;
			checksum = json_object();
			checksums = st_hashtable_keys(file->digests, &nb_digests);
			for (k = 0; k < nb_digests; k++) {
				struct st_hashtable_value digest = st_hashtable_get(file->digests, checksums[k]);
				json_object_set_new(checksum, checksums[k], json_string(digest.value.string));
			}
			free(checksums);
			json_object_set_new(jfile, "checksums", checksum);
			if (file->check_time > 0) {
				json_object_set_new(jfile, "checksum ok", file->check_ok ? json_true() : json_false());
				json_object_set_new(jfile, "check time", json_integer(file->check_time));
			}

			json_object_set_new(jfile, "archived time", json_integer(file->archived_time));

			json_array_append_new(jfiles, jfile);
		}
		json_object_set_new(volume, "files", jfiles);
		json_object_set_new(volume, "nb files", json_integer(vol->nb_files));

		json_t * media = json_object();
		json_object_set_new(media, "uuid", json_string(vol->media->uuid));
		if (vol->media->label != NULL)
			json_object_set_new(media, "label", json_string(vol->media->label));
		else
			json_object_set_new(media, "label", json_null());
		if (vol->media->name != NULL)
			json_object_set_new(media, "name", json_string(vol->media->name));
		else
			json_object_set_new(media, "name", json_null());
		json_object_set_new(media, "medium serial number", json_string(vol->media->medium_serial_number));
		json_object_set_new(volume, "media", media);

		json_array_append_new(volumes, volume);

		nb_files += vol->nb_files;
		if (last_file != NULL && !strcmp(last_file, vol->files[0].file->name))
			nb_files--;
		last_file = vol->files[vol->nb_files - 1].file->name;
	}
	json_object_set_new(jarchive, "volumes", volumes);
	json_object_set_new(jarchive, "nb volumes", json_integer(self->archive->nb_volumes));
	json_object_set_new(jarchive, "nb files", json_integer(nb_files));

	json_t * jjob = json_object();
	json_object_set_new(jjob, "name", json_string(job->name));
	json_object_set_new(jjob, "host", st_host_get_info());
	json_object_set_new(jjob, "num run", json_integer(job->num_runs));

	json_t * sdata = json_object();
	json_object_set_new(sdata, "archive", jarchive);
	json_object_set_new(sdata, "job", jjob);

	json_t * returned_data = st_script_run(job->db_connect, job, job->driver->name, st_script_type_pre, self->pool, sdata);
	bool sr = st_io_json_should_run(returned_data);

	json_decref(returned_data);
	json_decref(sdata);

	return sr;
}

static int st_job_create_archive_run(struct st_job * job) {
	struct st_job_create_archive_private * self = job->data;

	st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_important, "Start archive job (named: %s), num runs %ld", job->name, job->num_runs);

	if (!job->user->can_archive) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Error, user (%s) cannot create archive", job->user->login);
		return 1;
	}

	unsigned int i;
	ssize_t max_file_size = 0;
	for (i = 0; i < self->nb_selected_paths; i++) {
		struct st_job_selected_path * p = self->selected_paths + i;

		ssize_t l_mfs = st_format_find_greater_file(p->path, self->pool->format);
		if (l_mfs > max_file_size)
			max_file_size = l_mfs;
	}

	if (self->pool->unbreakable_level == st_pool_unbreakable_level_file && max_file_size > self->pool->format->capacity) {
		char buf_fs[16], buf_ms[16];
		st_util_file_convert_size_to_string(max_file_size, buf_fs, 16);
		st_util_file_convert_size_to_string(self->pool->format->capacity, buf_ms, 16);

		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Pool (%s) request that file should not be split and we found a file (size: %s > media's size: %s) which need to be split", self->pool->name, buf_fs, buf_ms);
		job->sched_status = st_job_status_error;

		return 1;
	}

	char bufsize[16];
	st_util_file_convert_size_to_string(self->total_size, bufsize, 16);
	st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "Will archive %s", bufsize);

	if (job->db_status == st_job_status_stopped) {
		st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_important, "Job stopped by user request");
		return 0;
	}

	self->meta = st_job_create_archive_meta_worker_new(job, job->db_connect);
	self->worker = st_job_create_archive_single_worker(job, self->archive, self->total_size, job->db_connect, self->meta);

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
			st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "Archiving file: %s", p->path);
			failed = st_job_create_archive_archive(job, p, p->path);
		}

		bool stopped = job->db_status == st_job_status_stopped;

		self->worker->ops->close(self->worker);
		self->meta->ops->wait(self->meta, true);

		job->done = 0.97;
		st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "Synchronize database");
		self->worker->ops->sync_db(self->worker);

		job->done = 0.98;
		st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "Writing index file");
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
			st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_important, "Job stopped by user request");

		job->done = 0.99;
		st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_important, "Archive job (named: %s) finished", job->name);
	} else {
		if (self->worker != NULL)
			self->worker->ops->close(self->worker);

		self->meta->ops->wait(self->meta, true);

		st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_normal, "Job stopped by user request");
		st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_important, "Archive job (named: %s) finished with status 'stopped'", job->name);
	}

	return failed;
}

