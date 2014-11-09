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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// getopt_long
#include <getopt.h>
// glob, globfree
#include <glob.h>
// gettext
#include <libintl.h>
// printf
#include <stdio.h>
// strchr
#include <string.h>
// uname
#include <sys/utsname.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>

#include <libstoriqone/database.h>
#include <libstoriqone/json.h>
#include <libstoriqone/value.h>

#include "common.h"
#include "config.h"
#include "hardware.h"

enum {
	OPT_HELP = 'h',
};

int soctl_config(int argc, char ** argv) {
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
				printf(gettext("storiqonectl config : basic configuration\n"));
				printf(gettext("  -h, --help : show this and exit\n"));
				return 0;

			default:
				return 1;
		}
	} while (opt > -1);

	struct so_value * config = so_json_parse_file(config_file);
	if (config == 0)
		return 1;

	struct so_value * db_configs = so_value_hashtable_get2(config, "database", false, false);
	if (db_configs != NULL)
		so_database_load_config(db_configs);

	struct so_database * db_driver = so_database_get_default_driver();
	if (db_driver == NULL)
		return 2;

	struct so_database_config * db_config = db_driver->ops->get_default_config();
	if (db_config == NULL)
		return 3;

	struct so_database_connection * db_connection = db_config->ops->connect(db_config);
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

	struct so_value * changers = soctl_detect_hardware();

	int failed = 0;
	struct so_value_iterator * iter = so_value_list_get_iterator(changers);
	while (failed == 0 && so_value_iterator_has_next(iter)) {
		struct so_value * val_changer = so_value_iterator_get_value(iter, false);
		struct so_changer * changer = so_value_custom_get(val_changer);
		failed = db_connection->ops->sync_changer(db_connection, changer, so_database_sync_init);
	}
	so_value_iterator_free(iter);

	db_connection->ops->close(db_connection);

	so_value_free(changers);
	so_value_free(config);

	return failed;
}

