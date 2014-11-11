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

#include <libstoriqone/database.h>
#include <libstoriqone/host.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/thread_pool.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/changer.h>

#include "job.h"

static struct so_job * job = NULL;
static bool stop = false;
static volatile bool finished = false;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void daemon_request(int fd, short event, void * data);
static void job_worker(void * arg);


static void daemon_request(int fd, short event, void * data __attribute__((unused))) {
	switch (event) {
		case POLLHUP:
			so_log_write(so_log_level_alert, "Stoned has hang up");
			stop = true;
			break;
	}

	struct so_value * request = so_json_parse_fd(fd, 1000);
	char * command;
	if (request == NULL || so_value_unpack(request, "{ss}", "command", &command) < 0) {
		if (request != NULL)
			so_value_free(request);
		return;
	}

	if (!strcmp("sync", command)) {
		struct so_value * cj = so_job_convert(job);
		so_json_encode_to_fd(cj, 1, true);
		so_value_free(cj);
	}

	so_value_free(request);
	free(command);
}

static void job_worker(void * arg) {
	struct so_job_driver * job_dr = soj_job_get_driver();
	struct so_job * j = arg;

	struct so_database * db_driver = so_database_get_default_driver();
	if (db_driver == NULL)
		goto error;
	struct so_database_config * db_conf = db_driver->ops->get_default_config();
	if (db_conf == NULL)
		goto error;
	struct so_database_connection * db_connect = db_conf->ops->connect(db_conf);
	if (db_connect == NULL)
		goto error;

	so_job_add_record(j, db_connect, so_log_level_notice, so_job_record_notif_important, "Starting simulation of job (type: %s, key: %s, name: %s)", j->type, j->key, j->name);

	int failed = job_dr->simulate(j, db_connect);
	if (failed != 0) {
		so_job_add_record(j, db_connect, so_log_level_error, so_job_record_notif_important, "Simulation of job (type: %s, key: %s, name: %s) failed with code: %d", j->type, j->key, j->name, failed);
		job->exit_code = failed;
		job->status = so_job_status_error;
		goto error;
	}

	if (!job_dr->script_pre_run(j, db_connect)) {
		job->exit_code = failed;
		job->status = so_job_status_error;
		goto error;
	}

	so_job_add_record(j, db_connect, so_log_level_notice, so_job_record_notif_important, "Starting job (type: %s, key: %s, name: %s)", j->type, j->key, j->name);
	failed = job_dr->run(j, db_connect);
	so_job_add_record(j, db_connect, so_log_level_notice, so_job_record_notif_important, "Job exit (type: %s, key: %s, name: %s), exit code: %d", j->type, j->key, j->name, failed);

	if (failed != 0 && job->stopped_by_user)
		goto error;

	if (failed == 0)
		job_dr->script_post_run(j, db_connect);
	else
		job_dr->script_on_error(j, db_connect);

error:
	job_dr->exit(j, db_connect);

	db_connect->ops->close(db_connect);
	db_connect->ops->free(db_connect);

	pthread_mutex_lock(&lock);
	finished = true;
	pthread_mutex_unlock(&lock);
}

int main() {
	struct so_job_driver * job_dr = soj_job_get_driver();
	if (job_dr == NULL)
		return 1;

	struct so_value * config = so_json_parse_fd(0, 5000);
	if (config == NULL)
		return 2;

	struct so_value * log_config = NULL, * db_config = NULL, * devices = NULL, * vjob = NULL;
	so_value_unpack(config, "{sosososo}", "logger", &log_config, "database", &db_config, "devices", &devices, "job", &vjob);
	if (log_config == NULL || db_config == NULL || devices == NULL || vjob == NULL)
		return 3;

	so_poll_register(0, POLLIN | POLLHUP, daemon_request, NULL, NULL);

	so_log_configure(log_config, so_log_type_job);
	so_database_load_config(db_config);
	soj_changer_set_config(devices);

	job = malloc(sizeof(struct so_job));
	bzero(job, sizeof(struct so_job));
	so_job_sync(job, vjob);

	struct so_database * db_driver = so_database_get_default_driver();
	if (db_driver == NULL)
		return 4;
	struct so_database_config * db_conf = db_driver->ops->get_default_config();
	if (db_conf == NULL)
		return 4;
	struct so_database_connection * db_connect = db_conf->ops->connect(db_conf);
	if (db_connect == NULL)
		return 4;

	if (!so_host_init(db_connect))
		return 5;

	job->status = so_job_status_running;

	db_connect->ops->sync_job(db_connect, job);
	db_connect->ops->start_job(db_connect, job);

	so_thread_pool_run("job_worker", job_worker, job);

	while (!stop) {
		so_poll(5000);

		pthread_mutex_lock(&lock);
		if (finished)
			stop = true;
		pthread_mutex_unlock(&lock);

		db_connect->ops->sync_job(db_connect, job);
	}

	if (job->status == so_job_status_running)
		job->status = so_job_status_finished;

	db_connect->ops->stop_job(db_connect, job);

	struct so_value * status = so_value_pack("{ssso}", "command", "finished", "job", so_job_convert(job));
	so_json_encode_to_fd(status, 1, true);
	so_value_free(status);

	so_log_stop_logger();

	so_value_free(config);

	return 0;
}

