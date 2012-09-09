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
*  Last modified: Fri, 24 Aug 2012 09:18:50 +0200                         *
\*************************************************************************/

// free, malloc
#include <stdlib.h>

#include <libstone/database.h>
#include <libstone/job.h>

struct st_job_save_private {
	struct st_database_connection * connect;
};

static short st_job_save_check(struct st_job * job);
static void st_job_save_free(struct st_job * job);
static void st_job_save_init(void) __attribute__((constructor));
static void st_job_save_new_job(struct st_job * job, struct st_database_connection * db);
static int st_job_save_run(struct st_job * job);

static struct st_job_ops st_job_save_ops = {
	.check = st_job_save_check,
	.free  = st_job_save_free,
	.run   = st_job_save_run,
};

static struct st_job_driver st_job_save_driver = {
	.name        = "save",
	.new_job     = st_job_save_new_job,
	.cookie      = NULL,
	.api_version = STONE_JOB_API_LEVEL,
};


static short st_job_save_check(struct st_job * job __attribute__((unused))) {
	return 2;
}

static void st_job_save_free(struct st_job * job __attribute__((unused))) {}

static void st_job_save_init(void) {
	st_job_register_driver(&st_job_save_driver);
}

static void st_job_save_new_job(struct st_job * job, struct st_database_connection * db) {
	struct st_job_save_private * self = malloc(sizeof(struct st_job_save_private));
	self->connect = db->config->ops->connect(db->config);

	job->data = self;
	job->ops = &st_job_save_ops;
}

static int st_job_save_run(struct st_job * job __attribute__((unused))) {
	return 1;
}

