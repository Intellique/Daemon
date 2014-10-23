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

#include <libstone/database.h>
#include <libstone/json.h>
#include <libstone/log.h>
#include <libstone/poll.h>
#include <libstone/value.h>

#include "admin.h"
#include "device.h"
#include "env.h"
#include "logger.h"
#include "main.h"
#include "scheduler.h"

#include "checksum/stoned.chcksum"
#include "config.h"
#include "stone.version"

static bool std_daemon_run = true;
static void std_show_help(void);

int main(int argc, char ** argv) {
	bindtextdomain("stoned", LOCALE_DIR);
	textdomain("stoned");

	st_log_write2(st_log_level_notice, st_log_type_daemon, gettext("stone, version: %s, build: %s %s"), STONE_VERSION, __DATE__, __TIME__);
	st_log_write2(st_log_level_notice, st_log_type_daemon, gettext("stone (pid: %d, sid: %d)"), getpid(), getsid(0));
	st_log_write2(st_log_level_debug, st_log_type_daemon, gettext("Checksum: %s, last commit: %s"), STONED_SRCSUM, STONE_GIT_COMMIT);

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
				st_log_write2(st_log_level_notice, st_log_type_daemon, gettext("Using configuration file: '%s' instead of '%s'"), optarg, config_file);
				config_file = optarg;
				break;

			case OPT_HELP:
				std_show_help();
				return 0;

			case OPT_PID_FILE:
				// pid_file = optarg;
				// st_log_write_all(st_log_level_info, st_log_type_daemon, "Using pid file: '%s'", optarg);
				break;

			case OPT_VERSION:
				printf(gettext("stone, version: %s, build: %s %s\n"), STONE_VERSION, __DATE__, __TIME__);
				printf(gettext("Checksum: %s, last commit: %s\n"), STONED_SRCSUM, STONE_GIT_COMMIT);
				return 0;

			default:
				return 1;
		}
	} while (opt > -1);

	st_log_write2(st_log_level_debug, st_log_type_daemon, gettext("Parsing option: ok"));

	struct st_value * config = st_json_parse_file(config_file);
	if (config == NULL)
		return 2;

	if (!std_env_setup())
		return 3;

	struct st_value * logger_config = NULL;
	st_value_unpack(config, "{so}", "logger", &logger_config);
	if (logger_config == NULL || logger_config->type != st_value_hashtable)
		return 2;
	std_logger_start(logger_config);

	struct st_value * log_file = st_value_hashtable_get2(logger_config, "socket", false, false);
	if (log_file == NULL || log_file->type != st_value_hashtable)
		return 2;
	st_log_configure(log_file, st_log_type_daemon);

	struct st_value * admin_config = st_value_hashtable_get2(config, "admin", false, false);
	if (admin_config != NULL && admin_config->type == st_value_hashtable)
		std_admin_config(admin_config);
	else
		st_log_write2(st_log_level_warning, st_log_type_daemon, gettext("No administration configured"));

	struct st_value * db_configs = st_value_hashtable_get2(config, "database", false, false);
	if (db_configs != NULL)
		st_database_load_config(db_configs);

	struct st_database * db = st_database_get_default_driver();
	if (db == NULL)
		return 3;

	struct st_database_config * db_config = db->ops->get_default_config();
	if (db_config == NULL)
		return 3;

	struct st_database_connection * connection = db_config->ops->connect(db_config);
	if (connection == NULL)
		return 3;

	if (logger_config != NULL && db_configs != NULL)
		std_device_configure(log_file, db_configs, connection);

	std_scheduler_init(connection);

	while (std_daemon_run) {
		std_scheduler_do(log_file, db_configs, connection);

		st_poll(5);
	}

	st_value_free(config);

	st_log_write2(st_log_level_notice, st_log_type_daemon, gettext("Daemon will shut down"));

	std_device_stop();

	st_log_stop_logger();
	std_logger_stop();

	return 0;
}

static void std_show_help(void) {
	printf(gettext("stone, version: %s, build: %s %s\n"), STONE_VERSION, __DATE__, __TIME__);
	printf(gettext("    --config,   -c : Read this config file instead of \"%s\"\n"), DAEMON_CONFIG_FILE);
	printf(gettext("    --help,     -h : Show this and exit\n"));
	printf(gettext("    --pid-file, -p : Write the pid of daemon into instead of \"%s\"\n"), DAEMON_PID_FILE);
	printf(gettext("    --version,  -V : Show the version of STone then exit\n"));
}

void std_shutdown() {
	std_daemon_run = false;
}

