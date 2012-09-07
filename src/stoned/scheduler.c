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
*  Last modified: Fri, 07 Sep 2012 10:12:08 +0200                         *
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

#include <stone/database.h>
#include <stone/job.h>
#include <stone/log.h>
#include <stone/threadpool.h>
#include <stone/util/hashtable.h>

#include "library/common.h"
#include "scheduler.h"

struct st_sched_job {
	struct st_database_connection * status_con;
	time_t updated;
};

static int st_sched_add_record(struct st_job * j, enum st_log_level level, const char * format, ...) __attribute__ ((format (printf, 3, 4)));
static void st_sched_exit(int signal);
static void st_sched_init_job(struct st_job * j);
static void st_sched_run_job(void * arg);
static int st_sched_update_status(struct st_job * j);

static short st_sched_stop_request = 0;
static struct st_scheduler_ops st_sched_db_ops = {
	.add_record    = st_sched_add_record,
	.update_status = st_sched_update_status,
};


int st_sched_add_record(struct st_job * j, enum st_log_level level, const char * format, ...) {
	char * message = 0;

	va_list va;
	va_start(va, format);
	vasprintf(&message, format, va);
	va_end(va);

	st_log_write_all2(level, st_log_type_job, j->user, message);

	struct st_sched_job * jp = j->scheduler_private;
	int status = jp->status_con->ops->add_job_record(jp->status_con, j, message);
	free(message);
	return status;
}

void st_sched_do_loop() {
	signal(SIGINT, st_sched_exit);

	struct st_database * db = st_db_get_default_db();
	struct st_database_connection * connection = db->ops->connect(db, 0);
	if (!connection) {
		st_log_write_all(st_log_level_error, st_log_type_scheduler, "Scheduler: failed to connect to database");
		return;
	}

	st_log_write_all(st_log_level_info, st_log_type_scheduler, "starting main loop");

	struct st_job ** jobs = 0;
	unsigned int nb_jobs = 0;
	time_t update = time(0);
	long last_max_jobs = -1;

	while (!st_sched_stop_request) {
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
			else if (j->db_status == st_job_status_stopped && j->sched_status == st_job_status_running)
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

		// update status of changer
		st_changer_update_status(connection);

		struct tm current;
		update = time(0);
		localtime_r(&update, &current);
		sleep(5 - current.tm_sec % 5);

	}

	connection->ops->free(connection);
}

void st_sched_exit(int signal) {
	if (signal == SIGINT) {
		st_log_write_all(st_log_level_info, st_log_type_scheduler, "catch SIGINT");
		st_sched_stop_request = 1;
	}
}

void st_sched_init_job(struct st_job * j) {
	struct st_database * db = st_db_get_default_db();
	struct st_database_connection * connection = db->ops->connect(db, 0);

	struct st_sched_job * jp = j->scheduler_private = malloc(sizeof(struct st_sched_job));
	jp->status_con = connection;
	jp->updated = time(0);

	j->done = 0;
	j->db_ops = &st_sched_db_ops;
}

void st_sched_run_job(void * arg) {
	struct st_job * job = arg;

	st_log_write_all(st_log_level_info, st_log_type_scheduler, "starting job id = %ld", job->id);

	job->sched_status = st_job_status_running;
	job->repetition--;
	job->num_runs++;

	sleep(1);
	st_sched_update_status(job);

	int status = job->job_ops->run(job);

	if (job->sched_status == st_job_status_running)
		job->sched_status = st_job_status_idle;
	if (job->db_status == st_job_status_stopped)
		job->sched_status = st_job_status_stopped;

	sleep(1);
	st_sched_update_status(job);

	job->job_ops->free(job);
	job->data = 0;

	struct st_sched_job * jp = job->scheduler_private;
	jp->status_con->ops->close(jp->status_con);
	jp->status_con->ops->free(jp->status_con);
	free(jp);

	free(job->name);
	job->name = 0;

	job->db_ops = 0;
	job->scheduler_private = 0;

	// j->archive
	job->pool = 0;
	job->tape = 0;

	if (job->paths) {
		unsigned int i;
		for (i = 0; i < job->nb_paths; i++)
			free(job->paths[i]);
		free(job->paths);
		job->paths = 0;
	};

	if (job->checksums) {
		unsigned int i;
		for (i = 0; i < job->nb_checksums; i++)
			free(job->checksums[i]);
		free(job->checksums);
		free(job->checksum_ids);
		job->checksums = 0;
		job->checksum_ids = 0;
	}

	if (job->nb_block_numbers > 0) {
		unsigned int i;
		for (i = 0; i < job->nb_block_numbers; i++) {
			struct st_job_block_number * bn = job->block_numbers + i;
			free(bn->path);
			bn->path = NULL;
		}
		free(job->block_numbers);
		job->block_numbers = NULL;
		job->nb_block_numbers = 0;
	}

	if (job->restore_to) {
		free(job->restore_to->path);
		free(job->restore_to);
		job->restore_to = 0;
	}

	job->user = 0;

	job->driver = 0;

	if (job->job_meta) {
		st_hashtable_free(job->job_meta);
		job->job_meta = 0;
	}
	if (job->job_option) {
		st_hashtable_free(job->job_option);
		job->job_option = 0;
	}

	st_log_write_all(st_log_level_info, st_log_type_scheduler, "job finished, id = %ld, with exited code = %d", job->id, status);

	job->id = -1;
}

int st_sched_update_status(struct st_job * j) {
	struct st_sched_job * jp = j->scheduler_private;

	time_t now = time(0);
	if (now <= jp->updated)
		return 0;

	int status = jp->status_con->ops->update_job(jp->status_con, j);
	jp->updated = now;

	return status;
}

