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

#ifndef __STONE_DB_POSTGRESQL_CONNNECTION_H__
#define __STONE_DB_POSTGRESQL_CONNNECTION_H__

// bool
#include <stdbool.h>
// ssize_t, time_t
#include <sys/types.h>

#include <libstone/database.h>
#include <libstone/log.h>

typedef struct pg_conn PGconn;
typedef struct pg_result PGresult;


struct st_database_postgresql_config_private {
	char * user;
	char * password;
	char * db;
	char * host;
	char * port;
};


void st_database_postgresql_config_free(void * data);
/**
 * \brief Initialize a postgresql config
 *
 * \returns 0 if ok
 */
struct st_database_config * st_database_postgresql_config_init(struct st_value * params);

struct st_database_connection * st_database_postgresql_connect_init(PGconn * pg_connect);

const char * st_database_postgresql_bool_to_string(bool value);

int st_database_postgresql_get_bool(PGresult * result, int row, int column, bool * value);
int st_database_postgresql_get_double(PGresult * result, int row, int column, double * value);
void st_database_postgresql_get_error(PGresult * result, const char * prepared_query);
int st_database_postgresql_get_float(PGresult * result, int row, int column, float * value);
int st_database_postgresql_get_int(PGresult * result, int row, int column, int * value);
int st_database_postgresql_get_long(PGresult * result, int row, int column, long * value);
int st_database_postgresql_get_long_add(PGresult * result, int row, int column, long * value);
int st_database_postgresql_get_long_long(PGresult * result, int row, int column, long long * value);
int st_database_postgresql_get_ssize(PGresult * result, int row, int column, ssize_t * value);
int st_database_postgresql_get_string(PGresult * result, int row, int column, char * string, size_t length);
int st_database_postgresql_get_string_dup(PGresult * result, int row, int column, char ** string);
int st_database_postgresql_get_time(PGresult * result, int row, int column, time_t * value);
int st_database_postgresql_get_time_max(PGresult * result, int row, int column, time_t * value);
int st_database_postgresql_get_uchar(PGresult * result, int row, int column, unsigned char * value);
int st_database_postgresql_get_uint(PGresult * result, int row, int column, unsigned int * value);
int st_database_postgresql_get_uint_add(PGresult * result, int row, int column, unsigned int * value);

const char * st_database_postgresql_log_level_to_string(enum st_log_level level);

#endif

