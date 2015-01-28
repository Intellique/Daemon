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
*  Copyright (C) 2013-2015, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Wed, 05 Feb 2014 17:22:15 +0100                            *
\****************************************************************************/

#define _XOPEN_SOURCE 500
// PQgetvalue
#include <postgresql/libpq-fe.h>
// strcmp, strdup, strncpy
#include <string.h>
// bzero
#include <strings.h>
// mktime, strptime
#include <time.h>

#include <libstone/job.h>
#include <libstone/script.h>

#include "common.h"

static const struct st_db_postgresql_job_record_notif {
	char * name;
	enum st_job_record_notif notif;
} st_db_postgresql_job_record_notifs[] = {
	{ "important", st_job_record_notif_important },
	{ "normal",    st_job_record_notif_normal },
	{ "read",      st_job_record_notif_read },

	{ "unknwon", st_job_record_notif_unknown },
};

static const struct st_db_postgresql_script_type {
	char * name;
	enum st_script_type type;
} st_db_postgresql_script_types[] = {
	{ "on error", st_script_type_on_error, },
	{ "post job", st_script_type_post, },
	{ "pre job",  st_script_type_pre, },

	{ "unknown", st_script_type_unknown },
};


int st_db_postgresql_get_bool(PGresult * result, int row, int column, bool * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL)
		*val = strcmp(value, "t") ? false : true;

	return value != NULL;
}

int st_db_postgresql_get_double(PGresult * result, int row, int column, double * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && sscanf(value, "%lg", val) == 1)
		return 0;

	return value != NULL;
}

int st_db_postgresql_get_float(PGresult * result, int row, int column, float * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && sscanf(value, "%g", val) == 1)
		return 0;

	return value != NULL;
}

int st_db_postgresql_get_int(PGresult * result, int row, int column, int * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && sscanf(value, "%d", val) == 1)
		return 0;

	return value != NULL;
}

int st_db_postgresql_get_long(PGresult * result, int row, int column, long * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && sscanf(value, "%ld", val) == 1)
		return 0;

	return value != NULL;
}

int st_db_postgresql_get_long_add(PGresult * result, int row, int column, long * val) {
	long current = 0;

	int failed = st_db_postgresql_get_long(result, row, column, &current);

	if (failed == 0 && val != NULL)
		*val += current;

	return failed;
}

int st_db_postgresql_get_ssize(PGresult * result, int row, int column, ssize_t * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && sscanf(value, "%zd", val) == 1)
		return 0;

	return value != NULL;
}

int st_db_postgresql_get_string(PGresult * result, int row, int column, char * string, size_t length) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL) {
		strncpy(string, value, length);
		string[length] = '\0';
	}

	return value != NULL;
}

int st_db_postgresql_get_string_dup(PGresult * result, int row, int column, char ** string) {
	if (row < 0 || column < 0)
		return -1;

	if (PQgetisnull(result, row, column))
		return 1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL)
		*string = strdup(value);

	return value != NULL;
}

int st_db_postgresql_get_time(PGresult * result, int row, int column, time_t * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	struct tm tv;
	bzero(&tv, sizeof(struct tm));
	int failed = strptime(value, "%F %T", &tv) ? 0 : 1;

	if (!failed) {
		*val = mktime(&tv);

		if (tv.tm_isdst)
			*val -= 3600 * tv.tm_isdst;
	}

	return failed;
}

int st_db_postgresql_get_time_max(PGresult * result, int row, int column, time_t * val) {
	time_t current = 0;

	int failed = st_db_postgresql_get_time(result, row, column, &current);
	if (failed == 0 && val != NULL) {
		if (*val < current)
			*val = current;
	}

	return failed;
}

int st_db_postgresql_get_uchar(PGresult * result, int row, int column, unsigned char * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && sscanf(value, "%hhu", val) == 1)
		return 0;

	return value != NULL;
}

int st_db_postgresql_get_uint(PGresult * result, int row, int column, unsigned int * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value != NULL && sscanf(value, "%u", val) == 1)
		return 0;

	return value != NULL;
}

int st_db_postgresql_get_uint_add(PGresult * result, int row, int column, unsigned int * val) {
	unsigned int current = 0;

	int failed = st_db_postgresql_get_uint(result, row, column, &current);

	if (failed == 0 && val != NULL)
		*val += current;

	return failed;
}

const char * st_db_postgresql_job_record_notif_to_string(enum st_job_record_notif notif) {
	unsigned int i;
	for (i = 0; st_db_postgresql_job_record_notifs[i].notif != st_job_record_notif_unknown; i++)
		if (st_db_postgresql_job_record_notifs[i].notif == notif)
			return st_db_postgresql_job_record_notifs[i].name;

	return st_db_postgresql_job_record_notifs[i].name;
}

const char * st_db_postgresql_script_type_to_string(enum st_script_type type) {
	unsigned int i;
	for (i = 0; st_db_postgresql_script_types[i].type != st_script_type_unknown; i++)
		if (st_db_postgresql_script_types[i].type == type)
			return st_db_postgresql_script_types[i].name;

	return st_db_postgresql_script_types[i].name;
}

