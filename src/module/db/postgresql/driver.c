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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 24 Dec 2012 19:12:05 +0100                         *
\*************************************************************************/

// realloc
#include <malloc.h>
// strdup
#include <string.h>

#include <libstone/log.h>

#include "common.h"

static struct st_database_config * st_db_postgresql_add(const struct st_hashtable * params);
static struct st_database_config * st_db_postgresql_get_default_config(void);
static void st_db_postgresql_init(void) __attribute__((constructor));


static struct st_database_ops st_db_postgresql_driver_ops = {
	.add	            = st_db_postgresql_add,
	.get_default_config = st_db_postgresql_get_default_config,
};

static struct st_database st_db_postgresql_driver = {
	.name      = "postgresql",

	.ops       = &st_db_postgresql_driver_ops,

	.configurations    = NULL,
	.nb_configurations = 0,

	.cookie    = NULL,
	.api_level      = {
		.checksum = 0,
		.database = STONE_DATABASE_API_LEVEL,
		.job      = 0,
	},
};


static struct st_database_config * st_db_postgresql_add(const struct st_hashtable * params) {
	if (params == NULL)
		return NULL;

	void * new_addr = realloc(st_db_postgresql_driver.configurations, (st_db_postgresql_driver.nb_configurations + 1) * sizeof(struct st_database_config));
	if (new_addr == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Not enought memory to add new config");
		return NULL;
	}

	st_db_postgresql_driver.configurations = new_addr;
	struct st_database_config * config = st_db_postgresql_driver.configurations + st_db_postgresql_driver.nb_configurations;

	if (st_db_postgresql_config_init(config, params)) {
		st_db_postgresql_driver.configurations = realloc(st_db_postgresql_driver.configurations, st_db_postgresql_driver.nb_configurations * sizeof(struct st_database_config));
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Not enought memory to add new config");
		return NULL;
	} else {
		st_db_postgresql_driver.nb_configurations++;
		config->driver = &st_db_postgresql_driver;
		return config;
	}
}

static struct st_database_config * st_db_postgresql_get_default_config(void) {
	if (st_db_postgresql_driver.nb_configurations > 0)
		return st_db_postgresql_driver.configurations;
	return NULL;
}

static void st_db_postgresql_init(void) {
	st_database_register_driver(&st_db_postgresql_driver);
}

