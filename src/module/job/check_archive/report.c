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
*  Last modified: Mon, 14 Oct 2013 12:23:34 +0200                            *
\****************************************************************************/

// json_*
#include <jansson.h>
// pthread_mutex_*
#include <pthread.h>
// free, malloc
#include <stdlib.h>
// strcmp
#include <string.h>

#include <libstone/job.h>
#include <libstone/library/archive.h>
#include <libstone/library/media.h>
#include <libstone/user.h>
#include <libstone/util/time.h>

#include "common.h"

struct st_job_check_archive_report {
	json_t * root;
	json_t * job;
	json_t * archive;
	json_t * volumes;

	json_t * current_file;
	json_t * current_volume;

	pthread_mutex_t lock;
};


void st_job_check_archive_report_add_file(struct st_job_check_archive_report * report, struct st_archive_files * f) {
	pthread_mutex_lock(&report->lock);

	struct st_archive_file * file = f->file;
	if (report->current_file != NULL) {
		json_t * filename = json_object_get(report->current_file, "filename");

		if (!strcmp(json_string_value(filename), file->name)) {
			pthread_mutex_unlock(&report->lock);
			return;
		}
	}

	report->current_file = json_object();
	json_object_set_new(report->current_file, "filename", json_string(file->name));
	json_object_set_new(report->current_file, "filetype", json_string(st_archive_file_type_to_string(file->type)));
	json_object_set_new(report->current_file, "mime type", json_string(file->mime_type));
	json_object_set_new(report->current_file, "owner", json_string(file->owner));
	json_object_set_new(report->current_file, "owner id", json_integer(file->ownerid));
	json_object_set_new(report->current_file, "group", json_string(file->group));
	json_object_set_new(report->current_file, "group id", json_integer(file->groupid));
	json_object_set_new(report->current_file, "size", json_integer(file->size));

	char buffer[20];
	st_util_time_convert(&file->create_time, "%F %T", buffer, 20);
	json_object_set_new(report->current_file, "create time", json_string(buffer));

	st_util_time_convert(&file->modify_time, "%F %T", buffer, 20);
	json_object_set_new(report->current_file, "modify time", json_string(buffer));

	st_util_time_convert(&file->archived_time, "%F %T", buffer, 20);
	json_object_set_new(report->current_file, "archived time", json_string(buffer));

	json_t * files = json_object_get(report->current_volume, "files");
	json_array_append_new(files, report->current_file);

	pthread_mutex_unlock(&report->lock);
}

void st_job_check_archive_report_add_volume(struct st_job_check_archive_report * report, struct st_archive_volume * volume) {
	pthread_mutex_lock(&report->lock);

	json_t * jmedia = json_object();
	json_object_set_new(jmedia, "uuid", json_string(volume->media->uuid));
	json_object_set_new(jmedia, "medium serial number", json_string(volume->media->medium_serial_number));
	json_object_set_new(jmedia, "status", json_string(st_media_status_to_string(volume->media->status)));

	report->current_volume = json_object();
	json_object_set_new(report->current_volume, "sequence", json_integer(volume->sequence));
	json_object_set_new(report->current_volume, "size", json_integer(volume->size));
	json_object_set_new(report->current_volume, "media", jmedia);

	json_t * files = json_array();
	json_object_set_new(report->current_volume, "files", files);

	if (report->current_file != NULL)
		json_array_append(files, report->current_file);

	json_array_append_new(report->volumes, report->current_volume);

	pthread_mutex_unlock(&report->lock);
}

void st_job_check_archive_report_check_file(struct st_job_check_archive_report * report, bool ok) {
	pthread_mutex_lock(&report->lock);

	char buffer[20];
	st_util_time_convert(NULL, "%F %T", buffer, 20);
	json_object_set_new(report->current_file, "checktime", json_string(buffer));
	json_object_set_new(report->current_file, "checksum ok", ok ? json_true() : json_false());
	report->current_file = NULL;

	pthread_mutex_unlock(&report->lock);
}

void st_job_check_archive_report_check_volume(struct st_job_check_archive_report * report, bool ok) {
	pthread_mutex_lock(&report->lock);

	char buffer[20];
	st_util_time_convert(NULL, "%F %T", buffer, 20);
	json_object_set_new(report->current_volume, "checktime", json_string(buffer));
	json_object_set_new(report->current_volume, "checksum ok", ok ? json_true() : json_false());

	report->current_volume = NULL;

	pthread_mutex_unlock(&report->lock);
}

void st_job_check_archive_report_check_volume2(struct st_job_check_archive_report * report, struct st_archive_volume * volume, bool ok) {
	json_t * jmedia = json_object();
	json_object_set_new(jmedia, "uuid", json_string(volume->media->uuid));
	json_object_set_new(jmedia, "medium serial number", json_string(volume->media->medium_serial_number));
	json_object_set_new(jmedia, "status", json_string(st_media_status_to_string(volume->media->status)));

	json_t * jvol = json_object();
	json_object_set_new(jvol, "sequence", json_integer(volume->sequence));
	json_object_set_new(jvol, "size", json_integer(volume->size));
	json_object_set_new(jvol, "media", jmedia);

	char buffer[20];
	st_util_time_convert(NULL, "%F %T", buffer, 20);
	json_object_set_new(jvol, "checktime", json_string(buffer));
	json_object_set_new(jvol, "checksum ok", ok ? json_true() : json_false());

	pthread_mutex_lock(&report->lock);
	json_array_append_new(report->volumes, jvol);
	pthread_mutex_unlock(&report->lock);
}

void st_job_check_archive_report_free(struct st_job_check_archive_report * report) {
	pthread_mutex_destroy(&report->lock);
	json_decref(report->root);
	free(report);
}

char * st_job_check_archive_report_make(struct st_job_check_archive_report * report) {
	char buffer[20];
	st_util_time_convert(NULL, "%F %T", buffer, 20);
	json_object_set_new(report->job, "endtime", json_string(buffer));

	return json_dumps(report->root, JSON_COMPACT);
}

struct st_job_check_archive_report * st_job_check_archive_report_new(struct st_job * job, struct st_archive * archive, bool quick_mode) {
	struct st_job_check_archive_report * self = malloc(sizeof(struct st_job_check_archive_report));

	self->job = json_object();
	json_object_set_new(self->job, "type", json_string("check-archive"));
	char buffer[20];
	st_util_time_convert(NULL, "%F %T", buffer, 20);
	json_object_set_new(self->job, "start time", json_string(buffer));
	json_object_set_new(self->job, "quick mode", quick_mode ? json_true() : json_false());

	struct st_user * user = job->user;
	json_t * juser = json_object();
	json_object_set_new(juser, "login", json_string(user->login));
	json_object_set_new(juser, "name", json_string(user->fullname));

	self->archive = json_object();
	json_object_set_new(self->archive, "uuid", json_string(archive->uuid));
	json_object_set_new(self->archive, "name", json_string(archive->name));

	self->volumes = json_array();
	json_object_set_new(self->archive, "volumes", self->volumes);

	size_t total_size = 0;
	unsigned int i;
	for (i = 0; i < archive->nb_volumes; i++) {
		struct st_archive_volume * vol = archive->volumes + i;
		total_size += vol->size;
	}
	json_object_set_new(self->archive, "size", json_integer(total_size));

	struct st_pool * pool = archive->volumes->media->pool;
	json_t * jpool = json_object();
	json_object_set_new(jpool, "name", json_string(pool->name));
	json_object_set_new(jpool, "uuid", json_string(pool->uuid));

	self->root = json_object();
	json_object_set_new(self->root, "job", self->job);
	json_object_set_new(self->root, "user", juser);
	json_object_set_new(self->root, "archive", self->archive);
	json_object_set_new(self->root, "pool", jpool);

	self->current_file = NULL;
	self->current_volume = NULL;

	pthread_mutex_init(&self->lock, NULL);

	return self;
}

