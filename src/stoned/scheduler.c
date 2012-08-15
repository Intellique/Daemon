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
*  Last modified: Wed, 15 Aug 2012 17:07:28 +0200                         *
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

#include "scheduler.h"

static void st_sched_exit(int signal);
static void st_sched_run_job(void * arg);

static short st_sched_stop_request = 0;


void st_sched_do_loop(struct st_database_connection * connection) {
	signal(SIGINT, st_sched_exit);

	st_log_write_all(st_log_level_info, st_log_type_scheduler, "starting main loop");

	struct st_job ** jobs = 0;
	unsigned int nb_jobs = 0;
	time_t update = time(0);

	while (!st_sched_stop_request) {
		connection->ops->sync_job(connection, &jobs, &nb_jobs);

		/*
		unsigned int i;
		for (i = 0; i < nb_jobs; i++) {
			struct st_job * j = jobs[i];
			if (!j)
				continue;

			if (j->id < 0) {
				free(j);
				jobs[i] = 0;
				continue;
			}

			connection->ops->refresh_job(connection, j);

			if (j->id < 0 && j->sched_status == st_job_status_running)
				j->job_ops->stop(j);
		}

		short ok_transaction = connection->ops->start_transaction(connection) >= 0;
		if (!ok_transaction)
			st_log_write_all(st_log_level_warning, st_log_type_scheduler, "error while starting transaction");

		// check for new jobs
		long new_jobs = 0;
		int failed = connection->ops->get_nb_new_jobs(connection, &new_jobs, update, last_max_jobs);
		if (failed) {
			st_log_write_all(st_log_level_warning, st_log_type_scheduler, "failed to get new jobs");
		} else if (new_jobs > 0) {
			jobs = realloc(jobs, (nb_jobs + new_jobs) * sizeof(struct st_job *));
			connection->ops->get_new_jobs(connection, jobs + nb_jobs, new_jobs, update, last_max_jobs);

			for (i = nb_jobs; i < nb_jobs + new_jobs; i++) {
				struct st_job * j = jobs[i];

				if (!j->driver) {
					free(j);
					jobs[i] = 0;
					continue;
				}

				j->sched_status = st_job_status_idle;
				//st_sched_init_job(j);

				//j->driver->new_job(connection, j);
			}

			nb_jobs += new_jobs;
		}

		if (ok_transaction)
			connection->ops->cancel_transaction(connection);

		// update jobs array
		for (i = 0; i < nb_jobs; i++) {
			unsigned int j;
			for (j = i; !jobs[j] && j < nb_jobs; j++);
			if (!jobs[i]) {
				if (j < nb_jobs)
					memmove(jobs + i, jobs + j, (nb_jobs - j) * sizeof(struct st_job *));
				nb_jobs -= j - i;
				jobs = realloc(jobs, nb_jobs * sizeof(struct st_job *));
			}
		}

		// check if there is jobs needed to be started
		for (i = 0; i < nb_jobs; i++) {
			struct st_job * j = jobs[i];

			// start job
			if (st_job_status_idle == j->db_status && j->start < update && st_job_status_idle == j->sched_status && j->repetition > 0) {
				st_sched_init_job(j);
				j->driver->new_job(connection, j);
				st_threadpool_run(st_sched_run_job, j);
			}

			if (j->id > last_max_jobs)
				last_max_jobs = j->id;
		}

		// update status of stone-alone drives
		st_changer_update_drive_status();
		*/

		//struct tm current;
		//update = time(0);
		//localtime_r(&update, &current);
		//sleep(5 - current.tm_sec % 5);

	}
}

void st_sched_exit(int signal) {
	if (signal == SIGINT) {
		st_log_write_all(st_log_level_info, st_log_type_scheduler, "catch SIGINT");
		st_sched_stop_request = 1;
	}
}

void st_sched_run_job(void * arg) {
	struct st_job * job = arg;

	st_log_write_all(st_log_level_info, st_log_type_scheduler, "starting job");

	job->sched_status = st_job_status_running;
	job->repetition--;
	job->num_runs++;

	int status = job->job_ops->run(job);

	if (job->sched_status == st_job_status_running)
		job->sched_status = st_job_status_idle;

	job->job_ops->free(job);
	job->data = 0;

	free(job->name);
	job->name = 0;

	job->user = 0;

	job->driver = 0;

	if (job->meta)
		st_hashtable_free(job->meta);
	job->meta = 0;
	if (job->option)
		st_hashtable_free(job->option);
	job->option = 0;

	st_log_write_all(st_log_level_info, st_log_type_scheduler, "job finished, with exited code = %d", status);
}

