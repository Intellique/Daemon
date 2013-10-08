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
*  Last modified: Wed, 05 Jun 2013 20:04:36 +0200                            *
\****************************************************************************/

// json_*
#include <jansson.h>
// pthread_mutex_*
#include <pthread.h>
// free, malloc
#include <stdlib.h>

#include <libstone/job.h>
#include <libstone/library/archive.h>
#include <libstone/library/media.h>
#include <libstone/user.h>
#include <libstone/util/time.h>

#include "check_archive.h"

struct st_job_check_archive_report {
	json_t * root;
	json_t * job;
	json_t * archive;
	json_t * volumes;

	pthread_mutex_t lock;
};


void st_job_check_archive_report_check_volume(struct st_job_check_archive_report * report, struct st_archive_volume * volume, bool ok) {
	json_t * jmedia = json_object();
	json_object_set_new(jmedia, "uuid", json_string(volume->media->uuid));
	json_object_set_new(jmedia, "medium serial number", json_string(volume->media->medium_serial_number));
	json_object_set_new(jmedia, "status", json_string(st_media_status_to_string(volume->media->status)));

	json_t * jvol = json_object();
	json_object_set_new(jvol, "sequence", json_integer(volume->sequence));
	json_object_set_new(jvol, "size", json_integer(volume->size));
	json_object_set_new(jvol, "media", jmedia);
	json_object_set_new(jvol, "checksum ok", json_boolean(ok));

	char buffer[20];
	st_util_time_convert(NULL, "%F %T", buffer, 20);
	json_object_set_new(jvol, "checktime", json_string(buffer));

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
	json_object_set_new(self->job, "quick mode", json_boolean(quick_mode));

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

	pthread_mutex_init(&self->lock, NULL);

	return self;
}

