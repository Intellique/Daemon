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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// bindtextdomain
#include <libintl.h>
// NULL
#include <stddef.h>

#include <libstoriqone/log.h>
#include <libstoriqone/value.h>

#include <libdatabase-postgresql.chcksum>

#include "common.h"

#include "config.h"

static struct so_database_config * so_database_postgresql_add(struct so_value * params);
static struct so_database_config * so_database_postgresql_get_default_config(void);
static void so_database_postgresql_init(void) __attribute__((constructor));


static struct so_database_ops so_database_postgresql_driver_ops = {
	.add	            = so_database_postgresql_add,
	.get_default_config = so_database_postgresql_get_default_config,
};

static struct so_database so_database_postgresql_driver = {
	.name = "postgresql",
	.ops  = &so_database_postgresql_driver_ops,

	.configurations = NULL,

	.cookie       = NULL,
	.src_checksum = STORIQONE_DATABASE_POSTGRESQL_SRCSUM,
};


static struct so_database_config * so_database_postgresql_add(struct so_value * params) {
	if (params == NULL)
		return NULL;

	struct so_database_config * config = so_database_postgresql_config_init(params);
	if (config != NULL) {
		config->driver = &so_database_postgresql_driver;

		struct so_value * vconfig = so_value_new_custom(config, so_database_postgresql_config_free);
		so_value_hashtable_put2(so_database_postgresql_driver.configurations, "default", vconfig, true);
		so_value_hashtable_put(so_database_postgresql_driver.configurations, so_value_new_string(config->name), true, vconfig, false);
	}

	return config;
}

static struct so_database_config * so_database_postgresql_get_default_config() {
	struct so_value * vconfig = so_value_hashtable_get2(so_database_postgresql_driver.configurations, "default", false, false);
	if (vconfig->type == so_value_custom)
		return so_value_custom_get(vconfig);
	return NULL;
}

static void so_database_postgresql_init() {
	bindtextdomain("libstoriqone-database-postgresql", LOCALE_DIR);

	so_database_postgresql_driver.configurations = so_value_new_hashtable2();
	so_database_register_driver(&so_database_postgresql_driver);
}

