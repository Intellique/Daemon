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

// bool
#include <stdbool.h>
// getpid
#include <unistd.h>

#include <libstone/json.h>
#include <libstone/poll.h>
#include <libstone/value.h>

#include <libstone-logger/log.h>

#include "listen.h"

static bool stop = false;

static void daemon_request(int fd, short event, void * data);


static void daemon_request(int fd __attribute__((unused)), short event, void * data __attribute__((unused))) {
	switch (event) {
		case POLLHUP:
			lgr_log_write2(st_log_level_alert, st_log_type_logger, "Stoned has hang up");
			stop = true;
			break;
	}
}

int main() {
	lgr_log_write2(st_log_level_notice, st_log_type_logger, "Starting logger process (pid: %d)", getpid());

	struct st_value * config = st_json_parse_fd(0, 5000);
	if (config == NULL || !st_value_hashtable_has_key2(config, "module")) {
		lgr_log_write2(st_log_level_emergencey, st_log_type_logger, "No configuration received from daemon, will quit");
		return 1;
	}

	st_poll_register(0, POLLIN | POLLHUP, daemon_request, NULL, NULL);

	struct st_value * module = st_value_hashtable_get2(config, "module", false);
	lgr_log_load(module);
	lgr_log_write2(st_log_level_debug, st_log_type_logger, "Modules loaded");

	struct st_value * socket = st_value_hashtable_get2(config, "socket", false);
	lgr_listen_configure(socket);

	unsigned int nb_handlers = lgr_listen_nb_clients();
	while (!stop || nb_handlers > 1) {
		st_poll(-1);

		nb_handlers = lgr_listen_nb_clients();
	}

	st_value_free(config);

	lgr_log_write2(st_log_level_notice, st_log_type_logger, "Logger process (pid: %d) will now exit", getpid());

	return 0;
}

