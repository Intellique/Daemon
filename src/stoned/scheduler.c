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

// scandir
#include <dirent.h>
// fnmatch
#include <fnmatch.h>
// malloc
#include <stdlib.h>
// bzero
#include <strings.h>
// time
#include <time.h>

#include <libstone/database.h>
#include <libstone/file.h>
#include <libstone/job.h>
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
};

static struct st_value * jobs = NULL;

static int job_filter(const struct dirent *);


static int job_filter(const struct dirent * file) {
	return !fnmatch("job_*", file->d_name, 0);
}

void std_scheduler_do(struct st_value * logger, struct st_value * db_config, struct st_database_connection * db_connection) {
	struct st_value_iterator * iter = st_value_hashtable_get_iterator(jobs);
	while (st_value_iterator_has_next(iter)) {
		struct st_value * vjob = st_value_iterator_get_value(iter, false);
		struct st_job * job = st_value_custom_get(vjob);
	}
	st_value_iterator_free(iter);

	db_connection->ops->sync_jobs(db_connection, jobs);

	iter = st_value_hashtable_get_iterator(jobs);
	while (st_value_iterator_has_next(iter)) {
		struct st_value * vjob = st_value_iterator_get_value(iter, false);
		struct st_job * job = st_value_custom_get(vjob);

		if (job->data == NULL) {
			struct st_job_private * self = job->data = malloc(sizeof(struct st_job_private));
			bzero(self, sizeof(struct st_job_private));

			st_process_new(&self->process, job->type, NULL, 0);
			self->fd_in = st_process_pipe_to(&self->process);
			self->fd_out = st_process_pipe_from(&self->process, st_process_stdout);
			self->config = st_value_pack("{sOsOsO}",
				"logger", logger,
				"database", db_config,
				"devices", std_device_get(false)
			);
		}

		time_t now = time(NULL);
		if (job->status == st_job_status_scheduled && now > job->next_start && job->repetition != 0) {
			// start job
		}
	}
	st_value_iterator_free(iter);
}

void std_scheduler_init(struct st_database_connection * connection) {
	jobs = st_value_new_hashtable2();

	struct dirent ** files = NULL;
	int nb_files = scandir(DAEMON_BIN_DIR, &files, job_filter, NULL);
	int i;
	for (i = 0; i < nb_files; i++) {
		connection->ops->sync_plugin_job(connection, files[i]->d_name + 4);
		free(files[i]);
	}
	free(files);
}

