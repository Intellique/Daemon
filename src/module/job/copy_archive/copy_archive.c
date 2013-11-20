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
*  Last modified: Thu, 07 Nov 2013 13:30:34 +0100                            *
\****************************************************************************/

// malloc
#include <stdlib.h>
// strdup
#include <string.h>

#include <libstone/database.h>
#include <libstone/library/archive.h>
#include <libstone/library/drive.h>
#include <libstone/library/media.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>

#include <libjob-copy-archive.chcksum>

#include "copy_archive.h"

static bool st_job_copy_archive_check(struct st_job * job);
static void st_job_copy_archive_free(struct st_job * job);
static void st_job_copy_archive_init(void) __attribute__((constructor));
static void st_job_copy_archive_new_job(struct st_job * job, struct st_database_connection * db);
static int st_job_copy_archive_run(struct st_job * job);

static struct st_job_ops st_job_copy_archive_ops = {
	.check = st_job_copy_archive_check,
	.free  = st_job_copy_archive_free,
	.run   = st_job_copy_archive_run,
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
	free(job->data);
	job->data = NULL;
}

static void st_job_copy_archive_init() {
	st_job_register_driver(&st_job_copy_archive_driver);
}

static void st_job_copy_archive_new_job(struct st_job * job, struct st_database_connection * db) {
	struct st_job_copy_archive_private * self = malloc(sizeof(struct st_job_copy_archive_private));
	self->job = job;
	self->connect = db->config->ops->connect(db->config);

	self->total_done = 0;
	self->archive_size = 0;
	self->archive = NULL;
	self->copy = NULL;

	self->drive_input = NULL;
	self->slot_input = NULL;
	self->nb_remain_files = 0;

	self->current_volume = NULL;
	self->drive_output = NULL;
	self->pool = NULL;
	self->writer = NULL;

	self->first_media = self->last_media = NULL;

	self->checksum_writer = NULL;
	self->nb_checksums = 0;
	self->checksums = NULL;

	job->data = self;
	job->ops = &st_job_copy_archive_ops;
}

static int st_job_copy_archive_run(struct st_job * job) {
	struct st_job_copy_archive_private * self = job->data;

	st_job_add_record(self->connect, st_log_level_info, job, "Start copy archive job (named: %s), num runs %ld", job->name, job->num_runs);

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

	if (self->drive_input != NULL) {
		self->drive_input->lock->ops->unlock(self->drive_input->lock);
		self->drive_input = NULL;
	}

	self->drive_output->lock->ops->unlock(self->drive_output->lock);
	self->drive_output = NULL;

	for (i = 0; i < self->nb_checksums; i++)
		free(self->checksums[i]);
	free(self->checksums);
	self->checksums = NULL;

	if (job->sched_status == st_job_status_running) {
		job->done = 1;
	}

	return failed;
}

