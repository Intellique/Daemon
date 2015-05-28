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

#include <unistd.h>

#include <libstoriqone/database.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/value.h>

#include "changer.h"
#include "drive.h"
#include "listen.h"
#include "media.h"

#include "config.h"

static bool stop = false;

static void sochgr_daemon_hungup(int fd, short event, void * data);
static void sochgr_daemon_request(int fd, short event, void * data);


static void sochgr_daemon_hungup(int fd __attribute__((unused)), short event __attribute__((unused)), void * data __attribute__((unused))) {
	so_log_write(so_log_level_alert, dgettext("libstoriqone-changer", "Stoned has hang up"));
	stop = true;
}

static void sochgr_daemon_request(int fd, short event __attribute__((unused)), void * data __attribute__((unused))) {
	struct so_value * request = so_json_parse_fd(fd, 1000);
	char * command;
	if (request == NULL || so_value_unpack(request, "{ss}", "command", &command) < 0) {
		if (request != NULL)
			so_value_free(request);
		return;
	}

	if (!strcmp("stop", command))
		stop = true;
	else if (!strcmp("ping", command)) {
		struct so_value * response = so_value_pack("{ss}", "return", "pong");
		so_json_encode_to_fd(response, 1, true);
		so_value_free(response);
	}

	so_value_free(request);
	free(command);
}

int main() {
	bindtextdomain("libstoriqone-changer", LOCALE_DIR);

	struct so_changer_driver * driver = sochgr_changer_get();
	if (driver == NULL)
		return 1;

	so_log_write(so_log_level_info,
		dgettext("libstoriqone-changer", "Starting changer (type: %s)"),
		driver->name);

	struct so_value * config = so_json_parse_fd(0, 5000);
	if (config == NULL)
		return 2;

	struct so_value * log_config = NULL, * changer_config = NULL, * db_config = NULL, * socket = NULL;
	so_value_unpack(config, "{sososo}", "logger", &log_config, "changer", &changer_config, "database", &db_config);
	so_value_unpack(changer_config, "{so}", "socket", &socket);

	if (log_config == NULL || changer_config == NULL || db_config == NULL || socket == NULL)
		return 3;

	so_log_configure(log_config, so_log_type_changer);
	so_database_load_config(db_config);
	sochgr_drive_set_config(log_config, db_config, socket);
	sochgr_listen_configure(socket);

	so_poll_register(0, POLLHUP, sochgr_daemon_hungup, NULL, NULL);
	so_poll_register(0, POLLIN, sochgr_daemon_request, NULL, NULL);

	struct so_database * db_driver = so_database_get_default_driver();
	if (db_driver == NULL)
		return 4;
	struct so_database_config * db_conf = db_driver->ops->get_default_config();
	if (db_conf == NULL)
		return 4;
	struct so_database_connection * db_connect = db_conf->ops->connect(db_conf);
	if (db_connect == NULL)
		return 4;

	int c = 1;
	while (c)
		sleep(1);

	so_log_write(so_log_level_info,
		dgettext("libstoriqone-changer", "Initialize changer (type: %s)"),
		driver->name);

	struct so_changer * changer = driver->device;
	int failed = changer->ops->init(changer_config, db_connect);
	if (failed != 0)
		return 5;

	sochgr_listen_set_db_connection(db_connect);
	sochgr_media_init(changer);

	unsigned int nb_free_drives = changer->nb_drives;
	while (!stop) {
		db_connect->ops->sync_changer(db_connect, changer, so_database_sync_default);

		switch (changer->next_action) {
			case so_changer_action_put_offline:
				so_log_write(so_log_level_notice,
					dgettext("libstoriqone-changer", "[%s | %s]: changer will be offline"),
					changer->vendor, changer->model);

				if (changer->ops->put_offline(db_connect) == 0)
					so_log_write(so_log_level_notice,
						dgettext("libstoriqone-changer", "[%s | %s]: changer is now offline"),
						changer->vendor, changer->model);
				else
					so_log_write(so_log_level_warning,
						dgettext("libstoriqone-changer", "[%s | %s]: failed to put changer offline"),
						changer->vendor, changer->model);
				break;

			case so_changer_action_put_online:
				so_log_write(so_log_level_notice,
					dgettext("libstoriqone-changer", "[%s | %s]: changer will be online"),
					changer->vendor, changer->model);

				if (changer->ops->put_online(db_connect) == 0) {
					so_log_write(so_log_level_notice,
						dgettext("libstoriqone-changer", "[%s | %s]: changer is now online"),
						changer->vendor, changer->model);

					sochgr_listen_online();
				} else
					so_log_write(so_log_level_warning,
						dgettext("libstoriqone-changer", "[%s | %s]: failed to put changer online"),
						changer->vendor, changer->model);
				break;

			default:
				so_poll(12000);
				break;
		}

		unsigned int i, nb_new_free_drives = 0;
		for (i = 0; i < changer->nb_drives; i++) {
			struct so_drive * dr = changer->drives + i;
			if (!dr->enable)
				continue;

			dr->ops->update_status(dr);

			if (dr->ops->is_free(dr))
				nb_new_free_drives++;
		}

		unsigned int nb_clients = sochgr_listen_nb_clients();
		if (nb_free_drives < nb_new_free_drives && nb_clients > 1)
			sochgr_socket_unlock(NULL, false);
		nb_free_drives = nb_new_free_drives;

		if (nb_clients == 0)
			changer->ops->check(db_connect);
	}

	so_log_write(so_log_level_info, dgettext("libstoriqone-changer", "Changer (type: %s) will stop"), driver->name);

	failed = changer->ops->shut_down(db_connect);

	so_log_stop_logger();

	return 0;
}

