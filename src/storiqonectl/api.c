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

#include <stddef.h>

// getopt_long
#include <getopt.h>
// gettext
#include <libintl.h>
// printf
#include <stdio.h>

#include <libstoriqone/application.h>
#include <libstoriqone/database.h>
#include <libstoriqone/json.h>
#include <libstoriqone/value.h>

#include "common.h"
#include "config.h"

enum {
	OPT_CREATE = 'c',
	OPT_HELP = 'h',
	OPT_LIST = 'l',
	OPT_REMOVE = 'r',
};

int soctl_api(int argc, char ** argv) {
	int option_index = 0;
	static struct option long_options[] = {
		{ "create", 1, NULL, OPT_HELP },
		{ "help",   0, NULL, OPT_HELP },
		{ "list",   0, NULL, OPT_LIST },
		{ "remove", 1, NULL, OPT_REMOVE },

		{ NULL, 0, NULL, 0 },
	};

	enum {
		api_key_create,
		api_key_default,
		api_key_list,
		api_key_remove,
	} action = api_key_default;
	const char * application = NULL;

	char * config_file = DAEMON_CONFIG_FILE;

	// parse option
	int opt;
	optind = 0;
	do {
		opt = getopt_long(argc, argv, "c:hlr:", long_options, &option_index);

		switch (opt) {
			case -1:
				break;

			case OPT_CREATE:
				if (action != api_key_default) {
					printf(gettext("Too manu action specified, you should specify only one [-c|-l|-r]\n"));
					return 1;
				}

				action = api_key_create;
				application = optarg;
				break;

			case OPT_HELP:
				printf(gettext("storiqonectl api : manage API keys\n"));
				printf(gettext("  -c, --create <name> : create new API key\n"));
				printf(gettext("  -h, --help          : show this and exit\n"));
				printf(gettext("  -l, --list          : list API keys (default action)\n"));
				printf(gettext("  -r, --remove <name> : remove a key\n"));
				return 0;

			case OPT_LIST:
				if (action != api_key_default) {
					printf(gettext("Too manu action specified, you should specify only one [-c|-l|-r]\n"));
					return 1;
				}

				action = api_key_list;
				break;

			case OPT_REMOVE:
				if (action != api_key_default) {
					printf(gettext("Too manu action specified, you should specify only one [-c|-l|-r]\n"));
					return 1;
				}

				action = api_key_remove;
				application = optarg;
				break;

			default:
				return 1;
		}
	} while (opt > -1);

	struct so_value * config = so_json_parse_file(config_file);
	if (config == 0) {
		printf(gettext("Failed to parse configuration file '%s'\n"), config_file);
		printf(gettext("or configuration file is not a valid JSON file\n"));
		return 2;
	}

	struct so_value * db_configs = so_value_hashtable_get2(config, "database", false, false);
	if (db_configs != NULL)
		so_database_load_config(db_configs);
	else {
		printf(gettext("No database configuration found in '%s'\n"), config_file);
		return 3;
	}

	struct so_database * db_driver = so_database_get_default_driver();
	if (db_driver == NULL) {
		printf(gettext("Failed to load database plugin\n"));
		return 3;
	}

	struct so_database_config * db_config = db_driver->ops->get_default_config();
	if (db_config == NULL) {
		printf(gettext("Database configuration is incorrect\n"));
		return 3;
	}

	struct so_database_connection * db_connection = db_config->ops->connect(db_config);
	if (db_connection == NULL) {
		printf(gettext("Connection to database: failure\n"));
		return 3;
	}

	switch (action) {
		case api_key_create: {
			struct so_application * app = db_connection->ops->api_key_create(db_connection, application);
			if (app != NULL) {
				printf(gettext("New key '%s' has been created\n"), app->api_key);
				return 0;
			} else {
				printf(gettext("Failed to create new key\n"));
				return 4;
			}
		}

		default:
		case api_key_list: {
			unsigned int i, nb_keys = 0;
			struct so_application * apps = db_connection->ops->api_key_list(db_connection, &nb_keys);

			if (apps == NULL) {
				printf(gettext("Failed to retreive keys\n"));
				return 4;
			}

			printf(ngettext("There is %u api key\n", "There is %u api keys\n", nb_keys), nb_keys);
			for (i = 0; i < nb_keys; i++)
				printf("#%u: App: %s, key: %s\n", i + 1, apps[i].application_name, apps[i].api_key);

			return 0;
		}

		case api_key_remove: {
			bool ok = db_connection->ops->api_key_remove(db_connection, application);
			if (ok) {
				printf(gettext("API key '%s' has been removed\n"), application);
				return 0;
			} else {
				printf(gettext("Failed to remove API key for '%s'\n"), application);
				return 4;
			}
		}
	}
}

