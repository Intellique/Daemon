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

#define _GNU_SOURCE
// bindtextdomain
#include <libintl.h>
// va_end, va_start
#include <stdarg.h>
// free
#include <stdlib.h>
// vasprintf
#include <stdio.h>
// time
#include <time.h>

#include <libstoriqone/string.h>
#include <libstoriqone/value.h>
#include <logger/log.h>

#include "loader.h"

#include "config.h"

static struct so_value * solgr_drivers = NULL;
static struct so_value * solgr_messages = NULL;
static struct so_value * solgr_modules = NULL;

static void solgr_log_exit(void) __attribute__((destructor(101)));
static void solgr_log_free_module(void * module);
static void solgr_log_init(void) __attribute__((constructor(101)));


static void solgr_log_exit() {
	so_value_free(solgr_drivers);
	so_value_free(solgr_messages);
	so_value_free(solgr_modules);
}

static void solgr_log_free_module(void * module) {
	struct solgr_log_module * mod = module;
	mod->ops->free(mod);
}

static void solgr_log_init() {
	bindtextdomain("libstoriqone-logger", LOCALE_DIR);

	solgr_drivers = so_value_new_hashtable(so_string_compute_hash);
	solgr_messages = so_value_new_linked_list();
	solgr_modules = so_value_new_linked_list();
}

bool solgr_log_load(struct so_value * params) {
	bool ok = true;

	struct so_value_iterator * iter = so_value_list_get_iterator(params);
	while (ok && so_value_iterator_has_next(iter)) {
		struct so_value * elt = so_value_iterator_get_value(iter, false);
		struct so_value * type = so_value_hashtable_get2(elt, "type", false, false);

		void * cookie = NULL;
		if (!so_value_hashtable_has_key(solgr_drivers, type)) {
			cookie = solgr_loader_load("log", so_value_string_get(type));

			if (cookie == NULL) {
				ok = false;
				break;
			}
		}

		struct so_value * so_driver = so_value_hashtable_get(solgr_drivers, type, false, false);
		struct solgr_log_driver * driver = so_value_custom_get(so_driver);
		if (cookie != NULL)
			driver->cookie = cookie;

		struct solgr_log_module * module = driver->new_module(elt);
		if (module != NULL)
			so_value_list_push(solgr_modules, so_value_new_custom(module, solgr_log_free_module), true);
		else
			ok = false;
	}
	so_value_iterator_free(iter);

	return ok;
}

void solgr_log_register_driver(struct solgr_log_driver * driver) {
	if (driver == NULL)
		return;

	if (!so_value_hashtable_has_key2(solgr_drivers, driver->name)) {
		so_value_hashtable_put(solgr_drivers, so_value_new_string(driver->name), true, so_value_new_custom(driver, NULL), true);
		solgr_loader_register_ok();
	}
}

void solgr_log_write(struct so_value * message) {
	struct so_value * level = so_value_hashtable_get2(message, "level", false, false);
	if (level == NULL || level->type != so_value_string)
		return;

	enum so_log_level lvl = so_log_string_to_level(so_value_string_get(level), false);
	struct so_value_iterator * iter = so_value_list_get_iterator(solgr_modules);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * module = so_value_iterator_get_value(iter, false);
		struct solgr_log_module * mod = so_value_custom_get(module);

		if (lvl <= mod->level)
			mod->ops->write(mod, message);
	}
	so_value_iterator_free(iter);
}

void solgr_log_write2(enum so_log_level level, enum so_log_type type, const char * format, ...) {
	char * str_message = NULL;

	long long int timestamp = time(NULL);

	va_list va;
	va_start(va, format);
	vasprintf(&str_message, format, va);
	va_end(va);

	struct so_value * message = so_value_pack("{sssssiss}", "level", so_log_level_to_string(level, false), "type", so_log_type_to_string(type, false), "timestamp", timestamp, "message", str_message);

	free(str_message);

	static bool writing_message = false;
	unsigned int nb_unsent_messages = so_value_list_get_length(solgr_messages);
	unsigned int nb_modules = so_value_list_get_length(solgr_modules);

	if (nb_modules == 0 || writing_message) {
		so_value_list_push(solgr_messages, message, true);
		return;
	}

	writing_message = true;

	if (nb_unsent_messages > 0) {
		struct so_value_iterator * iter = so_value_list_get_iterator(solgr_messages);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * msg = so_value_iterator_get_value(iter, false);
			solgr_log_write(msg);
		}
		so_value_iterator_free(iter);
		so_value_list_clear(solgr_messages);
	}

	solgr_log_write(message);
	so_value_free(message);

	writing_message = false;
}

