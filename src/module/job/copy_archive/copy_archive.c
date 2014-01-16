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
*  Last modified: Thu, 16 Jan 2014 16:32:33 +0100                            *
\****************************************************************************/

// json_*
#include <jansson.h>
// malloc
#include <stdlib.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>

#include <libstone/database.h>
#include <libstone/host.h>
#include <libstone/io.h>
#include <libstone/library/archive.h>
#include <libstone/library/drive.h>
#include <libstone/library/media.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>
#include <libstone/script.h>
#include <libstone/util/file.h>
#include <libstone/util/hashtable.h>

#include <libjob-copy-archive.chcksum>

#include "copy_archive.h"

static bool st_job_copy_archive_check(struct st_job * job);
static void st_job_copy_archive_free(struct st_job * job);
static void st_job_copy_archive_init(void) __attribute__((constructor));
static void st_job_copy_archive_new_job(struct st_job * job, struct st_database_connection * db);
static void st_job_copy_archive_on_error(struct st_job * job);
static void st_job_copy_archive_post_run(struct st_job * job);
static bool st_job_copy_archive_pre_run(struct st_job * job);
static int st_job_copy_archive_run(struct st_job * job);

static struct st_job_ops st_job_copy_archive_ops = {
	.check    = st_job_copy_archive_check,
	.free     = st_job_copy_archive_free,
	.on_error = st_job_copy_archive_on_error,
	.post_run = st_job_copy_archive_post_run,
	.pre_run  = st_job_copy_archive_pre_run,
	.run      = st_job_copy_archive_run,
};

static struct st_job_driver st_job_copy_archive_driver = {
	.name         = "copy-archive",
	.new_job      = st_job_copy_archive_new_job,
	.cookie       = NULL,
	.api_level    = {
		.checksum = 0,
		.database = 0,
		.job      = STONE_JOB_API_LEVEL,
	},
	.src_checksum = STONE_JOB_COPYARCHIVE_SRCSUM,
};


static bool st_job_copy_archive_check(struct st_job * job) {
	job->sched_status = st_job_status_error;
	job->repetition = 0;
	return false;
}

static void st_job_copy_archive_free(struct st_job * job) {
	struct st_job_copy_archive_private * self = job->data;

	unsigned int i;
	for (i = 0; i < self->copy->nb_volumes; i++) {
		struct st_archive_volume * vol = self->copy->volumes + i;
		free(vol->files);
		vol->files = NULL;
		vol->nb_files = 0;
	}

	self->copy->copy_of = NULL;
	st_archive_free(self->archive);
	st_archive_free(self->copy);

	self->connect->ops->free(self->connect);
	self->connect = NULL;

	for (i = 0; i < self->nb_checksums; i++)
		free(self->checksums[i]);
	free(self->checksums);
	self->checksums = NULL;

	free(job->data);
	job->data = NULL;
}

static void st_job_copy_archive_init() {
	st_job_register_driver(&st_job_copy_archive_driver);
}

static void st_job_copy_archive_new_job(struct st_job * job, struct st_database_connection * db) {
	struct st_job_copy_archive_private * self = malloc(sizeof(struct st_job_copy_archive_private));
	bzero(self, sizeof(*self));
	self->job = job;
	self->connect = db->config->ops->connect(db->config);

	self->archive = self->connect->ops->get_archive_volumes_by_job(self->connect, job);
	self->pool = st_pool_get_by_job(job, self->connect);

	unsigned int i;
	for (i = 0; i < self->archive->nb_volumes; i++) {
		struct st_archive_volume * vol = self->archive->volumes + i;

		self->connect->ops->get_archive_files_by_job_and_archive_volume(self->connect, self->job, vol);
		self->archive_size += self->archive->volumes[i].size;
		self->nb_remain_files += vol->nb_files;
	}

	self->copy = st_archive_new(self->job->name, self->job->user);
	self->copy->copy_of = self->archive;
	self->copy->metadatas = NULL;
	if (self->archive->metadatas != NULL)
		self->copy->metadatas = strdup(self->archive->metadatas);

	self->checksums = self->connect->ops->get_checksums_by_pool(self->connect, self->pool, &self->nb_checksums);

	job->data = self;
	job->ops = &st_job_copy_archive_ops;
}

static void st_job_copy_archive_on_error(struct st_job * job) {
	struct st_job_copy_archive_private * self = job->data;

	if (self->connect->ops->get_nb_scripts(self->connect, job->driver->name, st_script_type_on_error, self->pool) == 0)
		return;

	struct st_archive * archive = self->archive;
	json_t * src_archive = json_object();
	json_object_set_new(src_archive, "name", json_string(archive->name));
	json_object_set_new(src_archive, "uuid", json_string(archive->uuid));
	json_object_set_new(src_archive, "size", json_integer(self->archive_size));
	json_object_set_new(src_archive, "checksum ok", archive->check_ok ? json_true() : json_false());
	if (archive->check_time > 0)
		json_object_set_new(src_archive, "check time", json_integer(archive->check_time));
	else
		json_object_set_new(src_archive, "check time", json_null());

	json_t * volumes = json_array();
	json_object_set_new(src_archive, "volumes", volumes);
	json_object_set_new(src_archive, "nb volumes", json_integer(archive->nb_volumes));

	unsigned int i;
	for (i = 0; i < archive->nb_volumes; i++) {
		struct st_archive_volume * vol = archive->volumes + i;
		json_t * jvol = json_object();

		json_object_set_new(jvol, "position", json_integer(vol->sequence));
		json_object_set_new(jvol, "size", json_integer(vol->size));
		json_object_set_new(jvol, "start time", json_integer(vol->start_time));
		json_object_set_new(jvol, "end time", json_integer(vol->end_time));

		json_t * checksum = json_object();
		unsigned int nb_digests, j;
		const void ** checksums = st_hashtable_keys(vol->digests, &nb_digests);
		for (j = 0; j < nb_digests; j++) {
			struct st_hashtable_value digest = st_hashtable_get(vol->digests, checksums[j]);
			json_object_set_new(checksum, checksums[j], json_string(digest.value.string));
		}
		free(checksums);
		json_object_set_new(jvol, "checksums", checksum);
		json_object_set_new(jvol, "checksum ok", vol->check_ok ? json_true() : json_false());
		json_object_set_new(jvol, "check time", json_integer(vol->check_time));

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
		json_object_set_new(jvol, "files", jfiles);
		json_object_set_new(jvol, "nb files", json_integer(vol->nb_files));

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
		json_object_set_new(jvol, "media", media);

		json_array_append_new(volumes, jvol);
	}

	// json_t * pool = json_object();
	// json_object_set_new(pool, archive->pool->

	archive = self->copy;
	json_t * copy_archive = json_object();
	json_object_set_new(copy_archive, "name", json_string(archive->name));

	json_t * data = json_object();
	json_object_set_new(data, "source archive", src_archive);
	json_object_set_new(data, "copy archive", copy_archive);
	json_object_set_new(data, "host", st_host_get_info());

	json_t * returned_data = st_script_run(self->connect, job, job->driver->name, st_script_type_pre, self->pool, data);

	json_decref(returned_data);
	json_decref(data);
}

static void st_job_copy_archive_post_run(struct st_job * job) {
	struct st_job_copy_archive_private * self = job->data;

	if (self->connect->ops->get_nb_scripts(self->connect, job->driver->name, st_script_type_post, self->pool) == 0)
		return;

	struct st_archive * archive = self->archive;
	json_t * src_archive = json_object();
	json_object_set_new(src_archive, "name", json_string(archive->name));
	json_object_set_new(src_archive, "uuid", json_string(archive->uuid));
	json_object_set_new(src_archive, "size", json_integer(self->archive_size));
	json_object_set_new(src_archive, "checksum ok", archive->check_ok ? json_true() : json_false());
	if (archive->check_time > 0)
		json_object_set_new(src_archive, "check time", json_integer(archive->check_time));
	else
		json_object_set_new(src_archive, "check time", json_null());

	json_t * volumes = json_array();
	json_object_set_new(src_archive, "volumes", volumes);
	json_object_set_new(src_archive, "nb volumes", json_integer(archive->nb_volumes));

	unsigned int i;
	for (i = 0; i < archive->nb_volumes; i++) {
		struct st_archive_volume * vol = archive->volumes + i;
		json_t * jvol = json_object();

		json_object_set_new(jvol, "position", json_integer(vol->sequence));
		json_object_set_new(jvol, "size", json_integer(vol->size));
		json_object_set_new(jvol, "start time", json_integer(vol->start_time));
		json_object_set_new(jvol, "end time", json_integer(vol->end_time));

		json_t * checksum = json_object();
		unsigned int nb_digests, j;
		const void ** checksums = st_hashtable_keys(vol->digests, &nb_digests);
		for (j = 0; j < nb_digests; j++) {
			struct st_hashtable_value digest = st_hashtable_get(vol->digests, checksums[j]);
			json_object_set_new(checksum, checksums[j], json_string(digest.value.string));
		}
		free(checksums);
		json_object_set_new(jvol, "checksums", checksum);
		json_object_set_new(jvol, "checksum ok", vol->check_ok ? json_true() : json_false());
		json_object_set_new(jvol, "check time", json_integer(vol->check_time));

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
		json_object_set_new(jvol, "files", jfiles);
		json_object_set_new(jvol, "nb files", json_integer(vol->nb_files));

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
		json_object_set_new(jvol, "media", media);

		json_array_append_new(volumes, jvol);
	}

	archive = self->copy;
	json_t * copy_archive = json_object();
	json_object_set_new(copy_archive, "name", json_string(archive->name));
	json_object_set_new(copy_archive, "uuid", json_string(archive->uuid));
	json_object_set_new(copy_archive, "size", json_integer(self->archive_size));
	json_object_set_new(copy_archive, "checksum ok", archive->check_ok ? json_true() : json_false());
	if (archive->check_time > 0)
		json_object_set_new(copy_archive, "check time", json_integer(archive->check_time));
	else
		json_object_set_new(copy_archive, "check time", json_null());

	volumes = json_array();
	json_object_set_new(copy_archive, "volumes", volumes);
	json_object_set_new(copy_archive, "nb volumes", json_integer(archive->nb_volumes));

	for (i = 0; i < archive->nb_volumes; i++) {
		struct st_archive_volume * vol = archive->volumes + i;
		json_t * jvol = json_object();

		json_object_set_new(jvol, "position", json_integer(vol->sequence));
		json_object_set_new(jvol, "size", json_integer(vol->size));
		json_object_set_new(jvol, "start time", json_integer(vol->start_time));
		json_object_set_new(jvol, "end time", json_integer(vol->end_time));

		json_t * checksum = json_object();
		unsigned int nb_digests, j;
		const void ** checksums = st_hashtable_keys(vol->digests, &nb_digests);
		for (j = 0; j < nb_digests; j++) {
			struct st_hashtable_value digest = st_hashtable_get(vol->digests, checksums[j]);
			json_object_set_new(checksum, checksums[j], json_string(digest.value.string));
		}
		free(checksums);
		json_object_set_new(jvol, "checksums", checksum);
		json_object_set_new(jvol, "checksum ok", vol->check_ok ? json_true() : json_false());
		json_object_set_new(jvol, "check time", json_integer(vol->check_time));

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
		json_object_set_new(jvol, "files", jfiles);
		json_object_set_new(jvol, "nb files", json_integer(vol->nb_files));

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
		json_object_set_new(jvol, "media", media);

		json_array_append_new(volumes, jvol);
	}

	json_t * data = json_object();
	json_object_set_new(data, "source archive", src_archive);
	json_object_set_new(data, "copy archive", copy_archive);
	json_object_set_new(data, "host", st_host_get_info());

	json_t * returned_data = st_script_run(self->connect, job, job->driver->name, st_script_type_post, self->pool, data);

	json_decref(returned_data);
	json_decref(data);
}

static bool st_job_copy_archive_pre_run(struct st_job * job) {
	struct st_job_copy_archive_private * self = job->data;

	if (self->connect->ops->get_nb_scripts(self->connect, job->driver->name, st_script_type_pre, self->pool) == 0)
		return true;

	struct st_archive * archive = self->archive;
	json_t * src_archive = json_object();
	json_object_set_new(src_archive, "name", json_string(archive->name));
	json_object_set_new(src_archive, "uuid", json_string(archive->uuid));
	json_object_set_new(src_archive, "size", json_integer(self->archive_size));
	json_object_set_new(src_archive, "checksum ok", archive->check_ok ? json_true() : json_false());
	if (archive->check_time > 0)
		json_object_set_new(src_archive, "check time", json_integer(archive->check_time));
	else
		json_object_set_new(src_archive, "check time", json_null());

	json_t * volumes = json_array();
	json_object_set_new(src_archive, "volumes", volumes);
	json_object_set_new(src_archive, "nb volumes", json_integer(archive->nb_volumes));

	unsigned int i;
	for (i = 0; i < archive->nb_volumes; i++) {
		struct st_archive_volume * vol = archive->volumes + i;
		json_t * jvol = json_object();

		json_object_set_new(jvol, "position", json_integer(vol->sequence));
		json_object_set_new(jvol, "size", json_integer(vol->size));
		json_object_set_new(jvol, "start time", json_integer(vol->start_time));
		json_object_set_new(jvol, "end time", json_integer(vol->end_time));

		json_t * checksum = json_object();
		unsigned int nb_digests, j;
		const void ** checksums = st_hashtable_keys(vol->digests, &nb_digests);
		for (j = 0; j < nb_digests; j++) {
			struct st_hashtable_value digest = st_hashtable_get(vol->digests, checksums[j]);
			json_object_set_new(checksum, checksums[j], json_string(digest.value.string));
		}
		free(checksums);
		json_object_set_new(jvol, "checksums", checksum);
		json_object_set_new(jvol, "checksum ok", vol->check_ok ? json_true() : json_false());
		json_object_set_new(jvol, "check time", json_integer(vol->check_time));

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
		json_object_set_new(jvol, "files", jfiles);
		json_object_set_new(jvol, "nb files", json_integer(vol->nb_files));

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
		json_object_set_new(jvol, "media", media);

		json_array_append_new(volumes, jvol);
	}

	// json_t * pool = json_object();
	// json_object_set_new(pool, archive->pool->

	archive = self->copy;
	json_t * copy_archive = json_object();
	json_object_set_new(copy_archive, "name", json_string(archive->name));

	json_t * data = json_object();
	json_object_set_new(data, "source archive", src_archive);
	json_object_set_new(data, "copy archive", copy_archive);
	json_object_set_new(data, "host", st_host_get_info());

	json_t * returned_data = st_script_run(self->connect, job, job->driver->name, st_script_type_pre, self->pool, data);
	bool sr = st_io_json_should_run(returned_data);

	json_decref(returned_data);
	json_decref(data);

	return sr;
}

static int st_job_copy_archive_run(struct st_job * job) {
	struct st_job_copy_archive_private * self = job->data;

	st_job_add_record(self->connect, st_log_level_info, job, st_job_record_notif_important, "Start copy archive job (named: %s), num runs %ld", job->name, job->num_runs);

	job->done = 0.01;

	st_job_copy_archive_select_output_media(self);
	st_job_copy_archive_select_input_media(self, self->archive->volumes->media);

	int failed = 0;
	if (job->db_status != st_job_status_stopped) {
		if (self->drive_input != NULL)
			failed = st_job_copy_archive_direct_copy(self);
		else {
			self->drive_input = self->drive_output;
			self->drive_output = NULL;

			st_job_copy_archive_select_input_media(self, self->archive->volumes->media);

			failed = st_job_copy_archive_indirect_copy(self);
		}
	}

	// release memory
	if (self->drive_input != NULL) {
		self->drive_input->lock->ops->unlock(self->drive_input->lock);
		self->drive_input = NULL;
	}

	self->drive_output->lock->ops->unlock(self->drive_output->lock);
	self->drive_output = NULL;

	if (job->sched_status == st_job_status_running) {
		job->done = 1;
	}

	return failed;
}

