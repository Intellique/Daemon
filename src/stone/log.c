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
*  Last modified: Tue, 03 Apr 2012 16:10:52 +0200                         *
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
// sleep
#include <unistd.h>

#include <stone/threadpool.h>
#include <stone/user.h>

#include "loader.h"
#include "log.h"

static void st_log_exit(void) __attribute__((destructor));
static void st_log_sent_message(void * arg);

static int st_log_display_at_exit = 1;
static struct st_log_driver ** st_log_drivers = 0;
static unsigned int st_log_nb_drivers = 0;
static struct st_log_message_unsent {
	struct st_log_message data;
	struct st_log_message_unsent * next;
} * st_log_message_first = 0, * st_log_message_last = 0;
static pthread_mutex_t st_log_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static pthread_cond_t st_log_wait = PTHREAD_COND_INITIALIZER;

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
	if (st_log_display_at_exit) {
		struct st_log_message_unsent * mes;
		for (mes = st_log_message_first; mes; mes = mes->next)
			printf("%c: %s\n", st_log_level_to_string(mes->data.level)[0], mes->data.message);
	} else {
		while (st_log_message_first)
			sleep(1);
	}
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

void st_log_sent_message(void * arg __attribute__((unused))) {
	for (;;) {
		pthread_mutex_lock(&st_log_lock);

		if (!st_log_message_first)
			pthread_cond_wait(&st_log_wait, &st_log_lock);
		struct st_log_message_unsent * message = st_log_message_first;
		st_log_message_first = message->next;
		if (!st_log_message_first)
			st_log_message_last = 0;

		pthread_mutex_unlock(&st_log_lock);

		unsigned int i;
		struct st_log_message * mes = &message->data;
		for (i = 0; i < st_log_nb_drivers; i++) {
			unsigned int j;
			for (j = 0; j < st_log_drivers[i]->nb_modules; j++)
				if (st_log_drivers[i]->modules[j].level >= mes->level)
					st_log_drivers[i]->modules[j].ops->write(st_log_drivers[i]->modules + j, mes);
		}

		free(mes->message);
		free(message);
	}
}

void st_log_start_logger() {
	st_threadpool_run(st_log_sent_message, 0);
	st_log_display_at_exit = 0;
}

void st_log_stop_logger() {
	while (st_log_message_first)
		sleep(1);
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

	struct st_log_message_unsent * mes = malloc(sizeof(struct st_log_message_unsent));
	struct st_log_message * data = &mes->data;
	data->level = level;
	data->type = type;
	data->message = message;
	data->user = 0;
	data->timestamp = time(0);
	mes->next = 0;

	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	pthread_mutex_lock(&st_log_lock);

	if (!st_log_message_first)
		st_log_message_first = st_log_message_last = mes;
	else
		st_log_message_last = st_log_message_last->next = mes;

	pthread_mutex_unlock(&st_log_lock);
	if (old_state == PTHREAD_CANCEL_DISABLE)
		pthread_setcancelstate(old_state, 0);

	pthread_cond_signal(&st_log_wait);
}

void st_log_write_all2(enum st_log_level level, enum st_log_type type, struct st_user * user, const char * format, ...) {
	char * message = 0;

	va_list va;
	va_start(va, format);
	vasprintf(&message, format, va);
	va_end(va);

	struct st_log_message_unsent * mes = malloc(sizeof(struct st_log_message_unsent));
	struct st_log_message * data = &mes->data;
	data->level = level;
	data->type = type;
	data->message = message;
	data->user = user;
	data->timestamp = time(0);
	mes->next = 0;

	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	pthread_mutex_lock(&st_log_lock);

	if (!st_log_message_first)
		st_log_message_first = st_log_message_last = mes;
	else
		st_log_message_last = st_log_message_last->next = mes;

	pthread_mutex_unlock(&st_log_lock);
	if (old_state == PTHREAD_CANCEL_DISABLE)
		pthread_setcancelstate(old_state, 0);

	pthread_cond_signal(&st_log_wait);
}

