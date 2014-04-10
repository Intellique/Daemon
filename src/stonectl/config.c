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
// glob, globfree
#include <glob.h>
// printf
#include <stdio.h>
// strchr
#include <string.h>
// uname
#include <sys/utsname.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>

#include <libstone/database.h>
#include <libstone/json.h>
#include <libstone/value.h>

#include "common.h"
#include "config.h"
#include "hardware.h"

enum {
	OPT_HELP = 'h',
};

int stctl_config(int argc, char ** argv) {
	int option_index = 0;
	static struct option long_options[] = {
		{ "help",     0, NULL, OPT_HELP },

		{ NULL, 0, NULL, 0 },
	};

	char * config_file = DAEMON_CONFIG_FILE;

	// parse option
	int opt;
	optind = 0;
	do {
		opt = getopt_long(argc, argv, "h", long_options, &option_index);

		switch (opt) {
			case -1:
				break;

			case OPT_HELP:
				printf("stonectl config : basic configuration\n");
				printf("  -h, --help : show this and exit\n");
				return 0;

			default:
				return 1;
		}
	} while (opt > -1);

	struct st_value * config = st_json_parse_file(config_file);
	if (config == 0)
		return 1;

	struct st_value * db_configs = st_value_hashtable_get2(config, "database", false);
	if (db_configs != NULL)
		st_database_load_config(db_configs);

	struct st_database * db_driver = st_database_get_default_driver();
	if (db_driver == NULL)
		return 2;

	struct st_database_config * db_config = db_driver->ops->get_default_config();
	if (db_config == NULL)
		return 3;

	struct st_database_connection * db_connection = db_config->ops->connect(db_config);
	if (db_connection == NULL)
		return 4;

	struct utsname name;
	uname(&name);

	if (!db_connection->ops->find_host(db_connection, NULL, name.nodename)) {
		uuid_t id;
		static char uuid[37];

		uuid_generate(id);
		uuid_unparse_lower(id, uuid);

		char * domaine = strchr(name.nodename, '.');
		if (domaine != NULL) {
			*domaine = '\0';
			domaine++;
		}

		db_connection->ops->add_host(db_connection, uuid, name.nodename, domaine, NULL);
	}

	struct st_value * changers = stctl_detect_hardware();

	return 0;
}

