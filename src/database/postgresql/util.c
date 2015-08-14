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
#define _XOPEN_SOURCE 500
// freelocale, newlocale, uselocale
#include <locale.h>
// PQgetvalue
#include <libpq-fe.h>
// va_end, va_start
#include <stdarg.h>
// asprintf
#include <stdio.h>
// malloc
#include <stdlib.h>
// memmove, strchr, strcmp, strcspn, strdup, strlen, strncpy, strstr
#include <string.h>
// bzero
#include <strings.h>
// mktime, strptime
#include <time.h>

#include <libstoriqone/changer.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/string.h>

#include "common.h"

static int so_database_postgresql_get(const char * string, const char * format, ...) __attribute__((format (scanf, 2, 3)));

static void so_database_postgresql_util_init(void) __attribute__((constructor));


static struct so_database_postgresql_changer_action2 {
	unsigned long long hash;
	const char * name;
	const enum so_changer_action action;
} so_database_postgresql_changer_actions[] = {
	[so_changer_action_none]        = { 0, "none",        so_changer_action_none },
	[so_changer_action_put_offline] = { 0, "put offline", so_changer_action_put_offline },
	[so_changer_action_put_online]  = { 0, "put online",  so_changer_action_put_online },

	[so_changer_action_unknown] = { 0, "unknown action", so_changer_action_unknown },
};
static const unsigned int so_database_postgresql_changer_nb_actions = sizeof(so_database_postgresql_changer_actions) / sizeof(*so_database_postgresql_changer_actions);

static struct so_database_postgresql_changer_status2 {
	unsigned long long hash;
	const char * name;
	const enum so_changer_status status;
} so_database_postgresql_changer_status[] = {
	[so_changer_status_error]      = { 0, "error",      so_changer_status_error },
	[so_changer_status_exporting]  = { 0, "exporting",  so_changer_status_exporting },
	[so_changer_status_go_offline] = { 0, "go offline", so_changer_status_go_offline },
	[so_changer_status_go_online]  = { 0, "go online",  so_changer_status_go_online },
	[so_changer_status_idle]       = { 0, "idle",       so_changer_status_idle },
	[so_changer_status_importing]  = { 0, "importing",  so_changer_status_importing },
	[so_changer_status_loading]    = { 0, "loading",    so_changer_status_loading },
	[so_changer_status_offline]    = { 0, "offline",    so_changer_status_offline },
	[so_changer_status_unloading]  = { 0, "unloading",  so_changer_status_unloading },

	[so_changer_status_unknown] = { 0, "unknown", so_changer_status_unknown },
};
static const unsigned int so_database_postgresql_changer_nb_status = sizeof(so_database_postgresql_changer_status) / sizeof(*so_database_postgresql_changer_status);

static struct so_database_postgresql_drive_status2 {
	unsigned long long hash;
	const char * name;
	const enum so_drive_status status;
} so_database_postgresql_drive_status[] = {
	[so_drive_status_cleaning]    = { 0, "cleaning",    so_drive_status_cleaning },
	[so_drive_status_empty_idle]  = { 0, "empty idle",  so_drive_status_empty_idle },
	[so_drive_status_erasing]     = { 0, "erasing",     so_drive_status_erasing },
	[so_drive_status_error]       = { 0, "error",       so_drive_status_error },
	[so_drive_status_loaded_idle] = { 0, "loaded idle", so_drive_status_loaded_idle },
	[so_drive_status_loading]     = { 0, "loading",     so_drive_status_loading },
	[so_drive_status_positioning] = { 0, "positioning", so_drive_status_positioning },
	[so_drive_status_reading]     = { 0, "reading",     so_drive_status_reading },
	[so_drive_status_rewinding]   = { 0, "rewinding",   so_drive_status_rewinding },
	[so_drive_status_unloading]   = { 0, "unloading",   so_drive_status_unloading },
	[so_drive_status_writing]     = { 0, "writing",     so_drive_status_writing },

	[so_drive_status_unknown] = { 0, "unknown", so_drive_status_unknown },
};
static const unsigned int so_database_postgresql_drive_nb_status = sizeof(so_database_postgresql_drive_status) / sizeof(*so_database_postgresql_drive_status);

static struct so_database_postgresql_log_level2 {
	unsigned long long hash;
	const char * name;
	enum so_log_level level;
} so_database_postgresql_log_levels[] = {
	[so_log_level_alert]      = { 0, "alert",     so_log_level_alert },
	[so_log_level_critical]   = { 0, "critical",  so_log_level_critical },
	[so_log_level_debug]      = { 0, "debug",     so_log_level_debug },
	[so_log_level_emergencey] = { 0, "emergency", so_log_level_emergencey },
	[so_log_level_error]      = { 0, "error",     so_log_level_error },
	[so_log_level_info]       = { 0, "info",      so_log_level_info },
	[so_log_level_notice]     = { 0, "notice",    so_log_level_notice },
	[so_log_level_warning]    = { 0, "warning",   so_log_level_warning },

	[so_log_level_unknown]    = { 0, "unknown",   so_log_level_unknown },
};
static const unsigned int so_database_postgresql_log_nb_level = sizeof(so_database_postgresql_log_levels) / sizeof(*so_database_postgresql_log_levels);


static void so_database_postgresql_util_init() {
	unsigned int i;
	for (i = 0; i < so_database_postgresql_changer_nb_actions; i++)
		so_database_postgresql_changer_actions[i].hash = so_string_compute_hash2(so_database_postgresql_changer_actions[i].name);

	for (i = 0; i < so_database_postgresql_changer_nb_status; i++)
		so_database_postgresql_changer_status[i].hash = so_string_compute_hash2(so_database_postgresql_changer_status[i].name);

	for (i = 0; i < so_database_postgresql_drive_nb_status; i++)
		so_database_postgresql_drive_status[i].hash = so_string_compute_hash2(so_database_postgresql_drive_status[i].name);

	for (i = 0; i < so_database_postgresql_log_nb_level; i++)
		so_database_postgresql_log_levels[i].hash = so_string_compute_hash2(so_database_postgresql_log_levels[i].name);
}


const char * so_database_postgresql_bool_to_string(bool value) {
	if (value)
		return "TRUE";
	else
		return "FALSE";
}


static int so_database_postgresql_get(const char * string, const char * format, ...) {
	locale_t c_locale = newlocale(LC_ALL, "C", NULL);
	locale_t current_locale = uselocale(c_locale);

	va_list va;
	va_start(va, format);

	int nb_parsed = vsscanf(string, format, va);

	va_end(va);

	freelocale(c_locale);
	uselocale(current_locale);

	return nb_parsed;
}

int so_database_postgresql_get_bool(PGresult * result, int row, int column, bool * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL)
		*val = strcmp(value, "t") ? false : true;

	return value != NULL;
}

int so_database_postgresql_get_double(PGresult * result, int row, int column, double * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && so_database_postgresql_get(value, "%lg", val) == 1)
		return 0;

	return value != NULL;
}

int so_database_postgresql_get_float(PGresult * result, int row, int column, float * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && so_database_postgresql_get(value, "%g", val) == 1)
		return 0;

	return value != NULL;
}

int so_database_postgresql_get_int(PGresult * result, int row, int column, int * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && sscanf(value, "%d", val) == 1)
		return 0;

	return value != NULL;
}

int so_database_postgresql_get_long(PGresult * result, int row, int column, long * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && sscanf(value, "%ld", val) == 1)
		return 0;

	return value != NULL;
}

int so_database_postgresql_get_long_add(PGresult * result, int row, int column, long * val) {
	long current = 0;

	int failed = so_database_postgresql_get_long(result, row, column, &current);

	if (failed == 0 && val != NULL)
		*val += current;

	return failed;
}

int so_database_postgresql_get_long_long(PGresult * result, int row, int column, long long * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && sscanf(value, "%lld", val) == 1)
		return 0;

	return value != NULL;
}

int so_database_postgresql_get_size(PGresult * result, int row, int column, size_t * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && sscanf(value, "%zd", val) == 1)
		return 0;

	return value != NULL;
}

int so_database_postgresql_get_ssize(PGresult * result, int row, int column, ssize_t * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && sscanf(value, "%zd", val) == 1)
		return 0;

	return value != NULL;
}

int so_database_postgresql_get_string(PGresult * result, int row, int column, char * string, size_t length) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL) {
		strncpy(string, value, length);
		string[length] = '\0';
	}

	return value != NULL;
}

int so_database_postgresql_get_string_dup(PGresult * result, int row, int column, char ** string) {
	if (row < 0 || column < 0)
		return -1;

	if (PQgetisnull(result, row, column))
		return 1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL)
		*string = strdup(value);

	return value != NULL;
}

int so_database_postgresql_get_time(PGresult * result, int row, int column, time_t * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	struct tm tv;
	bzero(&tv, sizeof(struct tm));

	char * not_parsed = strptime(value, "%F %T", &tv);
	if (not_parsed != NULL && strlen(not_parsed) > 0) {
		size_t skip = strcspn(not_parsed, "+-");
		not_parsed = strptime(not_parsed + skip, "%z", &tv);
	}

	if (not_parsed != NULL) {
		tv.tm_sec -= tv.tm_gmtoff;
		tv.tm_gmtoff = 0;

		*val = mktime(&tv) - timezone;
	}

	return not_parsed != NULL;
}

int so_database_postgresql_get_time_max(PGresult * result, int row, int column, time_t * val) {
	time_t current = 0;

	int failed = so_database_postgresql_get_time(result, row, column, &current);
	if (failed == 0 && val != NULL) {
		if (*val < current)
			*val = current;
	}

	return failed;
}

int so_database_postgresql_get_uchar(PGresult * result, int row, int column, unsigned char * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && sscanf(value, "%hhu", val) == 1)
		return 0;

	return value != NULL;
}

int so_database_postgresql_get_uint(PGresult * result, int row, int column, unsigned int * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && sscanf(value, "%u", val) == 1)
		return 0;

	return value != NULL;
}

int so_database_postgresql_get_uint_add(PGresult * result, int row, int column, unsigned int * val) {
	unsigned int current = 0;

	int failed = so_database_postgresql_get_uint(result, row, column, &current);

	if (failed == 0 && val != NULL)
		*val += current;

	return failed;
}


char * so_database_postgresql_set_float(double fl) {
	locale_t c_locale = newlocale(LC_ALL, "C", NULL);
	locale_t current_locale = uselocale(c_locale);

	char * str_float = NULL;
	int size = asprintf(&str_float, "%lf", fl);

	freelocale(c_locale);
	uselocale(current_locale);

	if (size >= 0)
		return str_float;
	else
		return NULL;
}


const char * so_database_postgresql_changer_action_to_string(enum so_changer_action action) {
	return so_database_postgresql_changer_actions[action].name;
}

const char * so_database_postgresql_changer_status_to_string(enum so_changer_status status) {
	return so_database_postgresql_changer_status[status].name;
}

enum so_changer_action so_database_postgresql_string_to_action(const char * action) {
	if (action == NULL)
		return so_changer_action_unknown;

	unsigned int i;
	const unsigned long long hash = so_string_compute_hash2(action);

	for (i = 0; i < so_database_postgresql_changer_nb_actions; i++)
		if (hash == so_database_postgresql_changer_actions[i].hash)
			return so_database_postgresql_changer_actions[i].action;

	return so_changer_action_unknown;
}

enum so_changer_status so_database_postgresql_string_to_status(const char * status) {
	if (status == NULL)
		return so_changer_status_unknown;

	unsigned int i;
	const unsigned long long hash = so_string_compute_hash2(status);

	for (i = 0; i < so_database_postgresql_changer_nb_status; i++)
		if (hash == so_database_postgresql_changer_status[i].hash)
			return so_database_postgresql_changer_status[i].status;

	return so_changer_status_unknown;
}

const char * so_database_postgresql_drive_status_to_string(enum so_drive_status status) {
	return so_database_postgresql_drive_status[status].name;
}

const char * so_database_postgresql_log_level_to_string(enum so_log_level level) {
	return so_database_postgresql_log_levels[level].name;
}

