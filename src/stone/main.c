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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sun, 29 Jan 2012 15:42:09 +0100                         *
\*************************************************************************/

// getopt_long
#include <getopt.h>
// printf
#include <stdio.h>
// daemon
#include <unistd.h>

#include <stone/database.h>
#include <stone/library/changer.h>
#include <stone/job.h>

#include "admin.h"
#include "checksum.h"
#include "conf.h"
#include "config.h"
#include "log.h"
#include "scheduler.h"

void st_test(void);

static void st_show_help(void);

int main(int argc, char ** argv) {
	st_log_write_all(st_log_level_info, st_log_type_daemon, "STone, version: %s, build: %s %s", STONE_VERSION, __DATE__, __TIME__);

	enum {
		OPT_CONFIG   = 'c',
		OPT_DETACH   = 'd',
		OPT_HELP     = 'h',
		OPT_PID_FILE = 'p',
		OPT_VERSION  = 'V',
	};

	static int option_index = 0;
	static struct option long_options[] = {
		{ "config",   1, 0, OPT_CONFIG },
		{ "detach",   0, 0, OPT_DETACH },
		{ "help",     0, 0, OPT_HELP },
		{ "pid-file", 1, 0, OPT_PID_FILE },
		{ "version",  0, 0, OPT_VERSION },

		{0, 0, 0, 0},
	};

	char * config_file = DEFAULT_CONFIG_FILE;
	short detach = 0;
	char * pid_file = DEFAULT_PID_FILE;

	// parse option
	int opt;
	do {
		opt = getopt_long(argc, argv, "c:dhp:V", long_options, &option_index);

		switch (opt) {
			case OPT_CONFIG:
				config_file = optarg;
				st_log_write_all(st_log_level_info, st_log_type_daemon, "Using configuration file: '%s'", optarg);
				break;

			case OPT_DETACH:
				detach = 1;
				st_log_write_all(st_log_level_info, st_log_type_daemon, "Using detach mode (i.e. use fork())");
				break;

			case OPT_HELP:
				st_show_help();
				return 0;

			case OPT_PID_FILE:
				pid_file = optarg;
				st_log_write_all(st_log_level_info, st_log_type_daemon, "Using pid file: '%s'", optarg);
				break;

			case OPT_VERSION:
				st_log_disable_display_log();

				printf("STone, version: %s, build: %s %s\n", STONE_VERSION, __DATE__, __TIME__);
				return 0;
		}
	} while (opt > -1);

	st_log_write_all(st_log_level_debug, st_log_type_daemon, "Parsing option: ok");

	// check pid file
	int pid = st_conf_read_pid(pid_file);
	if (pid >= 0) {
		int code = st_conf_check_pid(pid);
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
	if (!st_db_get_default_db()) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Fatal error: There is no database into config file '%s'", config_file);
		return 5;
	}

	// synchronize checksum plugins
	// st_checksum_sync_plugins();
	// synchronize job plugins
	st_job_sync_plugins();

	if (st_changer_setup())
		return 6;

	// start remote admin
	st_admin_start();

	st_sched_do_loop();

	//st_test();

	st_log_write_all(st_log_level_info, st_log_type_daemon, "STone exit");

	return 0;
}

void st_show_help() {
	st_log_disable_display_log();

	printf("STone, version: %s, build: %s %s\n", STONE_VERSION, __DATE__, __TIME__);
	printf("    --config,   -c : Read this config file instead of \"%s\"\n", DEFAULT_CONFIG_FILE);
	printf("    --detach,   -d : Daemonize it\n");
	printf("    --help,     -h : Show this and exit\n");
	printf("    --pid-file, -p : Write the pid of daemon into instead of \"%s\"\n", DEFAULT_PID_FILE);
	printf("    --version,  -V : Show the version of STone then exit\n");
}

