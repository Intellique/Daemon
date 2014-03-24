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
*  Last modified: Wed, 18 Dec 2013 23:21:03 +0100                            *
\****************************************************************************/

// getopt_long
#include <getopt.h>
// printf
#include <stdio.h>
// strchr
#include <string.h>
// uname
#include <sys/utsname.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>

#include <libstone/conf.h>
#include <libstone/database.h>
#include <libstone/log.h>

#include "config.h"
#include "scan.h"
#include "stone.version"

static void st_show_help(void);

int main(int argc, char ** argv) {
	st_log_write_all(st_log_level_info, st_log_type_daemon, "STone config, version: " STONE_VERSION ", build: " __DATE__ " " __TIME__);

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
	struct st_database * db = st_database_get_default_driver();
	if (db == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Fatal error: There is no database into config file '%s'", config_file);
		printf("Fatal error: There is no database into config file '%s'", config_file);
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
		return 6;
	}

	// check hostname in database
	static struct utsname name;
	uname(&name);

	if (!connect->ops->find_host(connect, NULL, name.nodename)) {
		st_log_write_all(st_log_level_warning, st_log_type_daemon, "Host '%s' not found into database", name.nodename);

		uuid_t id;
		static char uuid[37];

		uuid_generate(id);
		uuid_unparse_lower(id, uuid);

		char * domaine = strchr(name.nodename, '.');
		if (domaine != NULL) {
			*domaine = '\0';
			domaine++;
		}

		int failed = connect->ops->add_host(connect, uuid, name.nodename, domaine, NULL);

		if (domaine != NULL) {
			domaine--;
			*domaine = '.';
		}

		if (failed != 0)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Failed to add host '%s'", name.nodename);
		else
			st_log_write_all(st_log_level_warning, st_log_type_daemon, "Host '%s' has been inserted into database", name.nodename);
	}

	connect->ops->close(connect);
	connect->ops->free(connect);

	if (verbose > 0)
		printf("Detect hardware configuration\n");

	stcfg_scan();

	st_log_write_all(st_log_level_info, st_log_type_daemon, "stone-config finished");

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
