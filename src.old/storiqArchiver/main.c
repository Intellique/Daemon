/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2010, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Mon, 25 Oct 2010 13:00:00 +0200                       *
\***********************************************************************/

// getopt_long
#include <getopt.h>
// printf
#include <stdio.h>
// strrchr
#include <string.h>

#include <storiqArchiver/log.h>

#include "conf.h"
#include "config.h"
#include "scheduler.h"

void showHelp(char * command);

int main(int argc, char ** argv) {
	log_writeAll(Log_level_info, "StorIqArchiver, version: %s, build: %s %s", STORIQARCHIVER_VERSION, __DATE__, __TIME__);

	static int option_index = 0;
	static struct option long_options[] = {
		{"config",		1,	0,	'c'},
		{"detach",		0,	0,	'd'},
		{"help",		0,	0,	'h'},
		{"pid-file",	1,	0,	'p'},
		{"version",		0,	0,	'V'},

		{0, 0, 0, 0},
	};

	char * config_file = DEFAULT_CONFIG_FILE;
	short detach = 0;
	char * pid_file = DEFAULT_PID_FILE;

	// parse option
	for (;;) {
		int c = getopt_long(argc, argv, "c:dhp:V", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 'c':
				config_file = optarg;
				log_writeAll(Log_level_info, "Using configuration file: '%s'", optarg);
				break;

			case 'd':
				detach = 1;
				log_writeAll(Log_level_info, "Using detach mode (i.e. use fork())");
				break;

			case 'h':
				showHelp(*argv);
				return 0;

			case 'p':
				pid_file = optarg;
				log_writeAll(Log_level_info, "Using pid file: '%s'", optarg);
				break;

			case 'V': {
					char * ptr = strrchr(*argv, '/');
					if (ptr)
						ptr++;
					else
						ptr = *argv;

					printf("%s\nVersion: %s, build: %s %s\n", ptr, STORIQARCHIVER_VERSION, __DATE__, __TIME__);
				}
				return 0;

			default:
				log_writeAll(Log_level_error, "Parsing option: unknown option '%c'", c);
				showHelp(*argv);
				return 1;
		}
	}

	log_writeAll(Log_level_info, "Parsing option: ok");

	// check pid file
	int pid = conf_readPid(pid_file);
	if (pid >= 0) {
		int code = conf_checkPid(pid);
		switch (code) {
			case -1:
				printf("Warning: another process used this pid (%d)\n", pid);
				break;

			case 0:
				printf("Info: daemon is dead, long life the daemon\n");
				break;

			case 1:
				printf("Error: Daemon is alive (pid: %d)\n", pid);
				return 2;
		}
	}

	// create daemon
	if (detach) {}

	// read configuration
	if (conf_readConfig(config_file)) {
		printf("Error while parsing '%s'\n", config_file);
		return 3;
	}

	sched_doLoop();

	log_writeAll(Log_level_info, "StorIqArchiver exit");

	return 0;
}

void showHelp(char * command) {
	char * ptr = strrchr(command, '/');
	if (ptr)
		ptr++;
	else
		ptr = command;

	printf("%s, version: %s, build: %s %s\n", ptr, STORIQARCHIVER_VERSION, __DATE__, __TIME__);
	printf("    --config,   -c : Read this config file instead of \"%s\"\n", DEFAULT_CONFIG_FILE);
	printf("    --detach,   -d : Daemonize it\n");
	printf("    --help,     -h : Show this and exit\n");
	printf("    --pid-file, -p : Write the pid of daemon into instead of \"%s\"\n", DEFAULT_PID_FILE);
	printf("    --version,  -V : Show the version of StorIqArchiver then exit\n");
}

