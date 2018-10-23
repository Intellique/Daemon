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
*  Copyright (C) 2013-2018, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// dgettext
#include <libintl.h>
// pthread_mutex_lock, pthread_mutex_unlock,
#include <pthread.h>
// free
#include <stdlib.h>

#include <libstoriqone/database.h>
#include <libstoriqone/log.h>
#include <libstoriqone/value.h>

#include "loader.h"

static struct so_value * so_database_drivers = NULL;
static struct so_database * so_database_default_driver = NULL;
static pthread_mutex_t so_database_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

static void so_database_exit(void) __attribute__((destructor));
static void so_database_init(void) __attribute__((constructor));


static void so_database_exit() {
	so_value_free(so_database_drivers);
	so_database_drivers = NULL;
}

struct so_database * so_database_get_default_driver() {
	return so_database_default_driver;
}

struct so_database * so_database_get_driver(const char * driver) {
	if (driver == NULL) {
		so_log_write(so_log_level_error,
			dgettext("libstoriqone", "so_database_get_driver: invalid parameter, 'driver' should not be NULL"));
		return NULL;
	}

	pthread_mutex_lock(&so_database_lock);

	void * cookie = NULL;
	if (!so_value_hashtable_has_key2(so_database_drivers, driver)) {
		cookie = so_loader_load("database", driver);

		if (cookie == NULL) {
			pthread_mutex_unlock(&so_database_lock);
			so_log_write(so_log_level_error,
				dgettext("libstoriqone", "so_database_get_driver: failed to load database driver '%s'"),
				driver);
			return NULL;
		}

		if (!so_value_hashtable_has_key2(so_database_drivers, driver)) {
			pthread_mutex_unlock(&so_database_lock);
			so_log_write(so_log_level_warning,
				dgettext("libstoriqone", "so_database_get_driver: driver '%s' did not call 'so_database_register_driver'"),
				driver);
			return NULL;
		}
	}

	struct so_value * vdriver = so_value_hashtable_get2(so_database_drivers, driver, false, false);
	struct so_database * dr = so_value_custom_get(vdriver);

	if (cookie != NULL)
		dr->cookie = cookie;

	pthread_mutex_unlock(&so_database_lock);

	return dr;
}

static void so_database_init() {
	so_database_drivers = so_value_new_hashtable2();
}

void so_database_load_config(struct so_value * config) {
	if (config == NULL || !(config->type == so_value_array || config->type == so_value_linked_list)) {
		so_log_write(so_log_level_error,
			dgettext("libstoriqone", "so_database_load_config: invalid parameters (config: %p)"),
			config);
		return;
	}

	struct so_value_iterator * iter = so_value_list_get_iterator(config);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * conf = so_value_iterator_get_value(iter, false);

		const char * type = NULL;
		if (so_value_unpack(conf, "{sS}", "type", &type) < 1)
			continue;

		struct so_database * driver = so_database_get_driver(type);
		if (driver != NULL)
			driver->ops->add(conf);
	}
	so_value_iterator_free(iter);
}

void so_database_register_driver(struct so_database * driver) {
	if (driver == NULL) {
		so_log_write(so_log_level_error,
			dgettext("libstoriqone", "so_database_register_driver: trying to register a NULL driver"));
		return;
	}

	pthread_mutex_lock(&so_database_lock);

	if (so_value_hashtable_has_key2(so_database_drivers, driver->name)) {
		pthread_mutex_unlock(&so_database_lock);
		so_log_write(so_log_level_warning,
			dgettext("libstoriqone", "so_database_register_driver: database driver '%s' is already registered"),
			driver->name);
		return;
	}

	so_value_hashtable_put2(so_database_drivers, driver->name, so_value_new_custom(driver, NULL), true);
	so_loader_register_ok();

	if (so_database_default_driver == NULL)
		so_database_default_driver = driver;

	pthread_mutex_unlock(&so_database_lock);

	so_log_write(so_log_level_info,
		dgettext("libstoriqone", "so_database_register_driver: database driver '%s' is now registered"),
		driver->name);
}
