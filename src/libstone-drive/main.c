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
#include <libstone/slot.h>
#include <libstone/value.h>

#include "changer.h"
#include "drive.h"
#include "listen.h"

static void changer_request(int fd, short event, void * data);


static void changer_request(int fd, short event __attribute__((unused)), void * data __attribute__((unused))) {
	st_log_write(st_log_level_alert, "changer has hang up");
	st_poll_unregister(fd, POLLHUP);
	stdr_changer_stop();
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

	struct st_value * listen = NULL;
	st_value_unpack(drive_config, "{ss}", "socket", &listen);

	if (log_config == NULL || drive_config == NULL || db_config == NULL)
		return 3;

	st_log_configure(log_config, st_log_type_drive);
	st_database_load_config(db_config);

	struct st_database * db_driver = st_database_get_default_driver();
	if (db_driver == NULL)
		return 4;
	struct st_database_config * db_conf = db_driver->ops->get_default_config();
	if (db_conf == NULL)
		return 4;
	struct st_database_connection * db_connect = db_conf->ops->connect(db_conf);
	if (db_connect == NULL)
		return 4;

	st_poll_register(0, POLLHUP, changer_request, NULL, NULL);

	stdr_changer_setup(db_connect);
	stdr_listen_configure(listen);
	stdr_listen_set_db_connection(db_connect);

	struct st_drive * drive = driver->device;
	int failed = drive->ops->init(drive_config);
	if (failed != 0)
		return 4;

	st_log_write(st_log_level_info, "Starting drive (type: %s, vendor: %s, model: %s, serial number: %s)", driver->name, drive->vendor, drive->model, drive->serial_number);

	drive->ops->update_status(db_connect);
	db_connect->ops->sync_drive(db_connect, drive, st_database_sync_default);

	while (!stdr_changer_is_stopped()) {
		st_poll(-1);

		db_connect->ops->sync_drive(db_connect, drive, st_database_sync_default);
	}

	st_log_write(st_log_level_info, "Changer (type: %s) will stop", driver->name);

	st_log_stop_logger();

	return 0;
}

