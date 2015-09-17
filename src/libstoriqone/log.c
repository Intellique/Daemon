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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// bindtextdomain, dgettext, gettext
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

#include <libstoriqone/file.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/thread_pool.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

#include "config.h"

static pthread_mutex_t so_log_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static pthread_cond_t so_log_wait = PTHREAD_COND_INITIALIZER;
static struct so_value * so_log_messages = NULL;
static enum so_log_type so_log_default_type = so_log_type_daemon;
static volatile bool so_log_finished = false;
static volatile bool so_log_running = false;

static void so_log_exit(void) __attribute__((destructor));
static void so_log_init(void) __attribute__((constructor));
static void so_log_send_message(void * arg);
static void so_log_write_inner(enum so_log_level level, enum so_log_type type, const char * format, va_list params);

static struct so_log_level2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	enum so_log_level level;
} so_log_levels[] = {
	[so_log_level_alert]      = { 0, 0, gettext_noop("Alert"),     NULL, so_log_level_alert },
	[so_log_level_critical]   = { 0, 0, gettext_noop("Critical"),  NULL, so_log_level_critical },
	[so_log_level_debug]      = { 0, 0, gettext_noop("Debug"),     NULL, so_log_level_debug },
	[so_log_level_emergencey] = { 0, 0, gettext_noop("Emergency"), NULL, so_log_level_emergencey },
	[so_log_level_error]      = { 0, 0, gettext_noop("Error"),     NULL, so_log_level_error },
	[so_log_level_info]       = { 0, 0, gettext_noop("Info"),      NULL, so_log_level_info },
	[so_log_level_notice]     = { 0, 0, gettext_noop("Notice"),    NULL, so_log_level_notice },
	[so_log_level_warning]    = { 0, 0, gettext_noop("Warning"),   NULL, so_log_level_warning },

	[so_log_level_unknown]    = { 0, 0, gettext_noop("Unknown level"), NULL, so_log_level_unknown },
};
static const unsigned int so_log_nb_levels = sizeof(so_log_levels) / sizeof(*so_log_levels);
static unsigned int so_log_level_max = 0;

static struct so_log_type2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	enum so_log_type type;
} so_log_types[] = {
	[so_log_type_changer]         = { 0, 0, gettext_noop("Changer"),         NULL, so_log_type_changer },
	[so_log_type_daemon]          = { 0, 0, gettext_noop("Daemon"),          NULL, so_log_type_daemon },
	[so_log_type_drive]           = { 0, 0, gettext_noop("Drive"),           NULL, so_log_type_drive },
	[so_log_type_job]             = { 0, 0, gettext_noop("Job"),             NULL, so_log_type_job },
	[so_log_type_logger]          = { 0, 0, gettext_noop("Logger"),          NULL, so_log_type_logger },
	[so_log_type_plugin_checksum] = { 0, 0, gettext_noop("Checksum Plugin"), NULL, so_log_type_plugin_checksum },
	[so_log_type_plugin_db]       = { 0, 0, gettext_noop("Database Plugin"), NULL, so_log_type_plugin_db },
	[so_log_type_plugin_log]      = { 0, 0, gettext_noop("Log Plugin"),      NULL, so_log_type_plugin_log },
	[so_log_type_scheduler]       = { 0, 0, gettext_noop("Scheduler"),       NULL, so_log_type_scheduler },

	[so_log_type_ui]	          = { 0, 0, gettext_noop("User Interface"), NULL, so_log_type_ui },
	[so_log_type_user_message]    = { 0, 0, gettext_noop("User Message"),   NULL, so_log_type_user_message },

	[so_log_type_unknown]         = { 0, 0, gettext_noop("Unknown type"), NULL, so_log_type_unknown },
};
static const unsigned int so_log_nb_types = sizeof(so_log_types) / sizeof(*so_log_types);
static unsigned int so_log_type_max = 0;


void so_log_configure(struct so_value * config, enum so_log_type default_type) {
	struct so_value * copy_config = so_value_copy(config, true);

	pthread_mutex_lock(&so_log_lock);

	if (!so_log_running) {
		so_log_running = true;
		so_log_default_type = default_type;
		so_thread_pool_run2("logger", so_log_send_message, copy_config, 4);
	}

	pthread_mutex_unlock(&so_log_lock);
}

static void so_log_exit() {
	so_value_free(so_log_messages);
	so_log_messages = NULL;
}

static void so_log_init() {
	setlocale(LC_ALL, "");
	bindtextdomain("libstoriqone", LOCALE_DIR);

	so_log_messages = so_value_new_linked_list();

	unsigned int i;
	for (i = 0; i < so_log_nb_levels; i++) {
		so_log_levels[i].hash = so_string_compute_hash2(so_log_levels[i].name);

		so_log_levels[i].translation = dgettext("libstoriqone", so_log_levels[i].name);
		so_log_levels[i].hash_translated = so_string_compute_hash2(so_log_levels[i].translation);

		unsigned int length = strlen(so_log_levels[i].translation);
		if (so_log_level_max < length)
			so_log_level_max = length;
	}

	for (i = 0; i < so_log_nb_types; i++) {
		so_log_types[i].hash = so_string_compute_hash2(so_log_types[i].name);

		so_log_types[i].translation = dgettext("libstoriqone", so_log_types[i].name);
		so_log_types[i].hash_translated = so_string_compute_hash2(so_log_types[i].translation);

		unsigned int length = strlen(so_log_types[i].translation);
		if (so_log_type_max < length)
			so_log_type_max = length;
	}
}

unsigned int so_log_level_max_length() {
	return so_log_level_max;
}

const char * so_log_level_to_string(enum so_log_level level, bool translate) {
	if (translate)
		return so_log_levels[level].translation;
	else
		return so_log_levels[level].name;
}

void so_log_stop_logger() {
	pthread_mutex_lock(&so_log_lock);

	if (so_log_running) {
		so_log_running = false;
		pthread_cond_signal(&so_log_wait);
		pthread_cond_wait(&so_log_wait, &so_log_lock);
	}

	pthread_mutex_unlock(&so_log_lock);
}

enum so_log_level so_log_string_to_level(const char * level, bool translate) {
	if (level == NULL)
		return so_log_level_unknown;

	unsigned int i;
	const unsigned long long hash = so_string_compute_hash2(level);

	if (translate) {
		for (i = 0; i < so_log_nb_levels; i++)
			if (hash == so_log_levels[i].hash_translated)
				return so_log_levels[i].level;
	} else {
		for (i = 0; i < so_log_nb_levels; i++)
			if (hash == so_log_levels[i].hash)
				return so_log_levels[i].level;
	}

	return so_log_level_unknown;
}

enum so_log_type so_log_string_to_type(const char * type, bool translate) {
	if (type == NULL)
		return so_log_type_unknown;

	unsigned int i;
	const unsigned long long hash = so_string_compute_hash2(type);

	if (translate) {
		for (i = 0; i < so_log_nb_types; i++)
			if (hash == so_log_types[i].hash_translated)
				return so_log_types[i].type;
	} else {
		for (i = 0; i < so_log_nb_types; i++)
			if (hash == so_log_types[i].hash)
				return so_log_types[i].type;
	}

	return so_log_type_unknown;
}

static void so_log_send_message(void * arg) {
	struct so_value * config = arg;
	struct so_value * messages = so_value_new_linked_list();
	int socket = -1;

	pthread_mutex_lock(&so_log_lock);
	for (;;) {
		struct so_value * tmp_messages = so_log_messages;
		so_log_messages = messages;
		messages = tmp_messages;

		unsigned int nb_messages = so_value_list_get_length(messages);
		if (nb_messages == 0 && !so_log_running)
			break;
		if (nb_messages == 0) {
			pthread_cond_wait(&so_log_wait, &so_log_lock);
			continue;
		}

		pthread_mutex_unlock(&so_log_lock);

		while (socket < 0) {
			socket = so_socket(config);
			if (socket < 0)
				sleep(1);

			if (!so_log_running)
				break;

			so_file_close_fd_on_exec(socket, true);
		}

		struct so_value_iterator * iter = so_value_list_get_iterator(messages);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * message = so_value_iterator_get_value(iter, false);
			char * str_message = so_json_encode_to_string(message);
			size_t str_message_length = strlen(str_message);

			ssize_t nb_send;
			do {
				nb_send = send(socket, str_message, str_message_length, MSG_NOSIGNAL);
				if (nb_send < 0) {
					close(socket);
					socket = -1;

					sleep(1);

					while (socket < 0) {
						socket = so_socket(config);
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
		so_value_iterator_free(iter);
		so_value_list_clear(messages);

		pthread_mutex_lock(&so_log_lock);
	}

	so_log_finished = true;

	pthread_cond_signal(&so_log_wait);
	pthread_mutex_unlock(&so_log_lock);

	so_value_free(messages);
	so_value_free(config);
}

unsigned int so_log_type_max_length() {
	return so_log_type_max;
}

const char * so_log_type_to_string(enum so_log_type type, bool translate) {
	if (translate)
		return so_log_types[type].translation;
	else
		return so_log_types[type].name;
}

void so_log_write(enum so_log_level level, const char * format, ...) {
	if (format == NULL || so_log_finished)
		return;

	va_list va;
	va_start(va, format);
	so_log_write_inner(level, so_log_default_type, format, va);
	va_end(va);
}

void so_log_write2(enum so_log_level level, enum so_log_type type, const char * format, ...) {
	if (format == NULL || so_log_finished)
		return;

	va_list va;
	va_start(va, format);
	so_log_write_inner(level, type, format, va);
	va_end(va);
}

static void so_log_write_inner(enum so_log_level level, enum so_log_type type, const char * format, va_list params) {
	long long int timestamp = time(NULL);

	char * str_message = NULL;
	int size = vasprintf(&str_message, format, params);

	if (size < 0)
		return;

	struct so_value * message = so_value_pack("{sssssIss}",
		"level", so_log_level_to_string(level, false),
		"type", so_log_type_to_string(type, false),
		"timestamp", timestamp,
		"message", str_message
	);

	free(str_message);

	pthread_mutex_lock(&so_log_lock);

	so_value_list_push(so_log_messages, message, true);

	pthread_cond_signal(&so_log_wait);
	pthread_mutex_unlock(&so_log_lock);
}

