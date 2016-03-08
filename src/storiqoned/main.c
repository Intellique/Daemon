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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// bindtextdomain, gettext, textdomain
#include <libintl.h>
// getopt_long
#include <getopt.h>
// bool
#include <stdbool.h>
// printf
#include <stdio.h>
// getpid, getsid
#include <unistd.h>

#include <libstoriqone/crash.h>
#include <libstoriqone/database.h>
#include <libstoriqone/host.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/value.h>

#include "admin.h"
#include "device.h"
#include "env.h"
#include "logger.h"
#include "main.h"
#include "scheduler.h"
#include "plugin.h"

#include "checksum/storiqoned.chcksum"
#include "config.h"
#include "storiqone.version"

static bool sod_daemon_run = true;
static void sod_show_help(void);

int main(int argc, char ** argv) {
	bindtextdomain("storiqoned", LOCALE_DIR);
	textdomain("storiqoned");

	so_crash_init(*argv);

	so_log_write2(so_log_level_notice, so_log_type_daemon, gettext("storiqone, version: %s, build: %s %s"), STORIQONE_VERSION, __DATE__, __TIME__);
	so_log_write2(so_log_level_notice, so_log_type_daemon, gettext("storiqone (pid: %d, sid: %d)"), getpid(), getsid(0));
	so_log_write2(so_log_level_debug, so_log_type_daemon, gettext("Checksum: %s, last commit: %s"), STORIQONED_SRCSUM, STORIQONE_GIT_COMMIT);

	enum {
		OPT_CONFIG   = 'c',
		OPT_HELP     = 'h',
		OPT_PID_FILE = 'p',
		OPT_VERSION  = 'V',
	};

	static int option_index = 0;
	static struct option long_options[] = {
		{ "config",   1, NULL, OPT_CONFIG },
		{ "help",     0, NULL, OPT_HELP },
		{ "pid-file", 1, NULL, OPT_PID_FILE },
		{ "version",  0, NULL, OPT_VERSION },

		{ NULL, 0, NULL, 0 },
	};

	char * config_file = DAEMON_CONFIG_FILE;
	// char * pid_file = DAEMON_PID_FILE;

	// parse option
	int opt;
	do {
		opt = getopt_long(argc, argv, "c:hp:V", long_options, &option_index);

		switch (opt) {
			case -1:
				break;

			case OPT_CONFIG:
				so_log_write2(so_log_level_notice, so_log_type_daemon, gettext("Using configuration file: '%s' instead of '%s'"), optarg, config_file);
				config_file = optarg;
				break;

			case OPT_HELP:
				sod_show_help();
				return 0;

			case OPT_PID_FILE:
				// pid_file = optarg;
				// so_log_write_all(so_log_level_info, so_log_type_daemon, "Using pid file: '%s'", optarg);
				break;

			case OPT_VERSION:
				printf(gettext("storiqone, version: %s, build: %s %s\n"), STORIQONE_VERSION, __DATE__, __TIME__);
				printf(gettext("Checksum: %s, last commit: %s\n"), STORIQONED_SRCSUM, STORIQONE_GIT_COMMIT);
				return 0;

			default:
				return 1;
		}
	} while (opt > -1);

	so_log_write2(so_log_level_debug, so_log_type_daemon, gettext("Parsing options: OK"));

	struct so_value * config = so_json_parse_file(config_file);
	if (config == NULL)
		return 2;

	if (!sod_env_setup())
		return 3;

	struct so_value * logger_config = NULL;
	so_value_unpack(config, "{so}", "logger", &logger_config);
	if (logger_config == NULL || logger_config->type != so_value_hashtable)
		return 2;
	sod_logger_start(logger_config);

	struct so_value * log_file = NULL;
	so_value_unpack(logger_config, "{so}", "socket", &log_file);
	if (log_file == NULL || log_file->type != so_value_hashtable)
		return 2;
	so_log_configure(log_file, so_log_type_daemon);

	struct so_value * admin_config = NULL;
	so_value_unpack(config, "{so}", "admin", &admin_config);
	if (admin_config != NULL && admin_config->type == so_value_hashtable)
		sod_admin_config(admin_config);
	else
		so_log_write2(so_log_level_warning, so_log_type_daemon, gettext("Administration not configured"));

	struct so_value * db_configs = NULL;
	so_value_unpack(config, "{so}", "database", &db_configs);
	if (db_configs != NULL)
		so_database_load_config(db_configs);

	struct so_database * db = so_database_get_default_driver();
	if (db == NULL)
		return 3;

	struct so_database_config * db_config = db->ops->get_default_config();
	if (db_config == NULL)
		return 3;

	sod_plugin_sync_checksum(db_config);

	struct so_database_connection * connection = db_config->ops->connect(db_config);
	if (connection == NULL)
		return 3;

	struct so_value * default_values = so_value_new_null();
	so_value_unpack(config, "{so}", "default values", &default_values);

	so_host_init(connection);
	struct so_host * host_info = so_host_get_info();

	sod_plugin_sync_job(connection);
	sod_plugin_sync_scripts(connection);

	if (logger_config != NULL && db_configs != NULL)
		sod_device_configure(log_file, db_configs, default_values, connection, false);

	sod_scheduler_init();

	while (sod_daemon_run) {
		connection->ops->update_host(connection, host_info->uuid, STORIQONE_VERSION);
		sod_device_configure(log_file, db_configs, default_values, connection, true);

		sod_scheduler_do(log_file, db_configs, connection);

		so_poll(5000);
	}

	so_value_free(config);

	so_log_write2(so_log_level_notice, so_log_type_daemon, gettext("Daemon will shut down"));

	sod_device_stop();

	so_log_stop_logger();
	sod_logger_stop();

	return 0;
}

static void sod_show_help(void) {
	printf(gettext("storiqone, version: %s, build: %s %s\n"), STORIQONE_VERSION, __DATE__, __TIME__);
	printf(gettext("    --config=FILE,   -c : Uses this configuration FILE instead of “%s”\n"), DAEMON_CONFIG_FILE);
	printf(gettext("    --help,          -h : Shows this help and exits\n"));
	printf(gettext("    --pid-file=FILE, -p : Writes the pid of daemon into FILE instead of “%s”\n"), DAEMON_PID_FILE);
	printf(gettext("    --version,       -V : prints StoriqOne version number and exits\n"));
}

void sod_shutdown() {
	sod_daemon_run = false;
}

