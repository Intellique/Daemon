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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Sun, 09 Sep 2012 23:04:45 +0200                            *
\****************************************************************************/

#ifndef __STONE_DB_POSTGRESQL_CONNNECTION_H__
#define __STONE_DB_POSTGRESQL_CONNNECTION_H__

// bool
#include <stdbool.h>
// ssize_t, time_t
#include <sys/types.h>

#include <libstone/database.h>

typedef struct pg_conn PGconn;
typedef struct pg_result PGresult;
struct st_hashtable;


struct st_stream_reader * st_db_postgresql_backup_init(PGconn * pg_connect);

/**
 * \brief Initialize a postgresql config
 *
 * \returns 0 if ok
 */
int st_db_postgresql_config_init(struct st_database_config * config, const struct st_hashtable * params);

struct st_database_connection * st_db_postgresql_connnect_init(PGconn * pg_connect);

int st_db_postgresql_get_bool(PGresult * result, int row, int column, bool * value);

int st_db_postgresql_get_double(PGresult * result, int row, int column, double * value);

void st_db_postgresql_get_error(PGresult * result, const char * prepared_query);

int st_db_postgresql_get_float(PGresult * result, int row, int column, float * value);

int st_db_postgresql_get_long(PGresult * result, int row, int column, long * value);

int st_db_postgresql_get_ssize(PGresult * result, int row, int column, ssize_t * value);

int st_db_postgresql_get_string(PGresult * result, int row, int column, char * string, size_t length);

int st_db_postgresql_get_string_dup(PGresult * result, int row, int column, char ** string);

int st_db_postgresql_get_time(PGresult * result, int row, int column, time_t * value);

int st_db_postgresql_get_uchar(PGresult * result, int row, int column, unsigned char * value);

int st_db_postgresql_get_uint(PGresult * result, int row, int column, unsigned int * value);

#endif

