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
*  Last modified: Wed, 22 Jan 2014 15:04:20 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// json_*
#include <jansson.h>
// pthread_cond_t
#include <pthread.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>

#include <libstone/library/archive.h>
#include <libstone/user.h>
#include <libstone/util/time.h>

#include "create_archive.h"

struct st_job_create_archive_report {
	json_t * root;
	json_t * job;
	json_t * archive;

	json_t * volumes;
	json_t * files;

	pthread_mutex_t lock;
};


void st_job_create_archive_report_free(struct st_job_create_archive_report * report) {
	pthread_mutex_destroy(&report->lock);
	json_decref(report->root);
	json_decref(report->volumes);
	json_decref(report->files);
	free(report);
}

struct st_job_create_archive_report * st_job_create_archive_report_new(struct st_job * job, struct st_archive * archive) {
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

	self->root = json_object();
	json_object_set_new(self->root, "job", self->job);
	json_object_set_new(self->root, "user", juser);
	json_object_set_new(self->root, "archive", self->archive);

	self->volumes = json_object();
	self->files = json_object();

	pthread_mutex_init(&self->lock, NULL);

	return self;
}

