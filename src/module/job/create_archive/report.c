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
*  Last modified: Fri, 14 Nov 2014 11:36:20 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// json_*
#include <jansson.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>

#include <libstone/job.h>
#include <libstone/library/archive.h>
#include <libstone/library/media.h>
#include <libstone/user.h>
#include <libstone/util/time.h>

#include "create_archive.h"

struct st_job_create_archive_report {
	json_t * root;
	json_t * job;
	json_t * archive;

	json_t * volumes;
	json_t * files;

	json_t * last_file;
};


void st_job_create_archive_report_add_file(struct st_job_create_archive_report * report, struct st_archive_volume * volume, struct st_archive_files * f) {
	struct st_archive_file * file = f->file;
	json_t * jfile = json_object();
	json_object_set_new(jfile, "filename", json_string(file->name));
	json_object_set_new(jfile, "filetype", json_string(st_archive_file_type_to_string(file->type)));
	json_object_set_new(jfile, "mime type", json_string(file->mime_type));
	json_object_set_new(jfile, "owner", json_string(file->owner));
	json_object_set_new(jfile, "owner id", json_integer(file->ownerid));
	json_object_set_new(jfile, "group", json_string(file->group));
	json_object_set_new(jfile, "group id", json_integer(file->groupid));
	json_object_set_new(jfile, "size", json_integer(file->size));

	char buffer[20];
	st_util_time_convert(&file->create_time, "%F %T", buffer, 20);
	json_object_set_new(jfile, "create time", json_string(buffer));

	st_util_time_convert(&file->modify_time, "%F %T", buffer, 20);
	json_object_set_new(jfile, "modify time", json_string(buffer));

	st_util_time_convert(&file->archived_time, "%F %T", buffer, 20);
	json_object_set_new(jfile, "archived time", json_string(buffer));

	char * vol_seq;
	asprintf(&vol_seq, "%ld", volume->sequence);

	json_object_set_new(report->files, file->name, jfile);

	json_t * vol = json_object_get(report->volumes, vol_seq);
	json_t * files = json_object_get(vol, "files");
	json_array_append(files, jfile);

	free(vol_seq);
}

void st_job_create_archive_report_add_volume(struct st_job_create_archive_report * report, struct st_archive_volume * volume) {
	json_t * jmedia = json_object();
	json_object_set_new(jmedia, "uuid", json_string(volume->media->uuid));
	json_object_set_new(jmedia, "medium serial number", json_string(volume->media->medium_serial_number));
	json_object_set_new(jmedia, "status", json_string(st_media_status_to_string(volume->media->status)));

	json_t * vol = json_object();
	json_object_set_new(vol, "sequence", json_integer(volume->sequence));
	json_object_set_new(vol, "size", json_integer(volume->size));
	json_object_set_new(vol, "media", jmedia);

	json_t * files = json_array();
	json_object_set_new(vol, "files", files);

	char * vol_seq;
	asprintf(&vol_seq, "%ld", volume->sequence);

	json_object_set_new(report->volumes, vol_seq, vol);

	json_t * vols = json_object_get(report->archive, "volumes");
	json_array_append(vols, vol);

	free(vol_seq);
}

void st_job_create_archive_report_free(struct st_job_create_archive_report * report) {
	json_decref(report->root);
	json_decref(report->volumes);
	json_decref(report->files);
	free(report);
}

char * st_job_create_archive_report_make(struct st_job_create_archive_report * report) {
	char buffer[20];
	st_util_time_convert(NULL, "%F %T", buffer, 20);
	json_object_set_new(report->job, "endtime", json_string(buffer));

	return json_dumps(report->root, JSON_COMPACT);
}

struct st_job_create_archive_report * st_job_create_archive_report_new(struct st_job * job, struct st_archive * archive, struct st_pool * pool) {
	struct st_job_create_archive_report * self = malloc(sizeof(struct st_job_create_archive_report));

	self->job = json_object();
	json_object_set_new(self->job, "type", json_string("create-archive"));
	char buffer[20];
	st_util_time_convert(NULL, "%F %T", buffer, 20);
	json_object_set_new(self->job, "start time", json_string(buffer));

	struct st_user * user = job->user;
	json_t * juser = json_object();
	json_object_set_new(juser, "login", json_string(user->login));
	json_object_set_new(juser, "name", json_string(user->fullname));

	self->archive = json_object();
	json_object_set_new(self->archive, "uuid", json_string(archive->uuid));
	json_object_set_new(self->archive, "name", json_string(archive->name));
	json_object_set_new(self->archive, "volumes", json_array());

	size_t total_size = 0;
	unsigned int i;
	for (i = 0; i < archive->nb_volumes; i++) {
		struct st_archive_volume * vol = archive->volumes + i;
		total_size += vol->size;
	}
	json_object_set_new(self->archive, "size", json_integer(total_size));

	if (pool == NULL)
		pool = archive->volumes->media->pool;

	json_t * jpool = json_object();
	json_object_set_new(jpool, "name", json_string(pool->name));
	json_object_set_new(jpool, "uuid", json_string(pool->uuid));

	self->root = json_object();
	json_object_set_new(self->root, "job", self->job);
	json_object_set_new(self->root, "user", juser);
	json_object_set_new(self->root, "archive", self->archive);
	json_object_set_new(self->root, "pool", jpool);

	self->volumes = json_object();
	self->files = json_object();

	self->last_file = NULL;

	return self;
}

