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

#define _GNU_SOURCE
// pthread_mutex_lock, pthread_mutex_unlock,
#include <pthread.h>

#include <libstone/log.h>
#include <libstone/value.h>

#include "database.h"
#include "loader.h"

static struct st_value * st_database_drivers = NULL;
static struct st_database_v1 * st_database_default_driver = NULL;
static pthread_mutex_t st_database_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

static void st_database_exit(void) __attribute__((destructor));
static void st_database_init(void) __attribute__((constructor));


static void st_database_exit() {
	st_value_free(st_database_drivers);
	st_database_drivers = NULL;
}

__asm__(".symver st_database_get_default_driver_v1, st_database_get_default_driver@@LIBSTONE_1.2");
struct st_database * st_database_get_default_driver_v1() {
	return st_database_default_driver;
}

__asm__(".symver st_database_get_driver_v1, st_database_get_driver@@LIBSTONE_1.2");
struct st_database_v1 * st_database_get_driver_v1(const char * driver) {
	if (driver == NULL)
		return NULL;

	pthread_mutex_lock(&st_database_lock);

	void * cookie = NULL;
	if (!st_value_hashtable_has_key2(st_database_drivers, driver)) {
		cookie = st_loader_load("database", driver);

		if (cookie == NULL) {
			pthread_mutex_unlock(&st_database_lock);
			st_log_write(st_log_level_error, "Failed to load database driver '%s'", driver);
			return NULL;
		}

		if (!st_value_hashtable_has_key2(st_database_drivers, driver)) {
			pthread_mutex_unlock(&st_database_lock);
			st_log_write(st_log_level_warning, "Driver '%s' did not call 'register_driver'", driver);
			return NULL;
		}
	}

	struct st_value * vdriver = st_value_hashtable_get2(st_database_drivers, driver, false);
	struct st_database_v1 * dr = vdriver->value.custom.data;

	pthread_mutex_unlock(&st_database_lock);

	return dr;
}

static void st_database_init() {
	st_database_drivers = st_value_new_hashtable2();
}

__asm__(".symver st_database_load_config_v1, st_database_load_config@@LIBSTONE_1.2");
void st_database_load_config_v1(struct st_value * config) {
	if (config == NULL || !(config->type == st_value_array || config->type == st_value_linked_list))
		return;

	struct st_value_iterator * iter = st_value_list_get_iterator(config);
	while (st_value_iterator_has_next(iter)) {
		struct st_value * conf = st_value_iterator_get_value(iter, false);
		if (conf->type != st_value_hashtable || !st_value_hashtable_has_key2(conf, "type"))
			continue;

		struct st_value * type = st_value_hashtable_get2(conf, "type", false);
		if (type->type != st_value_string)
			continue;

		struct st_database_v1 * driver = st_database_get_driver_v1(type->value.string);
		if (driver != NULL)
			driver->ops->add(conf);
	}
	st_value_iterator_free(iter);
}

__asm__(".symver st_database_register_driver_v1, st_database_register_driver@@LIBSTONE_1.2");
void st_database_register_driver_v1(struct st_database_v1 * driver) {
	if (driver == NULL) {
		st_log_write(st_log_level_error, "Try to register with null driver");
		return;
	}

	pthread_mutex_lock(&st_database_lock);

	if (st_value_hashtable_has_key2(st_database_drivers, driver->name)) {
		pthread_mutex_unlock(&st_database_lock);
		st_log_write(st_log_level_warning, "Database driver '%s' is already registred", driver->name);
		return;
	}

	st_value_hashtable_put2(st_database_drivers, driver->name, st_value_new_custom(driver, NULL), true);
	driver->api_level = 1;
	st_loader_register_ok();

	if (st_database_default_driver == NULL)
		st_database_default_driver = driver;

	pthread_mutex_unlock(&st_database_lock);

	st_log_write(st_log_level_info, "Database driver '%s' is now registred", driver->name);
}

