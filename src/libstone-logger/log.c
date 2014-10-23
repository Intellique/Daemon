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

#define _GNU_SOURCE
// bindtextdomain, gettext, textdomain
#include <libintl.h>
// va_end, va_start
#include <stdarg.h>
// free
#include <stdlib.h>
// vasprintf
#include <stdio.h>
// time
#include <time.h>

#include <libstone/log.h>
#include <libstone/string.h>
#include <libstone/value.h>

#include "loader.h"
#include "log.h"

#include "config.h"

static struct st_value_v1 * lgr_drivers = NULL;
static struct st_value_v1 * lgr_messages = NULL;
static struct st_value_v1 * lgr_modules = NULL;

static void lgr_log_exit(void) __attribute__((destructor));
static void lgr_log_free_module(void * module);
static void lgr_log_init(void) __attribute__((constructor));


static void lgr_log_exit() {
	st_value_free(lgr_drivers);
	st_value_free(lgr_messages);
	st_value_free(lgr_modules);
}

static void lgr_log_free_module(void * module) {
	struct lgr_log_module * mod = module;
	mod->ops->free(mod);
}

static void lgr_log_init() {
	bindtextdomain("libstone-logger", LOCALE_DIR);
	textdomain("libstone-logger");

	lgr_drivers = st_value_new_hashtable(st_string_compute_hash);
	lgr_messages = st_value_new_linked_list();
	lgr_modules = st_value_new_linked_list();
}

__asm__(".symver lgr_log_load_v1, lgr_log_load@@LIBSTONE_LOGGER_1.2");
bool lgr_log_load_v1(struct st_value_v1 * params) {
	bool ok = true;

	struct st_value_iterator * iter = st_value_list_get_iterator(params);
	while (ok && st_value_iterator_has_next(iter)) {
		struct st_value_v1 * elt = st_value_iterator_get_value(iter, false);
		struct st_value_v1 * type = st_value_hashtable_get2(elt, "type", false, false);

		void * cookie = NULL;
		if (!st_value_hashtable_has_key(lgr_drivers, type)) {
			cookie = lgr_loader_load("log", st_value_string_get(type));

			if (cookie == NULL) {
				ok = false;
				break;
			}
		}

		struct st_value_v1 * st_driver = st_value_hashtable_get(lgr_drivers, type, false, false);
		struct lgr_log_driver * driver = st_value_custom_get(st_driver);
		if (cookie != NULL)
			driver->cookie = cookie;

		struct lgr_log_module * module = driver->new_module(elt);
		if (module != NULL)
			st_value_list_push(lgr_modules, st_value_new_custom(module, lgr_log_free_module), true);
		else
			ok = false;
	}
	st_value_iterator_free(iter);

	return ok;
}

__asm__(".symver lgr_log_register_driver_v1, lgr_log_register_driver@@LIBSTONE_LOGGER_1.2");
void lgr_log_register_driver_v1(struct lgr_log_driver * driver) {
	if (driver == NULL)
		return;

	if (!st_value_hashtable_has_key2(lgr_drivers, driver->name)) {
		st_value_hashtable_put(lgr_drivers, st_value_new_string(driver->name), true, st_value_new_custom(driver, NULL), true);
		driver->api_level = 1;
		lgr_loader_register_ok();
	}
}

__asm__(".symver lgr_log_write_v1, lgr_log_write@@LIBSTONE_LOGGER_1.2");
void lgr_log_write_v1(struct st_value_v1 * message) {
	struct st_value_v1 * level = st_value_hashtable_get2(message, "level", false, false);
	if (level == NULL || level->type != st_value_string)
		return;

	enum st_log_level lvl = st_log_string_to_level(st_value_string_get(level));
	struct st_value_iterator * iter = st_value_list_get_iterator(lgr_modules);
	while (st_value_iterator_has_next(iter)) {
		struct st_value_v1 * module = st_value_iterator_get_value(iter, false);
		struct lgr_log_module * mod = st_value_custom_get(module);

		if (lvl <= mod->level)
			mod->ops->write(mod, message);
	}
	st_value_iterator_free(iter);
}

__asm__(".symver lgr_log_write2_v1, lgr_log_write2@@LIBSTONE_LOGGER_1.2");
void lgr_log_write2_v1(enum st_log_level level, enum st_log_type type, const char * format, ...) {
	char * str_message = NULL;

	long long int timestamp = time(NULL);

	va_list va;
	va_start(va, format);
	vasprintf(&str_message, format, va);
	va_end(va);

	struct st_value_v1 * message = st_value_pack("{sssssiss}", "level", st_log_level_to_string(level, false), "type", st_log_type_to_string(type, false), "timestamp", timestamp, "message", str_message);

	free(str_message);

	static bool writing_message = false;
	unsigned int nb_unsent_messages = st_value_list_get_length(lgr_messages);
	unsigned int nb_modules = st_value_list_get_length(lgr_modules);

	if (nb_modules == 0 || writing_message) {
		st_value_list_push(lgr_messages, message, true);
		return;
	}

	writing_message = true;

	if (nb_unsent_messages > 0) {
		struct st_value_iterator * iter = st_value_list_get_iterator(lgr_messages);
		while (st_value_iterator_has_next(iter)) {
			struct st_value_v1 * msg = st_value_iterator_get_value(iter, false);
			lgr_log_write_v1(msg);
		}
		st_value_iterator_free(iter);
		st_value_list_clear(lgr_messages);
	}

	lgr_log_write_v1(message);
	st_value_free(message);

	writing_message = false;
}

