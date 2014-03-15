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
*  Last modified: Tue, 08 Oct 2013 11:10:33 +0200                            *
\****************************************************************************/

#define _GNU_SOURCE
// bool
#include <stdbool.h>
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

#include <libstone/thread_pool.h>
#include <libstone/user.h>

#include "loader.h"
#include "log.h"

static void st_log_exit(void) __attribute__((destructor));
static void st_log_sent_message(void * arg);

static bool st_log_display_at_exit = true;
static volatile bool st_log_logger_running = false;
static bool st_log_finished = false;

static struct st_log_driver ** st_log_drivers = NULL;
static unsigned int st_log_nb_drivers = 0;

struct st_log_message_unsent {
	struct st_log_message data;
	struct st_log_message_unsent * next;
};
static struct st_log_message_unsent * volatile st_log_message_first = NULL;
static struct st_log_message_unsent * volatile st_log_message_last = NULL;

static pthread_mutex_t st_log_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static pthread_cond_t st_log_wait = PTHREAD_COND_INITIALIZER;


void st_log_disable_display_log() {
	st_log_display_at_exit = false;
}

static void st_log_exit() {
	if (st_log_display_at_exit) {
		struct st_log_message_unsent * mes;
		for (mes = st_log_message_first; mes != NULL; mes = mes->next)
			printf("%c: %s\n", st_log_level_to_string(mes->data.level)[0], mes->data.message);
	}

	unsigned int i;
	for (i = 0; i < st_log_nb_drivers; i++) {
		struct st_log_driver * driver = st_log_drivers[i];
		unsigned int j;
		for (j = 0; j < driver->nb_modules; j++) {
			struct st_log_module * mod = driver->modules + j;
			mod->ops->free(mod);
		}
		free(driver->modules);
		driver->modules = NULL;
		driver->nb_modules = 0;
	}

	free(st_log_drivers);
	st_log_drivers = NULL;
	st_log_nb_drivers = 0;

	struct st_log_message_unsent * message = st_log_message_first;
	st_log_message_first = st_log_message_last = NULL;

	while (message != NULL) {
		struct st_log_message * mes = &message->data;
		free(mes->message);

		struct st_log_message_unsent * next = message->next;
		free(message);

		message = next;
	}

	st_log_finished = true;
}

struct st_log_driver * st_log_get_driver(const char * driver) {
	if (driver == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Get checksum driver with driver's name is NULL");
		return NULL;
	}

	pthread_mutex_lock(&st_log_lock);

	unsigned int i;
	struct st_log_driver * dr = NULL;
	for (i = 0; i < st_log_nb_drivers && dr == NULL; i++)
		if (!strcmp(driver, st_log_drivers[i]->name))
			dr = st_log_drivers[i];

	if (dr == NULL) {
		void * cookie = st_loader_load("log", driver);

		if (dr == NULL && cookie == NULL) {
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Log: Failed to load driver %s", driver);
			pthread_mutex_unlock(&st_log_lock);
			return NULL;
		}

		for (i = 0; i < st_log_nb_drivers && dr == NULL; i++)
			if (!strcmp(driver, st_log_drivers[i]->name)) {
				dr = st_log_drivers[i];
				dr->cookie = cookie;

				st_log_write_all(st_log_level_debug, st_log_type_checksum, "Driver '%s' is now registred, src checksum: %s", driver, dr->src_checksum);
			}

		if (dr == NULL)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Log: Driver %s not found", driver);
	}

	pthread_mutex_unlock(&st_log_lock);

	return dr;
}

void st_log_register_driver(struct st_log_driver * driver) {
	if (driver == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Log: Try to register with driver=0");
		return;
	}

	if (driver->api_level != STONE_LOG_API_LEVEL) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Log: Driver '%s' has not the correct api version (current: %d, expected: %d)", driver->name, driver->api_level, STONE_LOG_API_LEVEL);
		return;
	}

	unsigned int i;
	for (i = 0; i < st_log_nb_drivers; i++)
		if (st_log_drivers[i] == driver || !strcmp(driver->name, st_log_drivers[i]->name)) {
			st_log_write_all(st_log_level_info, st_log_type_daemon, "Log: Driver '%s' is already registred", driver->name);
			return;
		}

	void * new_addr = realloc(st_log_drivers, (st_log_nb_drivers + 1) * sizeof(struct st_log_driver *));
	if (new_addr == NULL) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "Driver '%s' cannot be registred because there is not enough memory", driver->name);
		return;
	}

	st_log_drivers = new_addr;
	st_log_drivers[st_log_nb_drivers] = driver;
	st_log_nb_drivers++;

	st_loader_register_ok();

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Log: Driver '%s' is now registred", driver->name);
}

static void st_log_sent_message(void * arg __attribute__((unused))) {
	for (;;) {
		pthread_mutex_lock(&st_log_lock);
		if (!st_log_logger_running && st_log_message_first == NULL)
			break;
		if (st_log_message_first == NULL)
			pthread_cond_wait(&st_log_wait, &st_log_lock);

		struct st_log_message_unsent * message = st_log_message_first;
		st_log_message_first = st_log_message_last = NULL;
		pthread_mutex_unlock(&st_log_lock);

		while (message != NULL) {
			unsigned int i;
			struct st_log_message * mes = &message->data;
			for (i = 0; i < st_log_nb_drivers; i++) {
				unsigned int j;
				for (j = 0; j < st_log_drivers[i]->nb_modules; j++)
					if (st_log_drivers[i]->modules[j].level >= mes->level)
						st_log_drivers[i]->modules[j].ops->write(st_log_drivers[i]->modules + j, mes);
			}

			struct st_log_message_unsent * next = message->next;
			free(mes->message);
			free(message);

			message = next;
		}
	}

	pthread_cond_signal(&st_log_wait);
	pthread_mutex_unlock(&st_log_lock);
}

void st_log_start_logger() {
	pthread_mutex_lock(&st_log_lock);

	if (st_log_nb_drivers == 0) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Start logger without log modules loaded");
	} else if (!st_log_logger_running) {
		st_log_logger_running = 1;
		st_thread_pool_run2("logger", st_log_sent_message, NULL, 4);
		st_log_display_at_exit = 0;
	}

	pthread_mutex_unlock(&st_log_lock);
}

void st_log_stop_logger() {
	pthread_mutex_lock(&st_log_lock);

	if (st_log_logger_running) {
		st_log_logger_running = false;
		pthread_cond_signal(&st_log_wait);
		pthread_cond_wait(&st_log_wait, &st_log_lock);
	}

	pthread_mutex_unlock(&st_log_lock);
}

void st_log_write_all(enum st_log_level level, enum st_log_type type, const char * format, ...) {
	if (st_log_finished)
		return;

	char * message = NULL;

	va_list va;
	va_start(va, format);
	vasprintf(&message, format, va);
	va_end(va);

	struct st_log_message_unsent * mes = malloc(sizeof(struct st_log_message_unsent));
	struct st_log_message * data = &mes->data;
	data->level = level;
	data->type = type;
	data->message = message;
	data->user = NULL;
	data->timestamp = time(NULL);
	mes->next = NULL;

	pthread_mutex_lock(&st_log_lock);

	if (!st_log_message_first)
		st_log_message_first = st_log_message_last = mes;
	else
		st_log_message_last = st_log_message_last->next = mes;

	pthread_cond_signal(&st_log_wait);
	pthread_mutex_unlock(&st_log_lock);
}

void st_log_write_all2(enum st_log_level level, enum st_log_type type, struct st_user * user, const char * format, ...) {
	if (st_log_finished)
		return;

	char * message = NULL;

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
	data->timestamp = time(NULL);
	mes->next = NULL;

	pthread_mutex_lock(&st_log_lock);

	if (!st_log_message_first)
		st_log_message_first = st_log_message_last = mes;
	else
		st_log_message_last = st_log_message_last->next = mes;

	pthread_cond_signal(&st_log_wait);
	pthread_mutex_unlock(&st_log_lock);
}

