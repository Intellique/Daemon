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
*  Last modified: Fri, 18 Oct 2013 19:03:10 +0200                            *
\****************************************************************************/

// free, malloc
#include <stdlib.h>

#include <libstone/job.h>
#include <libstone/log.h>
#include <libstone/util/hashtable.h>

#include <libjob-check-archive.chcksum>

#include "common.h"

static bool st_job_check_archive_check(struct st_job * job);
static void st_job_check_archive_free(struct st_job * job);
static void st_job_check_archive_init(void) __attribute__((constructor));
static void st_job_check_archive_new_job(struct st_job * job, struct st_database_connection * db);
static int st_job_check_archive_run(struct st_job * job);

static struct st_job_ops st_job_check_archive_ops = {
	.check = st_job_check_archive_check,
	.free  = st_job_check_archive_free,
	.run   = st_job_check_archive_run,
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
	if (self != NULL) {
		self->connect->ops->close(self->connect);
		self->connect->ops->free(self->connect);
	}

	self->vol_checksums = NULL;
	self->nb_vol_checksums = 0;

	self->checksum_reader = NULL;

	free(self);
	job->data = NULL;
}

static void st_job_check_archive_init(void) {
	st_job_register_driver(&st_job_check_archive_driver);
}

static void st_job_check_archive_new_job(struct st_job * job, struct st_database_connection * db) {
	struct st_job_check_archive_private * self = malloc(sizeof(struct st_job_check_archive_private));
	self->job = job;
	self->connect = db->config->ops->connect(db->config);

	self->archive = NULL;
	self->vol_checksums = NULL;
	self->nb_vol_checksums = 0;

	job->data = self;
	job->ops = &st_job_check_archive_ops;
}

static int st_job_check_archive_run(struct st_job * job) {
	struct st_job_check_archive_private * self = job->data;

	st_job_add_record(self->connect, st_log_level_info, job, "Start check-archive job (named: %s), num runs %ld", job->name, job->num_runs);

	struct st_archive * archive = self->archive = self->connect->ops->get_archive_volumes_by_job(self->connect, job);
	if (archive == NULL) {
		st_job_add_record(self->connect, st_log_level_error, job, "Error, no archive associated to this job were found");
		job->sched_status = st_job_status_error;
		return 2;
	}

	job->done = 0.01;

	bool quick_mode = false;
	if (st_hashtable_has_key(job->option, "quick_mode")) {
		struct st_hashtable_value qm = st_hashtable_get(job->option, "quick_mode");

		if (st_hashtable_val_can_convert(&qm, st_hashtable_value_boolean))
			quick_mode = st_hashtable_val_convert_to_bool(&qm);
		else if (st_hashtable_val_can_convert(&qm, st_hashtable_value_signed_integer))
			quick_mode = st_hashtable_val_convert_to_signed_integer(&qm) != 0;
	}

	self->report = st_job_check_archive_report_new(job, archive, quick_mode);

	int failed;

	if (quick_mode)
		failed = st_job_check_archive_quick_mode(self);
	else
		failed = st_job_check_archive_thorough_mode(self);

	if (failed)
		st_job_add_record(self->connect, st_log_level_error, job, "Check-archive job (named: %s) finished with status %d", job->name, failed);
	else
		st_job_add_record(self->connect, st_log_level_info, job, "Check-archive job (named: %s) finished with status OK", job->name);

	char * report = st_job_check_archive_report_make(self->report);
	if (report != NULL)
		self->connect->ops->add_report(self->connect, job, archive, report);
	free(report);

	st_job_check_archive_report_free(self->report);
	self->report = NULL;

	return failed;
}

