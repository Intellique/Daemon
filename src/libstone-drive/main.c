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
// free
#include <stdlib.h>
// strcmp
#include <string.h>

#include <libstone/database.h>
#include <libstone/json.h>
#include <libstone/log.h>
#include <libstone/poll.h>
#include <libstone/value.h>

#include "drive.h"

static bool stop = false;

static void changer_request(int fd, short event, void * data);


static void changer_request(int fd, short event, void * data __attribute__((unused))) {
	switch (event) {
		case POLLHUP:
			st_log_write(st_log_level_alert, "changer has hang up");
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

	if (!strcmp("stop", command))
		stop = true;
	else if (!strcmp("update status", command)) {
		struct st_drive_driver * driver = stdr_drive_get();
		struct st_drive * drive = driver->device;

		int failed = drive->ops->update_status();

		struct st_value * returned = st_value_pack("{si}", "status", (long long int) failed);
		st_json_encode_to_fd(returned, 1, true);
		st_value_free(returned);
	}

	st_value_free(request);
	free(command);
}

int main() {
	struct st_drive_driver * driver = stdr_drive_get();
	if (driver == NULL)
		return 1;

	struct st_value * config = st_json_parse_fd(0, 5000);
	if (config == NULL)
		return 2;

	struct st_value * log_config = NULL, * drive_config = NULL, * db_config = NULL;
	st_value_unpack(config, "{sososo}", "logger", &log_config, "drive", &drive_config, "database", &db_config);

	if (log_config == NULL || drive_config == NULL || db_config == NULL)
		return 3;

	st_log_configure(log_config, st_log_type_drive);
	st_database_load_config(db_config);

	st_poll_register(0, POLLIN | POLLHUP, changer_request, NULL, NULL);

	struct st_drive * drive = driver->device;
	int failed = drive->ops->init(drive_config);
	if (failed != 0)
		return 4;

	while (!stop) {
		st_poll(-1);
	}

	return 0;
}

