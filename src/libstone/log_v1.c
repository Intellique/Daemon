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
// pthread
#include <pthread.h>
// va_end, va_start
#include <stdarg.h>
// vasprintf
#include <stdio.h>
// free
#include <stdlib.h>
// strcasecmp
#include <string.h>
// time
#include <time.h>
// read, write
#include <unistd.h>

#include "json_v1.h"
#include "log_v1.h"
#include "socket_v1.h"
#include "thread_pool_v1.h"
#include "value_v1.h"

static int st_log_socket = -1;
static pthread_mutex_t st_log_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static pthread_cond_t st_log_wait = PTHREAD_COND_INITIALIZER;
static struct st_value * st_log_messages = NULL;
static volatile bool st_log_finished = false;

static void st_log_send_message(void * arg);

static struct st_log_level2 {
	enum st_log_level level;
	const char * name;
} st_log_levels[] = {
	{ st_log_level_alert,      "Alert" },
	{ st_log_level_critical,   "Critical" },
	{ st_log_level_debug,      "Debug" },
	{ st_log_level_emergencey, "Emergency" },
	{ st_log_level_error,      "Error" },
	{ st_log_level_info,       "Info" },
	{ st_log_level_notice,     "Notice" },
	{ st_log_level_warning,    "Warning" },

	{ st_log_level_unknown, "Unknown level" },
};

static struct st_log_type2 {
	enum st_log_type type;
	const char * name;
} st_log_types[] = {
	{ st_log_type_changer,         "Changer" },
	{ st_log_type_daemon,          "Daemon" },
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


__asm__(".symver st_log_configure_v1, st_log_configure@@LIBSTONE_1.0");
void st_log_configure_v1(struct st_value * config) {
	pthread_mutex_lock(&st_log_lock);

	if (st_log_socket < 0) {
		st_log_socket = st_socket_v1(config);
		st_thread_pool_run2("logger", st_log_send_message, NULL, 4);
	}

	pthread_mutex_unlock(&st_log_lock);
}

__asm__(".symver st_log_level_to_string_v1, st_log_level_to_string@@LIBSTONE_1.0");
const char * st_log_level_to_string_v1(enum st_log_level level) {
	unsigned int i;
	for (i = 0; st_log_levels[i].level != st_log_level_unknown; i++)
		if (st_log_levels[i].level == level)
			return st_log_levels[i].name;

	return st_log_levels[i].name;
}

__asm__(".symver st_log_string_to_level_v1, st_log_string_to_level@@LIBSTONE_1.0");
enum st_log_level st_log_string_to_level_v1(const char * level) {
	if (level == NULL)
		return st_log_level_unknown;

	unsigned int i;
	for (i = 0; st_log_levels[i].level != st_log_level_unknown; i++)
		if (!strcasecmp(st_log_levels[i].name, level))
			return st_log_levels[i].level;

	return st_log_levels[i].level;
}

__asm__(".symver st_log_string_to_type_v1, st_log_string_to_type@@LIBSTONE_1.0");
enum st_log_type st_log_string_to_type_v1(const char * type) {
	if (type == NULL)
		return st_log_type_unknown;

	unsigned int i;
	for (i = 0; st_log_types[i].type != st_log_type_unknown; i++)
		if (!strcasecmp(type, st_log_types[i].name))
			return st_log_types[i].type;

	return st_log_types[i].type;
}

static void st_log_send_message(void * arg __attribute__((unused))) {
	struct st_value * messages = NULL;

	pthread_mutex_lock(&st_log_lock);
	for (;;) {
		struct st_value * tmp_messages = st_log_messages;
		st_log_messages = messages;
		messages = tmp_messages;

		if (messages == NULL) {
			pthread_cond_wait(&st_log_wait, &st_log_lock);
			continue;
		}

		unsigned int nb_messages = st_value_list_get_length_v1(messages);
		if (nb_messages == 0 && st_log_finished)
			break;
		if (nb_messages == 0)
			pthread_cond_wait(&st_log_wait, &st_log_lock);

		pthread_mutex_unlock(&st_log_lock);

		struct st_value_iterator * iter = st_value_list_get_iterator_v1(messages);
		while (st_value_iterator_has_next_v1(iter)) {
			struct st_value * message = st_value_iterator_get_value_v1(iter, false);
			st_json_encode_to_fd_v1(message, st_log_socket);
			struct st_value * returned = st_json_parse_fd_v1(st_log_socket, 5000);
			st_value_free_v1(returned);
		}
		st_value_iterator_free_v1(iter);
		st_value_list_clear_v1(messages);
	}

	pthread_cond_signal(&st_log_wait);
	pthread_mutex_unlock(&st_log_lock);
}

__asm__(".symver st_log_type_to_string_v1, st_log_type_to_string@@LIBSTONE_1.0");
const char * st_log_type_to_string_v1(enum st_log_type type) {
	unsigned int i;
	for (i = 0; st_log_types[i].type != st_log_type_unknown; i++)
		if (st_log_types[i].type == type)
			return st_log_types[i].name;

	return st_log_types[i].name;
}

__asm__(".symver st_log_write_v1, st_log_write@@LIBSTONE_1.0");
void st_log_write_v1(enum st_log_level level, enum st_log_type type, const char * format, ...) {
	if (st_log_finished)
		return;

	char * str_message = NULL;

	long long int timestamp = time(NULL);

	va_list va;
	va_start(va, format);
	vasprintf(&str_message, format, va);
	va_end(va);

	struct st_value * message = st_value_pack_v1("{sssssiss}", "level", st_log_level_to_string_v1(level), "type", st_log_type_to_string_v1(type), "timestamp", timestamp, "message", str_message);

	free(str_message);

	pthread_mutex_lock(&st_log_lock);

	if (st_log_messages == NULL)
		st_log_messages = st_value_new_linked_list_v1();
	st_value_list_push_v1(st_log_messages, message, true);

	pthread_cond_signal(&st_log_wait);
	pthread_mutex_unlock(&st_log_lock);
}

