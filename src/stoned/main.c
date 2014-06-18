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

#include "checksum/stoned.chcksum"
#include "config.h"
#include "stone.version"

static bool std_daemon_run = true;
static void std_show_help(void);

int main(int argc, char ** argv) {
	st_log_write2(st_log_level_notice, st_log_type_daemon, "stone, version: " STONE_VERSION ", build: " __DATE__ " " __TIME__);
	st_log_write2(st_log_level_notice, st_log_type_daemon, "stone (pid: %d, sid: %d)", getpid(), getsid(0));
	st_log_write2(st_log_level_debug, st_log_type_daemon, "Checksum: " STONED_SRCSUM ", last commit: " STONE_GIT_COMMIT);

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
				config_file = optarg;
				st_log_write2(st_log_level_notice, st_log_type_daemon, "Using configuration file: '%s'", optarg);
				break;

			case OPT_HELP:
				std_show_help();
				return 0;

			case OPT_PID_FILE:
				// pid_file = optarg;
				// st_log_write_all(st_log_level_info, st_log_type_daemon, "Using pid file: '%s'", optarg);
				break;

			case OPT_VERSION:
				printf("STone, version: " STONE_VERSION ", build: " __DATE__ " " __TIME__ "\n");
				printf("Checksum: " STONED_SRCSUM ", last commit: " STONE_GIT_COMMIT "\n");
				return 0;

			default:
				return 1;
		}
	} while (opt > -1);

	st_log_write2(st_log_level_debug, st_log_type_daemon, "Parsing option: ok");

	struct st_value * config = st_json_parse_file(config_file);
	if (config == NULL)
		return 2;

	if (!std_env_setup())
		return 3;

	struct st_value * logger_config = st_value_hashtable_get2(config, "logger", false, false);
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
		st_log_write2(st_log_level_warning, st_log_type_daemon, "No administration configured");

	struct st_value * db_configs = st_value_hashtable_get2(config, "database", false, false);
	if (db_configs != NULL)
		st_database_load_config(db_configs);

	if (logger_config != NULL && db_configs != NULL)
		std_device_configure(logger_config, db_configs);

	while (std_daemon_run) {
		st_poll(-1);
	}

	st_value_free(config);

	st_log_write2(st_log_level_notice, st_log_type_daemon, "Daemon will shut down");

	std_device_stop();

	st_log_stop_logger();
	std_logger_stop();

	return 0;
}

static void std_show_help(void) {
	printf("STone, version: " STONE_VERSION ", build: " __DATE__ " " __TIME__ "\n");
	printf("    --config,   -c : Read this config file instead of \"" DAEMON_CONFIG_FILE "\"\n");
	printf("    --help,     -h : Show this and exit\n");
	printf("    --pid-file, -p : Write the pid of daemon into instead of \"" DAEMON_PID_FILE "\"\n");
	printf("    --version,  -V : Show the version of STone then exit\n");
}

void std_shutdown() {
	std_daemon_run = false;
}

