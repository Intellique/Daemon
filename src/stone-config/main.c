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
*  Last modified: Wed, 22 Aug 2012 20:31:35 +0200                         *
\*************************************************************************/

// getopt_long
#include <getopt.h>
// printf
#include <stdio.h>

#include <libstone/conf.h>
#include <libstone/database.h>
#include <libstone/log.h>

#include "config.h"
#include "scan.h"
#include "stone.version"

static void st_show_help(void);

int main(int argc, char ** argv) {
	st_log_write_all(st_log_level_info, st_log_type_daemon, "STone config, version: %s, build: %s %s", STONE_VERSION, __DATE__, __TIME__);

	enum {
		OPT_CONFIG  = 'c',
		OPT_HELP    = 'h',
		OPT_VERBOSE = 'v',
		OPT_VERSION = 'V',
	};

	static int option_index = 0;
	static struct option long_options[] = {
		{ "config",  1, NULL, OPT_CONFIG },
		{ "help",    0, NULL, OPT_HELP },
		{ "verbose", 0, NULL, OPT_VERBOSE },
		{ "version", 0, NULL, OPT_VERSION },

		{NULL, 0, NULL, 0},
	};

	char * config_file = DAEMON_CONFIG_FILE;
	short verbose = 0;

	// parse option
	int opt;
	do {
		opt = getopt_long(argc, argv, "c:hvV", long_options, &option_index);

		switch (opt) {
			case -1:
				break;

			case OPT_CONFIG:
				config_file = optarg;
				st_log_write_all(st_log_level_info, st_log_type_daemon, "Using configuration file: '%s'", optarg);
				break;

			case OPT_HELP:
				st_log_disable_display_log();

				st_show_help();
				return 0;

			case OPT_VERBOSE:
				if (verbose < 2)
					verbose++;
				break;

			case OPT_VERSION:
				st_log_disable_display_log();

				printf("STone config, version: " STONE_VERSION ", build: " __DATE__ " " __TIME__ "\n");
				return 0;

			default:
				st_log_write_all(st_log_level_error, st_log_type_daemon, "Unsupported parameter '%d : %s'", opt, optarg);
				return 1;
		}
	} while (opt > -1);

	st_log_write_all(st_log_level_debug, st_log_type_daemon, "Parsing option: ok");
	if (verbose > 1)
		printf("Parsing option ok\n");

	// read configuration
	if (st_conf_read_config(config_file)) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Error while parsing '%s'", config_file);
		printf("Error while parsing '%s'", config_file);
		return 4;
	}

	// check if config file contains a database
	if (!st_database_get_default_driver()) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Fatal error: There is no database into config file '%s'", config_file);
		printf("Fatal error: There is no database into config file '%s'", config_file);
		return 5;
	}

	if (verbose > 0)
		printf("Detect hardware configuration\n");

	stcfg_scan();

	st_log_write_all(st_log_level_info, st_log_type_daemon, "STone finished");

	st_log_stop_logger();

	return 0;
}

static void st_show_help(void) {
	st_log_disable_display_log();

	printf("STone, version: " STONE_VERSION ", build: " __DATE__ " " __TIME__ "\n");
	printf("    --config,   -c : Read this config file instead of \"" DAEMON_CONFIG_FILE "\"\n");
	printf("    --help,     -h : Show this and exit\n");
	printf("    --version,  -V : Show the version of STone then exit\n");
}

