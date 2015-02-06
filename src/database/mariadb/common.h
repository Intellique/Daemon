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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __STONE_DB_MARIADB_CONNNECTION_H__
#define __STONE_DB_MARIADB_CONNNECTION_H__

// bool
#include <stdbool.h>
// mysql_*
#include <mysql/mysql.h>

#include <libstone/database.h>
#include <libstone/value.h>

struct st_database_mariadb_config_private {
	char * user;
	char * password;
	char * db;
	char * host;
	int port;
};

struct st_database_mariadb_connection_private {
	MYSQL handler;
	bool closed;
	struct st_value * cached_query;
};

void st_database_mariadb_config_free(void * data);
struct st_database_config * st_database_mariadb_config_init(struct st_value * params);

struct st_database_connection * st_database_mariadb_connnect_init(struct st_database_mariadb_config_private * config, struct st_database_mariadb_connection_private * self);

void st_database_mariadb_util_prepare_string(MYSQL_BIND * bind, const char * string);

#endif

