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

#define NULL (void *) 0

// pthread_mutex
#include <pthread.h>
// bool
#include <stdbool.h>

#include <libstone/database.h>
#include <libstone/json.h>
#include <libstone/log.h>
#include <libstone/poll.h>
#include <libstone/thread_pool.h>
#include <libstone/value.h>
#include <libstone-job/changer.h>

#include "job.h"

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
}

static void job_worker(void * arg __attribute__((unused))) {
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

	struct st_value * log_config = NULL, * db_config = NULL, * devices = NULL;
	st_value_unpack(config, "{sososo}", "logger", &log_config, "database", &db_config, "devices", &devices);
	if (log_config == NULL || db_config == NULL || devices == NULL)
		return 3;

	st_log_configure(log_config, st_log_type_job);
	st_database_load_config(db_config);
	stj_changer_set_config(devices);

	st_poll_register(0, POLLIN | POLLHUP, daemon_request, NULL, NULL);

	st_thread_pool_run("job_worker", job_worker, NULL);

	while (!stop) {
		st_poll(5000);

		pthread_mutex_lock(&lock);
		if (finished)
			stop = true;
		pthread_mutex_unlock(&lock);
	}

	st_log_stop_logger();

	return 0;
}

