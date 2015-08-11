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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// bindtextdomain, dgettext
#include <libintl.h>
// bool
#include <stdbool.h>
// free
#include <stdlib.h>
// strcmp
#include <string.h>

#include <libstoriqone/config.h>
#include <libstoriqone/database.h>
#include <libstoriqone/host.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>

#include "changer.h"
#include "drive.h"
#include "listen.h"

#include "config.h"

static void changer_request(int fd, short event, void * data);


static void changer_request(int fd, short event __attribute__((unused)), void * data __attribute__((unused))) {
	so_log_write(so_log_level_alert, "changer has hang up");
	so_poll_unregister(fd, POLLHUP);
	sodr_changer_stop();
}

int main() {
	bindtextdomain("libstoriqone-drive", LOCALE_DIR);

	struct so_drive_driver * driver = sodr_drive_get();
	if (driver == NULL)
		return 1;

	struct so_value * config = so_json_parse_fd(0, -1);
	if (config == NULL)
		return 2;

	so_log_write(so_log_level_info,
		dgettext("libstoriqone-drive", "Starting drive (type: %s)"),
		driver->name);

	struct so_value * log_config = NULL, * drive_config = NULL, * db_config = NULL, * socket_config = NULL, * default_values = NULL;
	so_value_unpack(config, "{sososososo}",
		"logger", &log_config,
		"drive", &drive_config,
		"database", &db_config,
		"socket", &socket_config,
		"default values", &default_values
	);

	so_config_set(default_values);

	if (log_config == NULL || drive_config == NULL || db_config == NULL || socket_config == NULL)
		return 3;

	so_log_configure(log_config, so_log_type_drive);
	so_database_load_config(db_config);

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

	so_poll_register(0, POLLHUP, changer_request, NULL, NULL);

	sodr_changer_setup(db_connect);
	sodr_listen_configure(socket_config);
	sodr_listen_set_db_connection(db_connect);

	so_json_encode_to_fd(sodr_listen_get_socket_config(), 1, true);

	struct so_drive * drive = driver->device;
	so_log_write(so_log_level_info,
		dgettext("libstoriqone-drive", "Initializing drive (type: %s)"),
		driver->name);
	int failed = drive->ops->init(drive_config, db_connect);
	if (failed != 0)
		return 6;

	so_log_write(so_log_level_info,
		dgettext("libstoriqone-drive", "Starting drive (type: %s, vendor: %s, model: %s, serial number: %s)"),
		driver->name, drive->vendor, drive->model, drive->serial_number);

	db_connect->ops->sync_drive(db_connect, drive, true, so_database_sync_id_only);

	drive->ops->update_status(db_connect);

	while (!sodr_changer_is_stopped()) {
		db_connect->ops->sync_drive(db_connect, drive, !sodr_listen_is_locked(), so_database_sync_default);

		so_poll(-1);
	}

	so_log_write(so_log_level_info,
		dgettext("libstoriqone-drive", "Drive (type: %s) will stop"),
		driver->name);

	so_log_stop_logger();

	so_value_free(config);

	return 0;
}

