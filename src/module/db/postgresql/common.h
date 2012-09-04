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
*  Last modified: Thu, 16 Aug 2012 16:34:55 +0200                         *
\*************************************************************************/

#ifndef __STONE_DB_POSTGRESQL_CONNNECTION_H__
#define __STONE_DB_POSTGRESQL_CONNNECTION_H__

#include <postgresql/libpq-fe.h>

#include <stone/database.h>
#include <stone/util.h>
#include <stone/util/hashtable.h>

struct st_db_postgresql_private {
	char * user;
	char * password;
	char * db;
	char * host;
	char * port;
};

struct st_db_postgresql_connetion_private {
	PGconn * db_con;
	struct st_hashtable * cached;
};

struct st_stream_reader * st_db_postgresql_init_backup(struct st_db_postgresql_private * driver_private);
int st_db_postgresql_init_connection(struct st_database_connection * connection, struct st_db_postgresql_private * driver_private);
struct st_stream_writer * st_db_postgresql_init_restore(struct st_db_postgresql_private * driver_private);
void st_db_postgresql_pr_free(struct st_db_postgresql_private * self);

#endif

