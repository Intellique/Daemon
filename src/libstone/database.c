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
*  Last modified: Sun, 21 Apr 2013 23:28:42 +0200                            *
\****************************************************************************/

#define _GNU_SOURCE
// pthread_mutex_lock, pthread_mutex_unlock, pthread_setcancelstate
#include <pthread.h>
// realloc
#include <stdlib.h>
// strcmp
#include <string.h>

#include <libstone/database.h>
#include <libstone/log.h>
#include <libstone/util/debug.h>

#include "loader.h"

static struct st_database ** st_database_drivers = NULL;
static unsigned int st_database_nb_databases = 0;

static void st_database_exit(void) __attribute__((destructor));


static void st_database_exit() {
	unsigned int i;
	for (i = 0; i < st_database_nb_databases; i++) {
		struct st_database * db = st_database_drivers[i];
		unsigned int j;
		for (j = 0; j < db->nb_configurations; j++) {
			struct st_database_config * conf = db->configurations + j;
			conf->ops->free(conf);
		}
		free(db->configurations);
		db->configurations = NULL;
		db->nb_configurations = 0;
	}

	free(st_database_drivers);
	st_database_drivers = 0;
	st_database_nb_databases = 0;
}

struct st_database_config * st_database_get_config_by_name(const char * name) {
	if (name == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_database, "Try to get a database configuration with name is NULL");
		st_debug_log_stack(16);
		return NULL;
	}

	unsigned int i;
	for (i = 0; i < st_database_nb_databases; i++) {
		unsigned int j;
		struct st_database * db = st_database_drivers[i];

		for (j = 0; j < db->nb_configurations; j++) {
			struct st_database_config * conf = db->configurations + j;
			if (!strcmp(name, conf->name))
				return conf;
		}
	}

	return NULL;
}

struct st_database * st_database_get_default_driver() {
	if (st_database_nb_databases > 0)
		return *st_database_drivers;

	return NULL;
}

struct st_database * st_database_get_driver(const char * driver) {
	if (driver == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_database, "Try to get a database driver with driver is NULL");
		st_debug_log_stack(16);
		return NULL;
	}

	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	pthread_mutex_lock(&lock);

	unsigned int i;
	struct st_database * dr = NULL;
	for (i = 0; i < st_database_nb_databases && dr == NULL; i++)
		if (!strcmp(driver, st_database_drivers[i]->name))
			dr = st_database_drivers[i];

	if (dr == NULL) {
		void * cookie = st_loader_load("db", driver);

		if (cookie == NULL) {
			st_log_write_all(st_log_level_error, st_log_type_database, "Failed to load driver %s", driver);
			st_debug_log_stack(16);
			pthread_mutex_unlock(&lock);
			return NULL;
		}

		for (i = 0; i < st_database_nb_databases && dr == NULL; i++)
			if (!strcmp(driver, st_database_drivers[i]->name)) {
				dr = st_database_drivers[i];
				dr->cookie = cookie;

				st_log_write_all(st_log_level_debug, st_log_type_checksum, "Driver '%s' is now registred, src checksum: %s", driver, dr->src_checksum);
			}

		if (dr == NULL)
			st_log_write_all(st_log_level_error, st_log_type_database, "Driver %s not found", driver);
	}

	pthread_mutex_unlock(&lock);

	return dr;
}

void st_database_register_driver(struct st_database * driver) {
	if (driver == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_database, "Try to register with a NULL driver");
		st_debug_log_stack(16);
		return;
	}

	if (st_plugin_check(&driver->api_level) == false) {
		st_log_write_all(st_log_level_error, st_log_type_database, "Driver '%s' has not the correct api version", driver->name);
		st_debug_log_stack(16);
		return;
	}

	unsigned int i;
	for (i = 0; i < st_database_nb_databases; i++)
		if (st_database_drivers[i] == driver || !strcmp(driver->name, st_database_drivers[i]->name)) {
			st_log_write_all(st_log_level_warning, st_log_type_checksum, "Driver '%s' is already registred", driver->name);
			return;
		}

	void * new_addr = realloc(st_database_drivers, (st_database_nb_databases + 1) * sizeof(struct st_database *));
	if (new_addr == NULL) {
		st_log_write_all(st_log_level_info, st_log_type_database, "Driver '%s' cannot be registred because there is not enough memory", driver->name);
		return;
	}

	st_database_drivers = new_addr;
	st_database_drivers[st_database_nb_databases] = driver;
	st_database_nb_databases++;

	st_loader_register_ok();

	st_log_write_all(st_log_level_info, st_log_type_database, "Driver '%s' is now registred", driver->name);
}

