/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Thu, 24 Nov 2011 12:50:15 +0100                         *
\*************************************************************************/

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

static void sa_log_exit(void) __attribute__((destructor));
static int sa_log_flush_message(void);
static void sa_log_store_message(char * who, enum sa_log_level level, char * message);

static int sa_log_display_at_exit = 1;
static struct sa_log_driver ** sa_log_drivers = 0;
static unsigned int sa_log_nb_drivers = 0;
static struct sa_log_message_unsent {
	char * who;
	enum sa_log_level level;
	char * message;
	char sent;
} * sa_log_message_unsent = 0;
static unsigned int sa_log_nb_message_unsent = 0;
static unsigned int sa_log_nb_message_unsent_to = 0;

static struct sa_log_level2 {
	enum sa_log_level level;
	const char * name;
} sa_log_levels[] = {
	{ sa_log_level_debug,   "Debug" },
	{ sa_log_level_error,   "Error" },
	{ sa_log_level_info,    "Info" },
	{ sa_log_level_warning, "Warning" },

	{ sa_log_level_unknown, "Unknown" },
};


void sa_log_disable_display_log() {
	sa_log_display_at_exit = 0;
}

void sa_log_exit() {
	if (!sa_log_display_at_exit)
		return;

	unsigned int mes;
	for (mes = 0; mes < sa_log_nb_message_unsent; mes++)
		printf("%c: %s\n", sa_log_level_to_string(sa_log_message_unsent[mes].level)[0], sa_log_message_unsent[mes].message);
}

int sa_log_flush_message() {
	unsigned int mes, ok = 0;
	for (mes = 0; mes < sa_log_nb_message_unsent; mes++) {
		unsigned int dr;
		for (dr = 0; dr < sa_log_nb_drivers; dr++) {
			unsigned int mod;
			for (mod = 0; mod < sa_log_drivers[dr]->nb_modules; mod++) {
				if (sa_log_message_unsent[mes].who && strcmp(sa_log_message_unsent[mes].who, sa_log_drivers[dr]->modules[mod].alias))
					continue;
				if (sa_log_drivers[dr]->modules[mod].level >= sa_log_message_unsent[mes].level) {
					sa_log_drivers[dr]->modules[mod].ops->write(sa_log_drivers[dr]->modules + mod, sa_log_message_unsent[mes].level, sa_log_message_unsent[mes].message);
					sa_log_message_unsent[mes].sent = 1;
					ok = 1;
				}
			}
		}
	}

	if (ok) {
		for (mes = 0; mes < sa_log_nb_message_unsent; mes++) {
			if (sa_log_message_unsent[mes].sent) {
				if (sa_log_message_unsent[mes].who) {
					free(sa_log_message_unsent[mes].who);
					sa_log_nb_message_unsent_to--;
				}
				if (sa_log_message_unsent[mes].message)
					free(sa_log_message_unsent[mes].message);

				if (mes + 1 < sa_log_nb_message_unsent)
					memmove(sa_log_message_unsent + mes, sa_log_message_unsent + mes + 1, (sa_log_nb_message_unsent - mes - 1) * sizeof(struct sa_log_message_unsent));
				sa_log_nb_message_unsent--, mes--;
				if (sa_log_nb_message_unsent > 0)
					sa_log_message_unsent = realloc(sa_log_message_unsent, sa_log_nb_message_unsent * sizeof(struct sa_log_message_unsent));
			}
		}

		if (sa_log_nb_message_unsent == 0) {
			free(sa_log_message_unsent);
			sa_log_message_unsent = 0;
			ok = 2;
		}
	}

	return ok;
}

struct sa_log_driver * sa_log_get_driver(const char * driver) {
	if (!driver) {
		sa_log_write_all(sa_log_level_error, "Log: Driver shall not be null");
		return 0;
	}

	unsigned int i;
	for (i = 0; i < sa_log_nb_drivers; i++)
		if (!strcmp(driver, sa_log_drivers[i]->name))
			return sa_log_drivers[i];
	void * cookie = sa_loader_load("log", driver);
	if (!cookie) {
		sa_log_write_all(sa_log_level_error, "Log: Failed to load driver %s", driver);
		return 0;
	}
	for (i = 0; i < sa_log_nb_drivers; i++)
		if (!strcmp(driver, sa_log_drivers[i]->name)) {
			struct sa_log_driver * dr = sa_log_drivers[i];
			dr->cookie = cookie;
			return dr;
		}
	sa_log_write_all(sa_log_level_error, "Log: Driver %s not found", driver);
	return 0;
}

const char * sa_log_level_to_string(enum sa_log_level level) {
	struct sa_log_level2 * ptr;
	for (ptr = sa_log_levels; ptr->level != sa_log_level_unknown; ptr++)
		if (ptr->level == level)
			return ptr->name;
	return ptr->name;
}

void sa_log_register_driver(struct sa_log_driver * driver) {
	if (!driver) {
		sa_log_write_all(sa_log_level_error, "Log: Try to register with driver=0");
		return;
	}

	if (driver->api_version != STORIQARCHIVER_LOG_APIVERSION) {
		sa_log_write_all(sa_log_level_error, "Log: Driver(%s) has not the correct api version (current: %d, expected: %d)", driver->name, driver->api_version, STORIQARCHIVER_LOG_APIVERSION);
		return;
	}

	unsigned int i;
	for (i = 0; i < sa_log_nb_drivers; i++)
		if (sa_log_drivers[i] == driver || !strcmp(driver->name, sa_log_drivers[i]->name)) {
			sa_log_write_all(sa_log_level_info, "Log: Driver(%s) is already registred", driver->name);
			return;
		}

	sa_log_drivers = realloc(sa_log_drivers, (sa_log_nb_drivers + 1) * sizeof(struct sa_log_driver *));
	sa_log_drivers[sa_log_nb_drivers] = driver;
	sa_log_nb_drivers++;

	sa_loader_register_ok();

	sa_log_write_all(sa_log_level_info, "Log: Driver(%s) is now registred", driver->name);
}

void sa_log_store_message(char * who, enum sa_log_level level, char * message) {
	sa_log_message_unsent = realloc(sa_log_message_unsent, (sa_log_nb_message_unsent + 1) * sizeof(struct sa_log_message_unsent));
	sa_log_message_unsent[sa_log_nb_message_unsent].who = who;
	sa_log_message_unsent[sa_log_nb_message_unsent].level = level;
	sa_log_message_unsent[sa_log_nb_message_unsent].message = message;
	sa_log_message_unsent[sa_log_nb_message_unsent].sent = 0;
	sa_log_nb_message_unsent++;
	if (who)
		sa_log_nb_message_unsent_to++;
}

enum sa_log_level sa_log_string_to_level(const char * string) {
	if (!string)
		return sa_log_level_unknown;

	struct sa_log_level2 * ptr;
	for (ptr = sa_log_levels; ptr->level != sa_log_level_unknown; ptr++)
		if (!strcasecmp(ptr->name, string))
			return ptr->level;
	return sa_log_level_unknown;
}

void sa_log_write_all(enum sa_log_level level, const char * format, ...) {
	char * message = malloc(256);

	va_list va;
	va_start(va, format);
	vsnprintf(message, 256, format, va);
	va_end(va);

	if (sa_log_nb_drivers == 0 || (sa_log_message_unsent && !sa_log_flush_message())) {
		sa_log_store_message(0, level, message);
		return;
	}

	unsigned int i, sent = 0;
	for (i = 0; i < sa_log_nb_drivers; i++) {
		unsigned int j;
		for (j = 0; j < sa_log_drivers[i]->nb_modules; j++) {
			if (sa_log_drivers[i]->modules[j].level >= level) {
				sa_log_drivers[i]->modules[j].ops->write(sa_log_drivers[i]->modules + j, level, message);
				sent = 1;
			}
		}
	}

	if (!sent)
		sa_log_store_message(0, level, message);

	if (sent)
		free(message);
}

