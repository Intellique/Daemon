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

#include <stddef.h>

#include <libstone/log.h>
#include <libstone/value.h>

#include <libdatabase-postgresql.chcksum>

#include "common.h"

static struct st_database_config * st_database_postgresql_add(struct st_value * params);
static struct st_database_config * st_database_postgresql_get_default_config(void);
static void st_database_postgresql_init(void) __attribute__((constructor));


static struct st_database_ops st_database_postgresql_driver_ops = {
	.add	            = st_database_postgresql_add,
	.get_default_config = st_database_postgresql_get_default_config,
};

static struct st_database st_database_postgresql_driver = {
	.name = "postgresql",
	.ops  = &st_database_postgresql_driver_ops,

	.configurations = NULL,

	.cookie       = NULL,
	.api_level    = 0,
	.src_checksum = STONE_DATABASE_POSTGRESQL_SRCSUM,
};


static struct st_database_config * st_database_postgresql_add(struct st_value * params) {
	if (params == NULL)
		return NULL;

	struct st_database_config * config = st_database_postgresql_config_init(params);
	if (config != NULL) {
		config->driver = &st_database_postgresql_driver;

		struct st_value * vconfig = st_value_new_custom(config, st_database_postgresql_config_free);
		st_value_hashtable_put2(st_database_postgresql_driver.configurations, "default", vconfig, true);
		st_value_hashtable_put(st_database_postgresql_driver.configurations, st_value_new_string(config->name), true, vconfig, false);
	}

	return config;
}

static struct st_database_config * st_database_postgresql_get_default_config() {
	struct st_value * vconfig = st_value_hashtable_get2(st_database_postgresql_driver.configurations, "default", false);
	if (vconfig->type == st_value_custom)
		return st_value_custom_get(vconfig);
	return NULL;
}

static void st_database_postgresql_init() {
	st_database_postgresql_driver.configurations = st_value_new_hashtable2();
	st_database_register_driver(&st_database_postgresql_driver);
}

