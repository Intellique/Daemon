/****************************************************************************\
*                    ______           _      ____                            *
*                   / __/ /____  ____(_)__ _/ __ \___  ___                   *
*                  _\ \/ __/ _ \/ __/ / _ `/ /_/ / _ \/ -_)                  *
*                 /___/\__/\___/_/ /_/\_, /\____/_//_/\__/                   *
*                                      /_/                                   *
*  ------------------------------------------------------------------------  *
*  This file is a part of Storiq One                                         *
*                                                                            *
*  Storiq One is free software; you can redistribute it and/or modify        *
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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// asprintf
#include <stdio.h>
// malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>
// time
#include <time.h>
// close, sleep
#include <unistd.h>

#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/job.h>
#include <libstoriqone/json.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/process.h>
#include <libstoriqone/value.h>

#include "config.h"
#include "device.h"
#include "scheduler.h"

struct so_job_private {
	struct so_process process;
	int fd_in;
	int fd_out;
	struct so_value * config;

	struct so_database_connection * db_connection;
	char * key;

	bool started;
	bool finished;
};

static struct so_value * jobs = NULL;

static void sod_job_process(int fd, short event, void * data);


static void sod_job_process(int fd, short event, void * data) {
	struct so_job * job = data;
	struct so_job_private * self = job->data;

	if (event & POLLIN) {
		struct so_value * request = so_json_parse_fd(fd, 1000);
		char * command;
		if (request == NULL || so_value_unpack(request, "{ss}", "command", &command) < 0) {
			if (request != NULL)
				so_value_free(request);
			return;
		}

		if (strcmp("finished", command) == 0) {
			self->finished = true;

			struct so_value * new_job = NULL;
			so_value_unpack(request, "{so}", "job", &new_job);
			if (new_job != NULL)
				so_job_sync(job, new_job);
		}

		so_value_free(request);
		free(command);
	}

	if (event & POLLHUP) {
		so_poll_unregister(fd, POLLHUP | POLLIN);

		if (!self->finished) {
			job->status = so_job_status_error;
			self->db_connection->ops->sync_job(self->db_connection, job);
		}

		so_value_hashtable_remove2(jobs, self->key);

		close(self->fd_in);
		close(self->fd_out);
		self->fd_in = self->fd_out = -1;
		so_process_wait(&self->process, 1);
		so_process_free(&self->process, 1);
		free(self->key);
		free(self);
	}
}

void sod_scheduler_do(struct so_value * logger, struct so_value * db_config, struct so_database_connection * db_connection) {
	static struct timespec last_start;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	if (now.tv_sec - last_start.tv_sec < 15)
		return;

	last_start = now;

	struct so_value_iterator * iter = so_value_hashtable_get_iterator(jobs);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * vjob = so_value_iterator_get_value(iter, false);
		struct so_job * job = so_value_custom_get(vjob);
		struct so_job_private * self = job->data;

		if (self->started && !self->finished) {
			struct so_value * command = so_value_pack("{ss}", "command", "sync");
			so_json_encode_to_fd(command, self->fd_in, true);
			so_value_free(command);

			struct so_value * response = so_json_parse_fd(self->fd_out, -1);
			if (response != NULL)
				so_job_sync(job, response);
			so_value_free(response);
		}
	}
	so_value_iterator_free(iter);

	db_connection->ops->sync_jobs(db_connection, jobs);

	iter = so_value_hashtable_get_iterator(jobs);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * kjob = so_value_iterator_get_key(iter, false, false);
		struct so_value * vjob = so_value_iterator_get_value(iter, false);
		struct so_job * job = so_value_custom_get(vjob);

		if (job->data == NULL) {
			struct so_job_private * self = job->data = malloc(sizeof(struct so_job_private));
			bzero(self, sizeof(struct so_job_private));

			char * process_name = NULL;
			int size = asprintf(&process_name, "job_%s", job->type);

			if (size < 0) {
				free(self);
				job->data = NULL;
				continue;
			}

			/**
			 * valgrind
			 * valgrind -v --log-file=valgrind.log --num-callers=24 --leak-check=full --show-reachable=yes --track-origins=yes ./bin/stoned
			 *
			 * char * path = NULL;
			 * asprintf(&path, "--log-file=valgrind/%s_%s_%s", job->key, job->type, job->name);
			 *
			 * const char * params[] = { "-v", "--log-file=valgrind.log", "--track-fds=yes", "--time-stamp=yes", "--num-callers=24", "--leak-check=full", "--show-reachable=yes", "--track-origins=yes", "--fullpath-after=/home/guillaume/prog/StoriqOne/", process_name };
			 * so_process_new(&self->process, "valgrind", params, 10);
			 *
			 * free(path)
			 */

			const char * params[] = { job->id, job->name };
			so_process_new(&self->process, process_name, params, 2);

			self->fd_in = -1;
			self->fd_out = -1;

			self->config = so_value_pack("{sOsOsOso}",
				"logger", logger,
				"database", db_config,
				"devices", sod_device_get(false),
				"job", so_job_convert(job)
			);

			self->db_connection = db_connection;
			so_value_unpack(kjob, "s", &self->key);
			self->started = false;
			self->finished = false;

			free(process_name);
		}

		time_t now = time(NULL);
		if (job->status == so_job_status_scheduled && now > job->next_start && job->repetition != 0) {
			struct so_job_private * self = job->data;

			self->fd_in = so_process_pipe_to(&self->process);
			self->fd_out = so_process_pipe_from(&self->process, so_process_stdout);

			so_poll_register(self->fd_out, POLLHUP | POLLIN, sod_job_process, job, NULL);

			// start job
			so_process_start(&self->process, 1);
			so_json_encode_to_fd(self->config, self->fd_in, true);

			self->started = true;
		}
	}
	so_value_iterator_free(iter);
}

void sod_scheduler_init(void) {
	jobs = so_value_new_hashtable2();
}

