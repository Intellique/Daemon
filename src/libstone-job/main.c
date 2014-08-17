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

// pthread_mutex
#include <pthread.h>
// malloc
#include <stdlib.h>
// bool
#include <stdbool.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>

#include <libstone/database.h>
#include <libstone/host.h>
#include <libstone/json.h>
#include <libstone/log.h>
#include <libstone/poll.h>
#include <libstone/thread_pool.h>
#include <libstone/value.h>
#include <libstone-job/changer.h>

#include "job.h"

static struct st_job * job = NULL;
static bool stop = false;
static volatile bool finished = false;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void daemon_request(int fd, short event, void * data);
static void job_worker(void * arg);


static void daemon_request(int fd, short event, void * data __attribute__((unused))) {
	switch (event) {
		case POLLHUP:
			st_log_write(st_log_level_alert, "Stoned has hang up");
			stop = true;
			break;
	}

	struct st_value * request = st_json_parse_fd(fd, 1000);
	char * command;
	if (request == NULL || st_value_unpack(request, "{ss}", "command", &command) < 0) {
		if (request != NULL)
			st_value_free(request);
		return;
	}

	if (!strcmp("sync", command)) {
		struct st_value * cj = st_job_convert(job);
		st_json_encode_to_fd(cj, 1, true);
		st_value_free(cj);
	}

	st_value_free(request);
	free(command);
}

static void job_worker(void * arg) {
	struct st_job_driver * job_dr = stj_job_get_driver();
	struct st_job * j = arg;

	struct st_database * db_driver = st_database_get_default_driver();
	if (db_driver == NULL)
		goto error;
	struct st_database_config * db_conf = db_driver->ops->get_default_config();
	if (db_conf == NULL)
		goto error;
	struct st_database_connection * db_connect = db_conf->ops->connect(db_conf);
	if (db_connect == NULL)
		goto error;

	int failed = job_dr->simulate(j, db_connect);
	if (failed != 0) {
		job->exit_code = failed;
		job->status = st_job_status_error;
		goto error;
	}

	failed = job_dr->script_pre_run(j, db_connect);
	if (failed != 0) {
		job->exit_code = failed;
		job->status = st_job_status_error;
		goto error;
	}

error:
	pthread_mutex_lock(&lock);
	finished = true;
	pthread_mutex_unlock(&lock);
}

int main() {
	struct st_job_driver * job_dr = stj_job_get_driver();
	if (job_dr == NULL)
		return 1;

	struct st_value * config = st_json_parse_fd(0, 5000);
	if (config == NULL)
		return 2;

	struct st_value * log_config = NULL, * db_config = NULL, * devices = NULL, * vjob = NULL;
	st_value_unpack(config, "{sosososo}", "logger", &log_config, "database", &db_config, "devices", &devices, "job", &vjob);
	if (log_config == NULL || db_config == NULL || devices == NULL || vjob == NULL)
		return 3;

	st_poll_register(0, POLLIN | POLLHUP, daemon_request, NULL, NULL);

	st_log_configure(log_config, st_log_type_job);
	st_database_load_config(db_config);
	stj_changer_set_config(devices);

	job = malloc(sizeof(struct st_job));
	bzero(job, sizeof(struct st_job));
	st_job_sync(job, vjob);

	struct st_database * db_driver = st_database_get_default_driver();
	if (db_driver == NULL)
		return 4;
	struct st_database_config * db_conf = db_driver->ops->get_default_config();
	if (db_conf == NULL)
		return 4;
	struct st_database_connection * db_connect = db_conf->ops->connect(db_conf);
	if (db_connect == NULL)
		return 4;

	if (!st_host_init(db_connect))
		return 5;

	job->status = st_job_status_running;

	db_connect->ops->sync_job(db_connect, job);
	db_connect->ops->start_job(db_connect, job);

	st_thread_pool_run("job_worker", job_worker, job);

	while (!stop) {
		st_poll(5000);

		pthread_mutex_lock(&lock);
		if (finished)
			stop = true;
		pthread_mutex_unlock(&lock);

		db_connect->ops->sync_job(db_connect, job);
	}

	if (job->status == st_job_status_running)
		job->status = st_job_status_finished;

	db_connect->ops->stop_job(db_connect, job);

	struct st_value * status = st_value_pack("{ssso}", "command", "finished", "job", st_job_convert(job));
	st_json_encode_to_fd(status, 1, true);
	st_value_free(status);

	st_log_stop_logger();

	return 0;
}

