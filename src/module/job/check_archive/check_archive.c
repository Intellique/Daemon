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
*  Copyright (C) 2013-2015, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Thu, 19 Feb 2015 18:05:30 +0100                            *
\****************************************************************************/

// json_*
#include <jansson.h>
// free, malloc
#include <stdlib.h>

#include <libstone/host.h>
#include <libstone/io.h>
#include <libstone/job.h>
#include <libstone/library/media.h>
#include <libstone/log.h>
#include <libstone/script.h>
#include <libstone/util/file.h>
#include <libstone/util/hashtable.h>
#include <libstone/user.h>

#include <libjob-check-archive.chcksum>

#include "common.h"

static bool st_job_check_archive_check(struct st_job * job);
static void st_job_check_archive_free(struct st_job * job);
static void st_job_check_archive_init(void) __attribute__((constructor));
static void st_job_check_archive_new_job(struct st_job * job);
static void st_job_check_archive_on_error(struct st_job * job);
static void st_job_check_archive_post_run(struct st_job * job);
static bool st_job_check_archive_pre_run(struct st_job * job);
static int st_job_check_archive_run(struct st_job * job);

static struct st_job_ops st_job_check_archive_ops = {
	.check    = st_job_check_archive_check,
	.free     = st_job_check_archive_free,
	.on_error = st_job_check_archive_on_error,
	.post_run = st_job_check_archive_post_run,
	.pre_run  = st_job_check_archive_pre_run,
	.run      = st_job_check_archive_run,
};

static struct st_job_driver st_job_check_archive_driver = {
	.name        = "check-archive",
	.new_job     = st_job_check_archive_new_job,
	.cookie      = NULL,
	.api_level   = {
		.checksum = 0,
		.database = 0,
		.job      = STONE_JOB_API_LEVEL,
	},
	.src_checksum   = STONE_JOB_CHECKARCHIVE_SRCSUM,
};


static bool st_job_check_archive_check(struct st_job * job) {
	job->sched_status = st_job_status_scheduled;
	job->repetition = 1;
	return true;
}

static void st_job_check_archive_free(struct st_job * job) {
	struct st_job_check_archive_private * self = job->data;
	if (job->db_connect != NULL) {
		job->db_connect->ops->close(job->db_connect);
		job->db_connect->ops->free(job->db_connect);
		job->db_connect = NULL;
	}

	if (self->archive != NULL)
		st_archive_free(self->archive);
	self->archive = NULL;
	self->pool = NULL;

	self->vol_checksums = NULL;
	self->nb_vol_checksums = 0;

	self->checksum_reader = NULL;

	free(self);
	job->data = NULL;
}

static void st_job_check_archive_init(void) {
	st_job_register_driver(&st_job_check_archive_driver);
}

static void st_job_check_archive_new_job(struct st_job * job) {
	struct st_job_check_archive_private * self = malloc(sizeof(struct st_job_check_archive_private));
	self->job = job;
	job->db_connect = job->db_config->ops->connect(job->db_config);

	job->data = self;
	job->ops = &st_job_check_archive_ops;

	self->quick_mode = false;
	if (st_hashtable_has_key(job->option, "quick_mode")) {
		struct st_hashtable_value qm = st_hashtable_get(job->option, "quick_mode");

		if (st_hashtable_val_can_convert(&qm, st_hashtable_value_boolean))
			self->quick_mode = st_hashtable_val_convert_to_bool(&qm);
		else if (st_hashtable_val_can_convert(&qm, st_hashtable_value_signed_integer))
			self->quick_mode = st_hashtable_val_convert_to_signed_integer(&qm) != 0;
	}

	self->archive = job->db_connect->ops->get_archive_volumes_by_job(job->db_connect, job);
	if (self->archive == NULL)
		return;

	self->archive_size = 0;
	self->pool = st_pool_get_by_archive(self->archive, job->db_connect);

	unsigned int i;
	for (i = 0; i < self->archive->nb_volumes; i++) {
		struct st_archive_volume * vol = self->archive->volumes + i;
		job->db_connect->ops->get_archive_files_by_job_and_archive_volume(job->db_connect, self->job, vol);
		self->archive_size += vol->size;

		unsigned int j;
		for (j = 0; j < vol->nb_files; j++) {
			struct st_archive_files * ff = vol->files + j;
			struct st_archive_file * file = ff->file;
			job->db_connect->ops->get_checksums_of_file(job->db_connect, file);
		}
	}

	self->vol_checksums = NULL;
	self->nb_vol_checksums = 0;
	self->checksum_reader = NULL;
	self->report = NULL;
}

static void st_job_check_archive_on_error(struct st_job * job) {
	struct st_job_check_archive_private * self = job->data;

	if (job->db_connect->ops->get_nb_scripts(job->db_connect, job->driver->name, st_script_type_on_error, self->pool) == 0)
		return;

	json_t * user = json_object();
	json_object_set_new(user, "login", json_string(job->user->login));
	json_object_set_new(user, "name", json_string(job->user->fullname));
	json_object_set_new(user, "email", json_string(job->user->email));

	json_t * archive = json_object();
	json_object_set_new(archive, "name", json_string(self->archive->name));
	json_object_set_new(archive, "uuid", json_string(self->archive->uuid));
	json_object_set_new(archive, "size", json_integer(self->archive_size));

	json_t * volumes = json_array();
	json_object_set_new(archive, "volumes", volumes);
	json_object_set_new(archive, "nb volumes", json_integer(self->archive->nb_volumes));

	unsigned int i;
	for (i = 0; i < self->archive->nb_volumes; i++) {
		json_t * volume = json_object();
		struct st_archive_volume * vol = self->archive->volumes + i;

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
	}

	json_t * jjob = json_object();
	json_object_set_new(jjob, "quick mode", self->quick_mode ? json_true() : json_false());

	json_t * data = json_object();
	json_object_set_new(data, "user", user);
	json_object_set_new(data, "archive", archive);
	json_object_set_new(data, "host", st_host_get_info());
	json_object_set_new(data, "job", jjob);

	json_t * returned_data = st_script_run(job->db_connect, job, job->driver->name, st_script_type_pre, self->pool, data);

	json_decref(returned_data);
	json_decref(data);
}

static void st_job_check_archive_post_run(struct st_job * job) {
	struct st_job_check_archive_private * self = job->data;

	json_t * user = json_object();
	json_object_set_new(user, "login", json_string(job->user->login));
	json_object_set_new(user, "name", json_string(job->user->fullname));
	json_object_set_new(user, "email", json_string(job->user->email));

	json_t * archive = json_object();
	json_object_set_new(archive, "name", json_string(self->archive->name));
	json_object_set_new(archive, "uuid", json_string(self->archive->uuid));
	json_object_set_new(archive, "size", json_integer(self->archive_size));

	json_t * volumes = json_array();
	json_object_set_new(archive, "volumes", volumes);
	json_object_set_new(archive, "nb volumes", json_integer(self->archive->nb_volumes));

	unsigned int i;
	for (i = 0; i < self->archive->nb_volumes; i++) {
		json_t * volume = json_object();
		struct st_archive_volume * vol = self->archive->volumes + i;

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
		json_object_set_new(volume, "checksum ok", vol->check_ok ? json_true() : json_false());
		json_object_set_new(volume, "check time", json_integer(vol->check_time));

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

			char perm[11];
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
	}

	json_t * jjob = json_object();
	json_object_set_new(jjob, "quick mode", self->quick_mode ? json_true() : json_false());

	json_t * data = json_object();
	json_object_set_new(data, "user", user);
	json_object_set_new(data, "archive", archive);
	json_object_set_new(data, "host", st_host_get_info());
	json_object_set_new(data, "job", jjob);

	json_t * returned_data = st_script_run(job->db_connect, job, job->driver->name, st_script_type_post, self->pool, data);

	json_decref(returned_data);
	json_decref(data);
}

static bool st_job_check_archive_pre_run(struct st_job * job) {
	struct st_job_check_archive_private * self = job->data;

	if (self->archive == NULL) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Error, no archive with this check job");
		return false;
	}

	if (job->db_connect->ops->get_nb_scripts(job->db_connect, job->driver->name, st_script_type_pre, self->pool) == 0)
		return true;

	json_t * user = json_object();
	json_object_set_new(user, "login", json_string(job->user->login));
	json_object_set_new(user, "name", json_string(job->user->fullname));
	json_object_set_new(user, "email", json_string(job->user->email));

	json_t * archive = json_object();
	json_object_set_new(archive, "name", json_string(self->archive->name));
	json_object_set_new(archive, "uuid", json_string(self->archive->uuid));
	json_object_set_new(archive, "size", json_integer(self->archive_size));

	json_t * volumes = json_array();
	json_object_set_new(archive, "volumes", volumes);
	json_object_set_new(archive, "nb volumes", json_integer(self->archive->nb_volumes));

	unsigned int i;
	for (i = 0; i < self->archive->nb_volumes; i++) {
		json_t * volume = json_object();
		struct st_archive_volume * vol = self->archive->volumes + i;

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
	}

	json_t * jjob = json_object();
	json_object_set_new(jjob, "quick mode", self->quick_mode ? json_true() : json_false());

	json_t * data = json_object();
	json_object_set_new(data, "user", user);
	json_object_set_new(data, "archive", archive);
	json_object_set_new(data, "host", st_host_get_info());
	json_object_set_new(data, "job", jjob);

	json_t * returned_data = st_script_run(job->db_connect, job, job->driver->name, st_script_type_pre, self->pool, data);
	bool sr = st_io_json_should_run(returned_data);

	json_decref(returned_data);
	json_decref(data);

	return sr;
}

static int st_job_check_archive_run(struct st_job * job) {
	struct st_job_check_archive_private * self = job->data;

	st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_important, "Start check-archive job (named: %s), num runs %ld", job->name, job->num_runs);

	if (self->archive == NULL) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Error, no archive associated to this job were found");
		job->sched_status = st_job_status_error;
		return 2;
	}

	job->done = 0.01;

	self->report = st_job_check_archive_report_new(job, self->archive, self->quick_mode);

	int failed;
	if (self->quick_mode)
		failed = st_job_check_archive_quick_mode(self);
	else
		failed = st_job_check_archive_thorough_mode(self);

	if (failed)
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Check-archive job (named: %s) finished with status %d", job->name, failed);
	else
		st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_important, "Check-archive job (named: %s) finished with status OK", job->name);

	char * report = st_job_check_archive_report_make(self->report);
	if (report != NULL)
		job->db_connect->ops->add_report(job->db_connect, job, self->archive, NULL, report);
	free(report);

	st_job_check_archive_report_free(self->report);
	self->report = NULL;

	return failed;
}

