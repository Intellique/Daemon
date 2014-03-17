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
// sleep
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
			stop = true;
			break;
	}
}

int main() {
	struct st_value * config = st_json_parse_fd(0, 5000);
	if (config == NULL || !st_value_hashtable_has_key2(config, "module"))
		return 1;

	st_poll_register(0, POLLIN | POLLHUP, daemon_request, NULL, NULL);

	struct st_value * module = st_value_hashtable_get2(config, "module", false);
	lgr_log_load(module);

	struct st_value * socket = st_value_hashtable_get2(config, "socket", false);
	lgr_listen_configure(socket);

	st_json_encode_to_file(config, "logger.json");

	while (!stop)
		st_poll(-1);

	return 0;
}

