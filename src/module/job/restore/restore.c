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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sun, 08 Jan 2012 23:52:11 +0100                         *
\*************************************************************************/

// free, malloc
#include <stdlib.h>

#include <stone/job.h>

struct st_job_restore_private {
};

static void st_job_restore_free(struct st_job * job);
static void st_job_restore_init(void) __attribute__((constructor));
static void st_job_restore_new_job(struct st_database_connection * db, struct st_job * job);
static int st_job_restore_run(struct st_job * job);
static int st_job_restore_stop(struct st_job * job);

struct st_job_ops st_job_restore_ops = {
	.free = st_job_restore_free,
	.run  = st_job_restore_run,
	.stop = st_job_restore_stop,
};

struct st_job_driver st_job_restore_driver = {
	.name        = "restore",
	.new_job     = st_job_restore_new_job,
	.cookie      = 0,
	.api_version = STONE_JOB_APIVERSION,
};


void st_job_restore_free(struct st_job * job) {
}

void st_job_restore_init() {
	st_job_register_driver(&st_job_restore_driver);
}

void st_job_restore_new_job(struct st_database_connection * db, struct st_job * job) {
	struct st_job_restore_private * self = malloc(sizeof(struct st_job_restore_private));

	job->data = self;
	job->job_ops = &st_job_restore_ops;
}

int st_job_restore_run(struct st_job * job) {
	return 0;
}

int st_job_restore_stop(struct st_job * job) {
	return 0;
}

