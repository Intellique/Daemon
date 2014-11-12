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
	bool finished;
};

static struct so_value * jobs = NULL;

static void sod_job_exited(int fd, short event, void * data);
static int sod_job_filter(const struct dirent *);


static void sod_job_exited(int fd, short event, void * data) {
	struct so_job * job = data;
	struct so_job_private * self = job->data;

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

		return;
	}

	struct so_value * request = so_json_parse_fd(fd, 1000);
	char * command;
	if (request == NULL || so_value_unpack(request, "{ss}", "command", &command) < 0) {
		if (request != NULL)
			so_value_free(request);
		return;
	}

	if (!strcmp("finished", command)) {
		self->finished = true;
	}

	so_value_free(request);
	free(command);
}

static int sod_job_filter(const struct dirent * file) {
	return !fnmatch("job_*", file->d_name, 0);
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

		struct so_value * command = so_value_pack("{ss}", "command", "sync");
		so_json_encode_to_fd(command, self->fd_in, true);
		so_value_free(command);

		struct so_value * response = so_json_parse_fd(self->fd_out, -1);
		so_job_sync(job, response);
		so_value_free(response);
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

			char * process_name;
			asprintf(&process_name, "job_%s", job->type);

			so_process_new(&self->process, process_name, NULL, 0);
			self->fd_in = so_process_pipe_to(&self->process);
			self->fd_out = so_process_pipe_from(&self->process, so_process_stdout);
			self->config = so_value_pack("{sOsOsOso}",
				"logger", logger,
				"database", db_config,
				"devices", sod_device_get(false),
				"job", so_job_convert(job)
			);

			self->db_connection = db_connection;
			so_value_unpack(kjob, "s", &self->key);
			self->finished = false;

			free(process_name);
		}

		time_t now = time(NULL);
		if (job->status == so_job_status_scheduled && now > job->next_start && job->repetition != 0) {
			struct so_job_private * self = job->data;

			so_poll_register(self->fd_out, POLLHUP | POLLIN, sod_job_exited, job, NULL);

			// start job
			so_process_start(&self->process, 1);
			so_json_encode_to_fd(self->config, self->fd_in, true);
		}
	}
	so_value_iterator_free(iter);
}

void sod_scheduler_init(struct so_database_connection * connection) {
	jobs = so_value_new_hashtable2();

	struct dirent ** files = NULL;
	int nb_files = scandir(DAEMON_BIN_DIR, &files, sod_job_filter, NULL);
	int i;
	for (i = 0; i < nb_files; i++) {
		connection->ops->sync_plugin_job(connection, files[i]->d_name + 4);
		free(files[i]);
	}
	free(files);
}
