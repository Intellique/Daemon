/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
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
*  Last modified: Sat, 17 Dec 2011 17:21:31 +0100                         *
\*************************************************************************/

// getopt_long
#include <getopt.h>
// printf
#include <stdio.h>
// strrchr
#include <string.h>

#include <stone/conf.h>
#include <stone/library/changer.h>

#include "config.h"
#include "log.h"
#include "scheduler.h"

static void sa_show_help(char * command);

int main(int argc, char ** argv) {
	sa_log_write_all(sa_log_level_info, "StorIqArchiver, version: %s, build: %s %s", STONE_VERSION, __DATE__, __TIME__);

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
				sa_log_write_all(sa_log_level_info, "Using configuration file: '%s'", optarg);
				break;

            case OPT_DETACH:
				detach = 1;
				sa_log_write_all(sa_log_level_info, "Using detach mode (i.e. use fork())");
				break;

            case OPT_HELP:
				sa_show_help(*argv);
				return 0;

            case OPT_PID_FILE:
				pid_file = optarg;
				sa_log_write_all(sa_log_level_info, "Using pid file: '%s'", optarg);
				break;

            case OPT_VERSION: {
					char * ptr = strrchr(*argv, '/');
					if (ptr)
						ptr++;
					else
						ptr = *argv;

					printf("%s\nVersion: %s, build: %s %s\n", ptr, STONE_VERSION, __DATE__, __TIME__);
				}
				return 0;
		}
	} while (opt > -1);

	sa_log_write_all(sa_log_level_debug, "Parsing option: ok");

	// check pid file
	int pid = sa_conf_read_pid(pid_file);
	if (pid >= 0) {
		int code = sa_conf_check_pid(pid);
		switch (code) {
			case -1:
				sa_log_write_all(sa_log_level_error, "Warning: another process used this pid (%d)", pid);
				break;

			case 0:
				sa_log_write_all(sa_log_level_error, "Info: daemon is dead, long life the daemon");
				break;

			case 1:
				sa_log_write_all(sa_log_level_error, "Error: Daemon is alive (pid: %d)", pid);
				return 2;
		}
	}

	// create daemon
	if (detach) {}

	// read configuration
	if (sa_conf_read_config(config_file)) {
		sa_log_write_all(sa_log_level_error, "Error while parsing '%s'", config_file);
		return 3;
	}

    sa_changer_setup();

	// sa_sched_do_loop();

	sa_log_write_all(sa_log_level_info, "StorIqArchiver exit");

	return 0;
}

void sa_show_help(char * command) {
	char * ptr = strrchr(command, '/');
	if (ptr)
		ptr++;
	else
		ptr = command;

	sa_log_disable_display_log();

	printf("%s, version: %s, build: %s %s\n", ptr, STONE_VERSION, __DATE__, __TIME__);
	printf("    --config,   -c : Read this config file instead of \"%s\"\n", DEFAULT_CONFIG_FILE);
	printf("    --detach,   -d : Daemonize it\n");
	printf("    --help,     -h : Show this and exit\n");
	printf("    --pid-file, -p : Write the pid of daemon into instead of \"%s\"\n", DEFAULT_PID_FILE);
	printf("    --version,  -V : Show the version of StorIqArchiver then exit\n");
}

