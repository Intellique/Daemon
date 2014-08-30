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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

#define _GNU_SOURCE
// scandir
#include <dirent.h>
// fnmatch
#include <fnmatch.h>
// stdio
#include <stdio.h>
// malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>
// time
#include <time.h>
// close
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/file.h>
#include <libstone/job.h>
#include <libstone/json.h>
#include <libstone/poll.h>
#include <libstone/process.h>
#include <libstone/value.h>

#include "config.h"
#include "device.h"
#include "scheduler.h"

struct st_job_private {
	struct st_process process;
	int fd_in;
	int fd_out;
	struct st_value * config;

	struct st_database_connection * db_connection;
	char * key;
	bool finished;
};

static struct st_value * jobs = NULL;

static void std_job_exited(int fd, short event, void * data);
static int std_job_filter(const struct dirent *);


static void std_job_exited(int fd, short event, void * data) {
	struct st_job * job = data;
	struct st_job_private * self = job->data;

	if (event & POLLHUP) {
		st_poll_unregister(fd, POLLHUP | POLLIN);

		if (!self->finished) {
			job->status = st_job_status_error;
			self->db_connection->ops->sync_job(self->db_connection, job);
		}

		st_value_hashtable_remove2(jobs, self->key);

		close(self->fd_in);
		close(self->fd_out);
		self->fd_in = self->fd_out = -1;
		st_process_wait(&self->process, 1);
		st_process_free(&self->process, 1);
		free(self->key);
		free(self);

		return;
	}

	struct st_value * request = st_json_parse_fd(fd, 1000);
	char * command;
	if (request == NULL || st_value_unpack(request, "{ss}", "command", &command) < 0) {
		if (request != NULL)
			st_value_free(request);
		return;
	}

	if (!strcmp("finished", command)) {
		self->finished = true;
	}

	st_value_free(request);
	free(command);
}

static int std_job_filter(const struct dirent * file) {
	return !fnmatch("job_*", file->d_name, 0);
}

void std_scheduler_do(struct st_value * logger, struct st_value * db_config, struct st_database_connection * db_connection) {
	static struct timespec last_start;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	if (now.tv_sec - last_start.tv_sec < 15)
		return;

	last_start = now;

	struct st_value_iterator * iter = st_value_hashtable_get_iterator(jobs);
	while (st_value_iterator_has_next(iter)) {
		struct st_value * vjob = st_value_iterator_get_value(iter, false);
		struct st_job * job = st_value_custom_get(vjob);
		struct st_job_private * self = job->data;

		struct st_value * command = st_value_pack("{ss}", "command", "sync");
		st_json_encode_to_fd(command, self->fd_in, true);
		st_value_free(command);

		struct st_value * response = st_json_parse_fd(self->fd_out, -1);
		st_job_sync(job, response);
		st_value_free(response);
	}
	st_value_iterator_free(iter);

	db_connection->ops->sync_jobs(db_connection, jobs);

	iter = st_value_hashtable_get_iterator(jobs);
	while (st_value_iterator_has_next(iter)) {
		struct st_value * kjob = st_value_iterator_get_key(iter, false, false);
		struct st_value * vjob = st_value_iterator_get_value(iter, false);
		struct st_job * job = st_value_custom_get(vjob);

		if (job->data == NULL) {
			struct st_job_private * self = job->data = malloc(sizeof(struct st_job_private));
			bzero(self, sizeof(struct st_job_private));

			char * process_name;
			asprintf(&process_name, "job_%s", job->type);

			st_process_new(&self->process, process_name, NULL, 0);
			self->fd_in = st_process_pipe_to(&self->process);
			self->fd_out = st_process_pipe_from(&self->process, st_process_stdout);
			self->config = st_value_pack("{sOsOsOso}",
				"logger", logger,
				"database", db_config,
				"devices", std_device_get(false),
				"job", st_job_convert(job)
			);

			self->db_connection = db_connection;
			st_value_unpack(kjob, "s", &self->key);
			self->finished = false;

			free(process_name);
		}

		time_t now = time(NULL);
		if (job->status == st_job_status_scheduled && now > job->next_start && job->repetition != 0) {
			struct st_job_private * self = job->data;

			st_poll_register(self->fd_out, POLLHUP | POLLIN, std_job_exited, job, NULL);

			// start job
			st_process_start(&self->process, 1);
			st_json_encode_to_fd(self->config, self->fd_in, true);
		}
	}
	st_value_iterator_free(iter);
}

void std_scheduler_init(struct st_database_connection * connection) {
	jobs = st_value_new_hashtable2();

	struct dirent ** files = NULL;
	int nb_files = scandir(DAEMON_BIN_DIR, &files, std_job_filter, NULL);
	int i;
	for (i = 0; i < nb_files; i++) {
		connection->ops->sync_plugin_job(connection, files[i]->d_name + 4);
		free(files[i]);
	}
	free(files);
}

