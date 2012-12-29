/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sat, 29 Dec 2012 17:04:19 +0100                         *
\*************************************************************************/

#include <stddef.h>

#include <libstone/job.h>

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
	.name        = "copy-archive",
	.new_job     = st_job_copy_archive_new_job,
	.cookie      = NULL,
	.api_level   = {
		.checksum = 0,
		.database = 0,
		.job      = STONE_JOB_API_LEVEL,
	},
};


static bool st_job_copy_archive_check(struct st_job * job) {
}

static void st_job_copy_archive_free(struct st_job * job) {
}

static void st_job_copy_archive_init() {
	st_job_register_driver(&st_job_copy_archive_driver);
}

static void st_job_copy_archive_new_job(struct st_job * job, struct st_database_connection * db) {
}

static int st_job_copy_archive_run(struct st_job * job) {
}

