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
*  Last modified: Mon, 15 Apr 2013 22:56:08 +0200                            *
\****************************************************************************/

// PQresultErrorField
#include <postgresql/libpq-fe.h>
// free
#include <stdlib.h>
// strdup, strcmp, strtok_r
#include <string.h>

#include <libstone/log.h>
#include <libstone/util/debug.h>

#include "common.h"

void st_db_postgresql_get_error(PGresult * result, const char * prepared_query) {
	char * error_code = PQresultErrorField(result, PG_DIAG_SQLSTATE);
	if (error_code != NULL && !strcmp("55P03", error_code))
		return;

	char * error = PQresultErrorField(result, PG_DIAG_MESSAGE_PRIMARY);
	if (prepared_query == NULL)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: error {%s} => %s", prepared_query, error);
	else
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: error => %s", error);

	error = PQresultErrorField(result, PG_DIAG_MESSAGE_DETAIL);
	if (error != NULL) {
		error = strdup(error);
		char * ptr;
		char * line = strtok_r(error, "\n", &ptr);
		while (line) {
			st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: detail => %s", line);
			line = strtok_r(NULL, "\n", &ptr);
		}
		free(error);
	}

	error = PQresultErrorField(result, PG_DIAG_MESSAGE_HINT);
	if (error != NULL) {
		error = strdup(error);
		char * ptr;
		char * line = strtok_r(error, "\n", &ptr);
		while (line) {
			st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: hint => %s", line);
			line = strtok_r(NULL, "\n", &ptr);
		}
		free(error);
	}

	st_debug_log_stack(16);
}

