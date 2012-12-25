/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Tue, 25 Dec 2012 20:13:18 +0100                         *
\*************************************************************************/

// getopt_long
#include <getopt.h>
// printf
#include <stdio.h>
// daemon
#include <unistd.h>

#include <libstone/checksum.h>
#include <libstone/conf.h>
#include <libstone/database.h>
#include <libstone/job.h>
#include <libstone/log.h>
#include <libstone/util/file.h>
#include <stoned/library/changer.h>

#include "checksum/stoned.chcksum"
#include "config.h"
#include "library/common.h"
#include "stone.version"
#include "scheduler.h"

static void st_show_help(void);

int main(int argc, char ** argv) {
	st_log_write_all(st_log_level_info, st_log_type_daemon, "STone, version: " STONE_VERSION ", build: " __DATE__ " " __TIME__);
	st_log_write_all(st_log_level_debug, st_log_type_daemon, "Checksum: " STONED_SRCSUM ", last commit: " STONE_GIT_COMMIT);

	enum {
		OPT_CONFIG   = 'c',
		OPT_DETACH   = 'd',
		OPT_HELP     = 'h',
		OPT_PID_FILE = 'p',
		OPT_VERSION  = 'V',
	};

	static int option_index = 0;
	static struct option long_options[] = {
		{ "config",   1, NULL, OPT_CONFIG },
		{ "detach",   0, NULL, OPT_DETACH },
		{ "help",     0, NULL, OPT_HELP },
		{ "pid-file", 1, NULL, OPT_PID_FILE },
		{ "version",  0, NULL, OPT_VERSION },

		{ NULL, 0, NULL, 0 },
	};

	char * config_file = DAEMON_CONFIG_FILE;
	short detach = 0;
	char * pid_file = DAEMON_PID_FILE;

	// parse option
	int opt;
	do {
		opt = getopt_long(argc, argv, "c:dhp:V", long_options, &option_index);

		switch (opt) {
			case -1:
				break;

			case OPT_CONFIG:
				config_file = optarg;
				st_log_write_all(st_log_level_info, st_log_type_daemon, "Using configuration file: '%s'", optarg);
				break;

			case OPT_DETACH:
				detach = 1;
				st_log_write_all(st_log_level_info, st_log_type_daemon, "Using detach mode (i.e. use fork())");
				break;

			case OPT_HELP:
				st_log_disable_display_log();

				st_show_help();
				return 0;

			case OPT_PID_FILE:
				pid_file = optarg;
				st_log_write_all(st_log_level_info, st_log_type_daemon, "Using pid file: '%s'", optarg);
				break;

			case OPT_VERSION:
				st_log_disable_display_log();

				printf("STone, version: " STONE_VERSION ", build: " __DATE__ " " __TIME__ "\n");
				printf("Checksum: " STONED_SRCSUM ", last commit: " STONE_GIT_COMMIT "\n");
				return 0;

			default:
				st_log_write_all(st_log_level_error, st_log_type_daemon, "Unsupported parameter '%d : %s'", opt, optarg);
				return 1;
		}
	} while (opt > -1);

	st_log_write_all(st_log_level_debug, st_log_type_daemon, "Parsing option: ok");

	// check pid file
	int pid = st_conf_read_pid(pid_file);
	if (pid >= 0) {
		int code = st_conf_check_pid(*argv, pid);
		switch (code) {
			case -1:
				st_log_write_all(st_log_level_warning, st_log_type_daemon, "Daemon: another process used this pid (%d)", pid);
				break;

			case 0:
				st_log_write_all(st_log_level_info, st_log_type_daemon, "Daemon: daemon is dead, long life the daemon");
				break;

			case 1:
				st_log_write_all(st_log_level_error, st_log_type_daemon, "Fatal error: daemon is alive (pid: %d)", pid);
				return 2;
		}
	}

	// create daemon
	if (detach) {
		if (!daemon(0, 0)) {
			pid_t pid = getpid();
			st_conf_write_pid(pid_file, pid);
		} else {
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Fatal error: Failed to daemonize");
			return 3;
		}
	} else {
		pid_t pid = getpid();
		st_conf_write_pid(pid_file, pid);
	}

	// read configuration
	if (st_conf_read_config(config_file)) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Error while parsing '%s'", config_file);
		return 4;
	}

	// check if config file contains a database
	struct st_database * db = st_database_get_default_driver();
	if (db == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Fatal error: There is no database into config file '%s'", config_file);
		return 5;
	}

	struct st_database_config * config = NULL;
	struct st_database_connection * connect = NULL;

	if (db != NULL)
		config = db->ops->get_default_config();
	if (config != NULL)
		connect = config->ops->connect(config);

	if (connect == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Fatal error: Failed to connect to database ");
		return 5;
	}

	// synchronize checksum plugins
	st_checksum_sync_plugins(connect);

	// synchronize job plugins
	st_job_sync_plugins(connect);

	if (st_changer_setup())
		return 6;

	// start remote admin
	//st_admin_start();

	st_sched_do_loop(connect);

	// remove pid file
	st_util_file_rm(pid_file);

	// stop libraries
	st_changer_stop();

	st_log_write_all(st_log_level_info, st_log_type_daemon, "STone exit");

	st_log_stop_logger();

	connect->ops->free(connect);

	usleep(250);

	return 0;
}

static void st_show_help(void) {
	st_log_disable_display_log();

	printf("STone, version: " STONE_VERSION ", build: " __DATE__ " " __TIME__ "\n");
	printf("    --config,   -c : Read this config file instead of \"" DAEMON_CONFIG_FILE "\"\n");
	printf("    --detach,   -d : Daemonize it\n");
	printf("    --help,     -h : Show this and exit\n");
	printf("    --pid-file, -p : Write the pid of daemon into instead of \"" DAEMON_PID_FILE "\"\n");
	printf("    --version,  -V : Show the version of STone then exit\n");
}

