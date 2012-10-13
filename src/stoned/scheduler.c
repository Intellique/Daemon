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
*  Last modified: Sat, 13 Oct 2012 09:44:51 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
#include <pthread.h>
// free, realloc
#include <malloc.h>
// signal
#include <signal.h>
// va_end, va_start
#include <stdarg.h>
// memmove
#include <string.h>
// time
#include <time.h>
// sleep
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/job.h>
#include <libstone/log.h>
#include <libstone/threadpool.h>
#include <libstone/util/hashtable.h>

#include "library/common.h"
#include "scheduler.h"

static void st_sched_exit(int signal);
static void st_sched_run_job(void * arg);

static short st_sched_stop_request = 0;


void st_sched_do_loop(struct st_database_connection * connection) {
	signal(SIGINT, st_sched_exit);

	st_log_write_all(st_log_level_info, st_log_type_scheduler, "starting main loop");

	st_changer_sync(connection);

	struct st_job ** jobs = NULL;
	unsigned int nb_jobs = 0;
	time_t update = time(NULL);

	connection->ops->sync_job(connection, &jobs, &nb_jobs);

	unsigned int i;
	for (i = 0; i < nb_jobs; i++) {
		struct st_job * job = jobs[i];

		if (job->db_status == st_job_status_running || job->db_status == st_job_status_waiting) {
			job->driver->new_job(job, connection);

			st_log_write_all(st_log_level_info, st_log_type_scheduler, "Checking job: %s", job->name);
			short ok = job->ops->check(job);
			job->ops->free(job);

			st_log_write_all(st_log_level_info, st_log_type_scheduler, "Checking job: %s finished, status: %s", job->name, ok ? "ok" : "can't recover job");
		}
	}

	while (!st_sched_stop_request) {
		connection->ops->sync_job(connection, &jobs, &nb_jobs);

		for (i = 0; i < nb_jobs; i++) {
			struct st_job * job = jobs[i];

			if (job->repetition != 0 && job->next_start < update && job->db_status == st_job_status_idle && job->sched_status == st_job_status_idle) {
				job->driver->new_job(job, connection);
				st_threadpool_run(st_sched_run_job, job);
			}
		}

		st_changer_sync(connection);

		time_t now = time(NULL);
		if (now - update < 5) {
			struct tm current;
			update = now;
			localtime_r(&update, &current);
			sleep(5 - current.tm_sec % 5);
		} else
			update = now;
	}
}

static void st_sched_exit(int signal) {
	if (signal == SIGINT) {
		st_log_write_all(st_log_level_info, st_log_type_scheduler, "catch SIGINT");
		st_sched_stop_request = 1;
	}
}

static void st_sched_run_job(void * arg) {
	struct st_job * job = arg;

	st_log_write_all(st_log_level_info, st_log_type_scheduler, "Starting job: %s", job->name);

	job->sched_status = st_job_status_running;
	if (job->repetition > 0)
		job->repetition--;
	job->num_runs++;

	int status = job->ops->run(job);

	job->ops->free(job);

	if (job->interval > 0) {
		time_t now = time(NULL);
		long diff = now - job->next_start;
		if (diff < 0)
			diff = job->next_start - now;
		job->next_start += diff - (diff % job->interval) + job->interval;
	}

	if (job->sched_status == st_job_status_running)
		job->sched_status = st_job_status_idle;

	st_log_write_all(st_log_level_info, st_log_type_scheduler, "Job %s finished, with exited code = %d", job->name, status);
}

