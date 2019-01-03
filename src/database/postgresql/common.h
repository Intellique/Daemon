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
*  Copyright (C) 2013-2019, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __STORIQONE_DB_POSTGRESQL_CONNNECTION_H__
#define __STORIQONE_DB_POSTGRESQL_CONNNECTION_H__

// bool
#include <stdbool.h>
// ssize_t, time_t
#include <sys/types.h>

#include <libstoriqone/database.h>
#include <libstoriqone/log.h>

enum so_archive_status;
enum so_changer_action;
enum so_changer_status;
enum so_drive_status;
enum so_job_status;

typedef struct pg_conn PGconn;
typedef struct pg_result PGresult;


struct so_database_postgresql_config_private {
	char * user;
	char * password;
	char * db;
	char * host;
	char * port;
};

struct so_stream_reader * so_database_postgresql_backup_init(struct so_database_postgresql_config_private * config);

void so_database_postgresql_config_free(void * data);
/**
 * \brief Initialize a postgresql config
 *
 * \returns 0 if ok
 */
struct so_database_config * so_database_postgresql_config_init(struct so_value * params);

struct so_database_connection * so_database_postgresql_connect_init(PGconn * pg_connect);

const char * so_database_postgresql_bool_to_string(bool value);

int so_database_postgresql_get_bool(PGresult * result, int row, int column, bool * value);
int so_database_postgresql_get_double(PGresult * result, int row, int column, double * value);
void so_database_postgresql_get_error(PGresult * result, const char * prepared_query);
int so_database_postgresql_get_float(PGresult * result, int row, int column, float * value);
int so_database_postgresql_get_int(PGresult * result, int row, int column, int * value);
int so_database_postgresql_get_long(PGresult * result, int row, int column, long * value);
int so_database_postgresql_get_long_add(PGresult * result, int row, int column, long * value);
int so_database_postgresql_get_long_long(PGresult * result, int row, int column, long long * value);
int so_database_postgresql_get_size(PGresult * result, int row, int column, size_t * value);
int so_database_postgresql_get_ssize(PGresult * result, int row, int column, ssize_t * value);
int so_database_postgresql_get_string(PGresult * result, int row, int column, char * string, size_t length);
int so_database_postgresql_get_string_dup(PGresult * result, int row, int column, char ** string);
int so_database_postgresql_get_time(PGresult * result, int row, int column, time_t * value);
int so_database_postgresql_get_time_max(PGresult * result, int row, int column, time_t * value);
int so_database_postgresql_get_uchar(PGresult * result, int row, int column, unsigned char * value);
int so_database_postgresql_get_uint(PGresult * result, int row, int column, unsigned int * value);
int so_database_postgresql_get_uint_add(PGresult * result, int row, int column, unsigned int * value);

char * so_database_postgresql_set_float(double fl);

const char * so_database_postgresql_archive_status_to_string(enum so_archive_status status);
const char * so_database_postgresql_changer_action_to_string(enum so_changer_action action);
const char * so_database_postgresql_changer_status_to_string(enum so_changer_status status);
const char * so_database_postgresql_job_status_to_string(enum so_job_status status);
enum so_changer_action so_database_postgresql_string_to_action(const char * action);
enum so_archive_status so_database_postgresql_string_to_archive_status(const char * status);
enum so_job_status so_database_postgresql_string_to_job_status(const char * status);
enum so_changer_status so_database_postgresql_string_to_status(const char * status);
const char * so_database_postgresql_drive_status_to_string(enum so_drive_status status);
const char * so_database_postgresql_log_level_to_string(enum so_log_level level);
enum so_media_type so_database_postgresql_media_string_to_type(const char * type);
const char * so_database_postgresql_media_type_to_string(enum so_media_type type);

#endif
