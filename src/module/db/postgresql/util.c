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
*  Last modified: Mon, 06 Aug 2012 10:03:29 +0200                         *
\*************************************************************************/

#define _XOPEN_SOURCE 500
// PQgetvalue
#include <postgresql/libpq-fe.h>
// strcmp, strdup, strncpy
#include <string.h>
// mktime, strptime
#include <time.h>

#include "common.h"

int st_db_postgresql_get_bool(PGresult * result, int row, int column, unsigned char * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value)
		*val = strcmp(value, "t") ? 0 : 1;

	return value != 0;
}

int st_db_postgresql_get_double(PGresult * result, int row, int column, double * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%lg", val) == 1)
		return 0;

	return value != 0;
}

int st_db_postgresql_get_long(PGresult * result, int row, int column, long * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%ld", val) == 1)
		return 0;

	return value != 0;
}

int st_db_postgresql_get_ssize(PGresult * result, int row, int column, ssize_t * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%zd", val) == 1)
		return 0;

	return value != 0;
}

int st_db_postgresql_get_string(PGresult * result, int row, int column, char * string, size_t length) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value)
		strncpy(string, value, length);

	return value != 0;
}

int st_db_postgresql_get_string_dup(PGresult * result, int row, int column, char ** string) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value)
		*string = strdup(value);

	return value != 0;
}

int st_db_postgresql_get_time(PGresult * result, int row, int column, time_t * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	struct tm tv;

	int failed = strptime(value, "%F %T", &tv) ? 0 : 1;
	if (!failed)
		*val = mktime(&tv);

	return failed;
}

int st_db_postgresql_get_uchar(PGresult * result, int row, int column, unsigned char * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%hhu", val) == 1)
		return 0;

	return value != 0;
}

int st_db_postgresql_get_uint(PGresult * result, int row, int column, unsigned int * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%u", val) == 1)
		return 0;

	return value != 0;
}

