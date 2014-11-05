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
// setlocale
#include <locale.h>
// pthread_mutex_lock, pthread_mutex_unlock
#include <pthread.h>
// va_end, va_start
#include <stdarg.h>
// vasprintf
#include <stdio.h>
// free
#include <stdlib.h>
// strcasecmp, strlen
#include <string.h>
// recv, send
#include <sys/socket.h>
// recv, send
#include <sys/types.h>
// time
#include <time.h>
// close, sleep
#include <unistd.h>

#define gettext_noop(String) String

#include <libstone/thread_pool.h>

#include "json.h"
#include "log.h"
#include "socket.h"
#include "string.h"

#include "config.h"

static pthread_mutex_t st_log_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static pthread_cond_t st_log_wait = PTHREAD_COND_INITIALIZER;
static struct st_value * st_log_messages = NULL;
static enum st_log_type st_log_default_type = st_log_type_daemon;
static volatile bool st_log_finished = false;
static volatile bool st_log_running = false;

static void st_log_exit(void) __attribute__((destructor));
static void st_log_init(void) __attribute__((constructor));
static void st_log_send_message(void * arg);
static void st_log_write_inner(enum st_log_level level, enum st_log_type type, const char * format, va_list params);

static struct st_log_level2 {
	unsigned long long hash;
	const char * name;
	enum st_log_level level;
} st_log_levels[] = {
	[st_log_level_alert]      = { 0, gettext_noop("Alert"),     st_log_level_alert },
	[st_log_level_critical]   = { 0, gettext_noop("Critical"),  st_log_level_critical },
	[st_log_level_debug]      = { 0, gettext_noop("Debug"),     st_log_level_debug },
	[st_log_level_emergencey] = { 0, gettext_noop("Emergency"), st_log_level_emergencey },
	[st_log_level_error]      = { 0, gettext_noop("Error"),     st_log_level_error },
	[st_log_level_info]       = { 0, gettext_noop("Info"),      st_log_level_info },
	[st_log_level_notice]     = { 0, gettext_noop("Notice"),    st_log_level_notice },
	[st_log_level_warning]    = { 0, gettext_noop("Warning"),   st_log_level_warning },

	[st_log_level_unknown]    = { 0, gettext_noop("Unknown level"), st_log_level_unknown },
};
static unsigned int st_log_level_max = 0;

static struct st_log_type2 {
	unsigned long long hash;
	const char * name;
	enum st_log_type type;
} st_log_types[] = {
	[st_log_type_changer]         = { 0, gettext_noop("Changer"),         st_log_type_changer },
	[st_log_type_daemon]          = { 0, gettext_noop("Daemon"),          st_log_type_daemon },
	[st_log_type_drive]           = { 0, gettext_noop("Drive"),           st_log_type_drive },
	[st_log_type_job]             = { 0, gettext_noop("Job"),             st_log_type_job },
	[st_log_type_logger]          = { 0, gettext_noop("Logger"),          st_log_type_logger },
	[st_log_type_plugin_checksum] = { 0, gettext_noop("Plugin Checksum"), st_log_type_plugin_checksum },
	[st_log_type_plugin_db]       = { 0, gettext_noop("Plugin Database"), st_log_type_plugin_db },
	[st_log_type_plugin_log]      = { 0, gettext_noop("Plugin Log"),      st_log_type_plugin_log },
	[st_log_type_scheduler]       = { 0, gettext_noop("Scheduler"),       st_log_type_scheduler },

	[st_log_type_ui]	          = { 0, gettext_noop("User Interface"), st_log_type_ui },
	[st_log_type_user_message]    = { 0, gettext_noop("User Message"),   st_log_type_user_message },

	[st_log_type_unknown]         = { 0, gettext_noop("Unknown type"), st_log_type_unknown },
};
static unsigned int st_log_type_max = 0;


__asm__(".symver st_log_configure_v1, st_log_configure@@LIBSTONE_1.2");
void st_log_configure_v1(struct st_value * config, enum st_log_type default_type) {
	struct st_value * copy_config = st_value_copy(config, true);

	pthread_mutex_lock(&st_log_lock);

	if (!st_log_running) {
		st_log_running = true;
		st_log_default_type = default_type;
		st_thread_pool_run2("logger", st_log_send_message, copy_config, 4);
	}

	pthread_mutex_unlock(&st_log_lock);
}

static void st_log_exit() {
	st_value_free(st_log_messages);
	st_log_messages = NULL;
}

static void st_log_init() {
	setlocale(LC_ALL, "");
	bindtextdomain("libstone", LOCALE_DIR);
	textdomain("libstone");

	st_log_messages = st_value_new_linked_list();

	unsigned int i;
	for (i = 0; i < sizeof(st_log_levels) / sizeof(*st_log_levels); i++) {
		st_log_levels[i].hash = st_string_compute_hash2(st_log_levels[i].name);

		unsigned int length = strlen(gettext(st_log_levels[i].name));
		if (st_log_level_max < length)
			st_log_level_max = length;
	}

	for (i = 0; i < sizeof(st_log_types) / sizeof(*st_log_types); i++) {
		st_log_types[i].hash = st_string_compute_hash2(st_log_types[i].name);

		unsigned int length = strlen(gettext(st_log_types[i].name));
		if (st_log_type_max < length)
			st_log_type_max = length;
	}
}

__asm__(".symver st_log_level_max_length_v1, st_log_level_max_length@@LIBSTONE_1.2");
unsigned int st_log_level_max_length_v1() {
	return st_log_level_max;
}

__asm__(".symver st_log_level_to_string_v1, st_log_level_to_string@@LIBSTONE_1.2");
const char * st_log_level_to_string_v1(enum st_log_level level, bool translate) {
	const char * value = st_log_levels[level].name;
	if (translate)
		value = gettext(value);
	return value;
}

__asm__(".symver st_log_stop_logger_v1, st_log_stop_logger@@LIBSTONE_1.2");
void st_log_stop_logger_v1() {
	pthread_mutex_lock(&st_log_lock);

	if (st_log_running) {
		st_log_running = false;
		pthread_cond_signal(&st_log_wait);
		pthread_cond_wait(&st_log_wait, &st_log_lock);
	}

	pthread_mutex_unlock(&st_log_lock);
}

__asm__(".symver st_log_string_to_level_v1, st_log_string_to_level@@LIBSTONE_1.2");
enum st_log_level st_log_string_to_level_v1(const char * level) {
	if (level == NULL)
		return st_log_level_unknown;

	unsigned int i;
	const unsigned long long hash = st_string_compute_hash2(level);
	for (i = 0; i < sizeof(st_log_levels) / sizeof(*st_log_levels); i++)
		if (hash == st_log_levels[i].hash)
			return st_log_levels[i].level;

	return st_log_levels[i].level;
}

__asm__(".symver st_log_string_to_type_v1, st_log_string_to_type@@LIBSTONE_1.2");
enum st_log_type st_log_string_to_type_v1(const char * type) {
	if (type == NULL)
		return st_log_type_unknown;

	unsigned int i;
	const unsigned long long hash = st_string_compute_hash2(type);
	for (i = 0; i < sizeof(st_log_types) / sizeof(*st_log_types); i++)
		if (hash == st_log_types[i].hash)
			return st_log_types[i].type;

	return st_log_types[i].type;
}

static void st_log_send_message(void * arg) {
	struct st_value * config = arg;
	struct st_value * messages = st_value_new_linked_list();
	int socket = -1;

	pthread_mutex_lock(&st_log_lock);
	for (;;) {
		struct st_value * tmp_messages = st_log_messages;
		st_log_messages = messages;
		messages = tmp_messages;

		unsigned int nb_messages = st_value_list_get_length(messages);
		if (nb_messages == 0 && !st_log_running)
			break;
		if (nb_messages == 0) {
			pthread_cond_wait(&st_log_wait, &st_log_lock);
			continue;
		}

		pthread_mutex_unlock(&st_log_lock);

		while (socket < 0) {
			socket = st_socket_v1(config);
			if (socket < 0)
				sleep(1);
		}

		struct st_value_iterator * iter = st_value_list_get_iterator(messages);
		while (st_value_iterator_has_next(iter)) {
			struct st_value * message = st_value_iterator_get_value(iter, false);
			char * str_message = st_json_encode_to_string_v1(message);
			size_t str_message_length = strlen(str_message);

			ssize_t nb_send;
			do {
				nb_send = send(socket, str_message, str_message_length, MSG_NOSIGNAL);
				if (nb_send < 0) {
					close(socket);
					socket = -1;

					sleep(1);

					while (socket < 0) {
						socket = st_socket_v1(config);
						if (socket < 0)
							sleep(1);
					}
				}
			} while (nb_send < 0);
			free(str_message);

			char buffer[32];
			ssize_t nb_recv = recv(socket, buffer, 32, 0);
			if (nb_recv == 0) {
				close(socket);
				socket = -1;
			}
		}
		st_value_iterator_free(iter);
		st_value_list_clear(messages);

		pthread_mutex_lock(&st_log_lock);
	}

	st_log_finished = true;

	pthread_cond_signal(&st_log_wait);
	pthread_mutex_unlock(&st_log_lock);

	st_value_free(messages);
	st_value_free(config);
}

__asm__(".symver st_log_type_max_length_v1, st_log_type_max_length@@LIBSTONE_1.2");
unsigned int st_log_type_max_length_v1() {
	return st_log_type_max;
}

__asm__(".symver st_log_type_to_string_v1, st_log_type_to_string@@LIBSTONE_1.2");
const char * st_log_type_to_string_v1(enum st_log_type type, bool translate) {
	const char * value = st_log_types[type].name;
	if (translate)
		value = gettext(value);
	return value;
}

__asm__(".symver st_log_write_v1, st_log_write@@LIBSTONE_1.2");
void st_log_write_v1(enum st_log_level level, const char * format, ...) {
	if (st_log_finished)
		return;

	va_list va;
	va_start(va, format);
	st_log_write_inner(level, st_log_default_type, format, va);
	va_end(va);
}

__asm__(".symver st_log_write2_v1, st_log_write2@@LIBSTONE_1.2");
void st_log_write2_v1(enum st_log_level level, enum st_log_type type, const char * format, ...) {
	if (st_log_finished)
		return;

	va_list va;
	va_start(va, format);
	st_log_write_inner(level, type, format, va);
	va_end(va);
}

static void st_log_write_inner(enum st_log_level level, enum st_log_type type, const char * format, va_list params) {
	long long int timestamp = time(NULL);

	char * str_message = NULL;
	vasprintf(&str_message, format, params);

	struct st_value * message = st_value_pack("{sssssiss}", "level", st_log_level_to_string_v1(level, false), "type", st_log_type_to_string_v1(type, false), "timestamp", timestamp, "message", str_message);

	free(str_message);

	pthread_mutex_lock(&st_log_lock);

	st_value_list_push(st_log_messages, message, true);

	pthread_cond_signal(&st_log_wait);
	pthread_mutex_unlock(&st_log_lock);
}

