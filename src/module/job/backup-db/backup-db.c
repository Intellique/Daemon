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
*  Last modified: Wed, 23 May 2012 15:52:20 +0200                         *
\*************************************************************************/

#include <stone/database.h>
#include <stone/io.h>
#include <stone/job.h>
#include <stone/log.h>
#include <stone/user.h>

static void st_job_backupdb_free(struct st_job * job);
static void st_job_backupdb_init(void) __attribute__((constructor));
static void st_job_backupdb_new_job(struct st_database_connection * db, struct st_job * job);
static int st_job_backupdb_run(struct st_job * job);
static int st_job_backupdb_stop(struct st_job * job);

struct st_job_ops st_job_backupdb_ops = {
	.free = st_job_backupdb_free,
	.run  = st_job_backupdb_run,
	.stop = st_job_backupdb_stop,
};


struct st_job_driver st_job_backupdb_driver = {
	.name        = "backup-db",
	.new_job     = st_job_backupdb_new_job,
	.cookie      = 0,
	.api_version = STONE_JOB_APIVERSION,
};

void st_job_backupdb_free(struct st_job * job __attribute__((unused))) {}

void st_job_backupdb_init() {
	st_job_register_driver(&st_job_backupdb_driver);
}

void st_job_backupdb_new_job(struct st_database_connection * db __attribute__((unused)), struct st_job * job) {
	job->data = 0;
	job->job_ops = &st_job_backupdb_ops;
}

int st_job_backupdb_run(struct st_job * job) {
	struct st_database * driver = st_db_get_default_db();

	struct st_stream_reader * db_reader = driver->ops->backup_db(driver);
	return 0;
}

int st_job_backupdb_stop(struct st_job * job __attribute__((unused))) {
	return 0;
}

