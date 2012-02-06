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
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Tue, 03 Jan 2012 10:43:39 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// pthread_mutex_lock, pthread_mutex_unlock, pthread_setcancelstate
#include <pthread.h>
// va_end, va_start
#include <stdarg.h>
// realloc
#include <stdlib.h>
// printf, snprintf
#include <stdio.h>
// strcasecmp, strcmp
#include <string.h>

#include "loader.h"
#include "log.h"

#include <stone/user.h>

static void st_log_exit(void) __attribute__((destructor));
static void st_log_store_message(char * who, enum st_log_level level, enum st_log_type type, char * message, struct st_user * user);

static int st_log_display_at_exit = 1;
static struct st_log_driver ** st_log_drivers = 0;
static unsigned int st_log_nb_drivers = 0;
static struct st_log_message_unsent {
	char * who;
	enum st_log_level level;
	enum st_log_type type;
	char * message;
	struct st_user * user;
	char sent;
} * st_log_message_unsent = 0;
static unsigned int st_log_nb_message_unsent = 0;
static unsigned int st_log_nb_message_unsent_to = 0;
static pthread_mutex_t st_log_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

static struct st_log_level2 {
	enum st_log_level level;
	const char * name;
} st_log_levels[] = {
	{ st_log_level_debug,   "Debug" },
	{ st_log_level_error,   "Error" },
	{ st_log_level_info,    "Info" },
	{ st_log_level_warning, "Warning" },

	{ st_log_level_unknown, "Unknown level" },
};

static struct st_log_type2 {
	enum st_log_type type;
	const char * name;
} st_log_types[] = {
	{ st_log_type_changer,         "Changer" },
	{ st_log_type_checksum,        "Checksum" },
	{ st_log_type_daemon,          "Daemon" },
	{ st_log_type_database,        "Database" },
	{ st_log_type_drive,           "Drive" },
	{ st_log_type_job,             "Job" },
	{ st_log_type_plugin_checksum, "Plugin Checksum" },
	{ st_log_type_plugin_db,       "Plugin Database" },
	{ st_log_type_plugin_log,      "Plugin Log" },
	{ st_log_type_scheduler,       "Scheduler" },
	{ st_log_type_ui,              "User Interface" },
	{ st_log_type_user_message,    "User Message" },

	{ st_log_type_unknown, "Unknown type" },
};


void st_log_disable_display_log() {
	st_log_display_at_exit = 0;
}

void st_log_exit() {
	if (!st_log_display_at_exit)
		return;

	unsigned int mes;
	for (mes = 0; mes < st_log_nb_message_unsent; mes++)
		printf("%c: %s\n", st_log_level_to_string(st_log_message_unsent[mes].level)[0], st_log_message_unsent[mes].message);
}

int st_log_flush_message() {
	unsigned int mes, ok = 0;
	for (mes = 0; mes < st_log_nb_message_unsent; mes++) {
		unsigned int dr;
		for (dr = 0; dr < st_log_nb_drivers; dr++) {
			unsigned int mod;
			for (mod = 0; mod < st_log_drivers[dr]->nb_modules; mod++) {
				if (st_log_message_unsent[mes].who && strcmp(st_log_message_unsent[mes].who, st_log_drivers[dr]->modules[mod].alias))
					continue;
				if (st_log_drivers[dr]->modules[mod].level >= st_log_message_unsent[mes].level) {
					st_log_drivers[dr]->modules[mod].ops->write(st_log_drivers[dr]->modules + mod, st_log_message_unsent[mes].level, st_log_message_unsent[mes].type, st_log_message_unsent[mes].message, st_log_message_unsent[mes].user);
					st_log_message_unsent[mes].sent = 1;
					ok = 1;
				}
			}
		}
	}

	if (ok) {
		for (mes = 0; mes < st_log_nb_message_unsent; mes++) {
			if (st_log_message_unsent[mes].sent) {
				if (st_log_message_unsent[mes].who) {
					free(st_log_message_unsent[mes].who);
					st_log_nb_message_unsent_to--;
				}
				if (st_log_message_unsent[mes].message)
					free(st_log_message_unsent[mes].message);

				if (mes + 1 < st_log_nb_message_unsent)
					memmove(st_log_message_unsent + mes, st_log_message_unsent + mes + 1, (st_log_nb_message_unsent - mes - 1) * sizeof(struct st_log_message_unsent));
				st_log_nb_message_unsent--, mes--;
				if (st_log_nb_message_unsent > 0)
					st_log_message_unsent = realloc(st_log_message_unsent, st_log_nb_message_unsent * sizeof(struct st_log_message_unsent));
			}
		}

		if (st_log_nb_message_unsent == 0) {
			free(st_log_message_unsent);
			st_log_message_unsent = 0;
			ok = 2;
		}
	}

	return ok;
}

struct st_log_driver * st_log_get_driver(const char * driver) {
	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	pthread_mutex_lock(&st_log_lock);

	if (!driver) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Log: Driver shall not be null");
		return 0;
	}

	unsigned int i;
	struct st_log_driver * dr = 0;
	for (i = 0; i < st_log_nb_drivers && !dr; i++)
		if (!strcmp(driver, st_log_drivers[i]->name))
			dr = st_log_drivers[i];

	void * cookie = 0;
	if (!dr)
		cookie =st_loader_load("log", driver);

	if (!dr && !cookie) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Log: Failed to load driver %s", driver);
		pthread_mutex_unlock(&st_log_lock);
		if (old_state == PTHREAD_CANCEL_DISABLE)
			pthread_setcancelstate(old_state, 0);
		return 0;
	}

	for (i = 0; i < st_log_nb_drivers && !dr; i++)
		if (!strcmp(driver, st_log_drivers[i]->name)) {
			dr = st_log_drivers[i];
			dr->cookie = cookie;
		}

	if (!dr)
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Log: Driver %s not found", driver);

	pthread_mutex_unlock(&st_log_lock);
	if (old_state == PTHREAD_CANCEL_DISABLE)
		pthread_setcancelstate(old_state, 0);

	return dr;
}

const char * st_log_level_to_string(enum st_log_level level) {
	struct st_log_level2 * ptr;
	for (ptr = st_log_levels; ptr->level != st_log_level_unknown; ptr++)
		if (ptr->level == level)
			return ptr->name;
	return ptr->name;
}

void st_log_register_driver(struct st_log_driver * driver) {
	if (!driver) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Log: Try to register with driver=0");
		return;
	}

	if (driver->api_version != STONE_LOG_APIVERSION) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Log: Driver(%s) has not the correct api version (current: %d, expected: %d)", driver->name, driver->api_version, STONE_LOG_APIVERSION);
		return;
	}

	unsigned int i;
	for (i = 0; i < st_log_nb_drivers; i++)
		if (st_log_drivers[i] == driver || !strcmp(driver->name, st_log_drivers[i]->name)) {
			st_log_write_all(st_log_level_info, st_log_type_daemon, "Log: Driver(%s) is already registred", driver->name);
			return;
		}

	st_log_drivers = realloc(st_log_drivers, (st_log_nb_drivers + 1) * sizeof(struct st_log_driver *));
	st_log_drivers[st_log_nb_drivers] = driver;
	st_log_nb_drivers++;

	st_loader_register_ok();

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Log: Driver(%s) is now registred", driver->name);
}

void st_log_store_message(char * who, enum st_log_level level, enum st_log_type type, char * message, struct st_user * user) {
	st_log_message_unsent = realloc(st_log_message_unsent, (st_log_nb_message_unsent + 1) * sizeof(struct st_log_message_unsent));
	st_log_message_unsent[st_log_nb_message_unsent].who = who;
	st_log_message_unsent[st_log_nb_message_unsent].level = level;
	st_log_message_unsent[st_log_nb_message_unsent].type = type;
	st_log_message_unsent[st_log_nb_message_unsent].message = message;
	st_log_message_unsent[st_log_nb_message_unsent].user = user;
	st_log_message_unsent[st_log_nb_message_unsent].sent = 0;
	st_log_nb_message_unsent++;
	if (who)
		st_log_nb_message_unsent_to++;
}

enum st_log_level st_log_string_to_level(const char * string) {
	if (!string)
		return st_log_level_unknown;

	struct st_log_level2 * ptr;
	for (ptr = st_log_levels; ptr->level != st_log_level_unknown; ptr++)
		if (!strcasecmp(ptr->name, string))
			return ptr->level;
	return st_log_level_unknown;
}

enum st_log_type st_log_string_to_type(const char * string) {
	if (!string)
		return st_log_type_unknown;

	struct st_log_type2 * ptr;
	for (ptr = st_log_types; ptr->type != st_log_type_unknown; ptr++)
		if (!strcasecmp(ptr->name, string))
			return ptr->type;
	return st_log_type_unknown;
}

const char * st_log_type_to_string(enum st_log_type type) {
	struct st_log_type2 * ptr;
	for (ptr = st_log_types; ptr->type != st_log_type_unknown; ptr++)
		if (ptr->type == type)
			return ptr->name;
	return ptr->name;
}

void st_log_write_all(enum st_log_level level, enum st_log_type type, const char * format, ...) {
	char * message = 0;

	va_list va;
	va_start(va, format);
	vasprintf(&message, format, va);
	va_end(va);

	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	pthread_mutex_lock(&st_log_lock);

	if (st_log_nb_drivers == 0 || st_log_message_unsent) {
		st_log_store_message(0, level, type, message, 0);

		pthread_mutex_unlock(&st_log_lock);
		if (old_state == PTHREAD_CANCEL_DISABLE)
			pthread_setcancelstate(old_state, 0);
		return;
	}

	unsigned int i, sent = 0;
	for (i = 0; i < st_log_nb_drivers; i++) {
		unsigned int j;
		for (j = 0; j < st_log_drivers[i]->nb_modules; j++) {
			if (st_log_drivers[i]->modules[j].level >= level) {
				st_log_drivers[i]->modules[j].ops->write(st_log_drivers[i]->modules + j, level, type, message, 0);
				sent = 1;
			}
		}
	}

	if (!sent)
		st_log_store_message(0, level, type, message, 0);

	pthread_mutex_unlock(&st_log_lock);
	if (old_state == PTHREAD_CANCEL_DISABLE)
		pthread_setcancelstate(old_state, 0);

	if (sent)
		free(message);
}

void st_log_write_all2(enum st_log_level level, enum st_log_type type, struct st_user * user, const char * format, ...) {
	char * message = 0;

	va_list va;
	va_start(va, format);
	vasprintf(&message, format, va);
	va_end(va);

	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	pthread_mutex_lock(&st_log_lock);

	if (st_log_nb_drivers == 0 || st_log_message_unsent) {
		st_log_store_message(0, level, type, message, user);

		pthread_mutex_unlock(&st_log_lock);
		if (old_state == PTHREAD_CANCEL_DISABLE)
			pthread_setcancelstate(old_state, 0);
		return;
	}

	unsigned int i, sent = 0;
	for (i = 0; i < st_log_nb_drivers; i++) {
		unsigned int j;
		for (j = 0; j < st_log_drivers[i]->nb_modules; j++) {
			if (st_log_drivers[i]->modules[j].level >= level) {
				st_log_drivers[i]->modules[j].ops->write(st_log_drivers[i]->modules + j, level, type, message, user);
				sent = 1;
			}
		}
	}

	if (!sent)
		st_log_store_message(0, level, type, message, user);

	pthread_mutex_unlock(&st_log_lock);
	if (old_state == PTHREAD_CANCEL_DISABLE)
		pthread_setcancelstate(old_state, 0);

	if (sent)
		free(message);
}

