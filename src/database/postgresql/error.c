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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

// gettext
#include <libintl.h>
// PQresultErrorField
#include <postgresql/libpq-fe.h>
// free
#include <stdlib.h>
// strdup, strcmp, strtok_r
#include <string.h>

#include <libstoriqone/log.h>
#include <libstoriqone/debug.h>

#include "common.h"

void so_database_postgresql_get_error(PGresult * result, const char * prepared_query) {
	char * error_code = PQresultErrorField(result, PG_DIAG_SQLSTATE);
	if (error_code != NULL && !strcmp("55P03", error_code))
		return;

	char * error = PQresultErrorField(result, PG_DIAG_MESSAGE_PRIMARY);
	if (prepared_query == NULL)
		so_log_write2(so_log_level_error, so_log_type_plugin_db, gettext("PSQL: error {%s} => %s"), prepared_query, error);
	else
		so_log_write2(so_log_level_error, so_log_type_plugin_db, gettext("PSQL: error => %s"), error);

	error = PQresultErrorField(result, PG_DIAG_MESSAGE_DETAIL);
	if (error != NULL) {
		error = strdup(error);
		char * ptr;
		char * line = strtok_r(error, "\n", &ptr);
		while (line != NULL) {
			so_log_write2(so_log_level_error, so_log_type_plugin_db, gettext("PSQL: detail => %s"), line);
			line = strtok_r(NULL, "\n", &ptr);
		}
		free(error);
	}

	error = PQresultErrorField(result, PG_DIAG_MESSAGE_HINT);
	if (error != NULL) {
		error = strdup(error);
		char * ptr;
		char * line = strtok_r(error, "\n", &ptr);
		while (line != NULL) {
			so_log_write2(so_log_level_error, so_log_type_plugin_db, gettext("PSQL: hint => %s"), line);
			line = strtok_r(NULL, "\n", &ptr);
		}
		free(error);
	}

	so_debug_log_stack(16);
}

