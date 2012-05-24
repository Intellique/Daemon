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
*  Last modified: Wed, 23 May 2012 16:25:24 +0200                         *
\*************************************************************************/

// free, malloc
#include <malloc.h>
// strdup
#include <string.h>
// PQfinish, PQsetdbLogin, PQstatus
#include <postgresql/libpq-fe.h>

#include <stone/log.h>
#include <stone/util/hashtable.h>

#include "common.h"

static struct st_stream_reader * st_db_postgresql_backup_db(struct st_database * db);
static struct st_database_connection * st_db_postgresql_connect(struct st_database * db, struct st_database_connection * connection);
static void st_db_postgresql_init(void) __attribute__((constructor));
static int st_db_postgresql_ping(struct st_database * db);
static int st_db_postgresql_setup(struct st_database * db, struct st_hashtable * params);


static struct st_database_ops st_db_postgresql_ops = {
	.backup_db = st_db_postgresql_backup_db,
	.connect   = st_db_postgresql_connect,
	.ping      = st_db_postgresql_ping,
	.setup     = st_db_postgresql_setup,
};

static struct st_database st_db_postgresql = {
	.name        = "postgresql",
	.ops         = &st_db_postgresql_ops,
	.data        = 0,
	.cookie      = 0,
	.api_version = STONE_DATABASE_APIVERSION,
};


struct st_stream_reader * st_db_postgresql_backup_db(struct st_database * db) {
	return st_db_postgresql_init_backup(db->data);
}

struct st_database_connection * st_db_postgresql_connect(struct st_database * db, struct st_database_connection * connection) {
	if (!db)
		return 0;

	short allocated = 0;

	if (!connection) {
		allocated = 1;
		connection = malloc(sizeof(struct st_database_connection));
	}

	connection->driver = db;

	int failed = st_db_postgresql_init_connection(connection, db->data);

	if (failed && allocated) {
		free(connection);
		return 0;
	}

	return connection;
}

void st_db_postgresql_init() {
	st_db_register_db(&st_db_postgresql);
}

void st_db_postgresql_pr_free(struct st_db_postgresql_private * self) {
	if (!self)
		return;

	if (self->user)
		free(self->user);
	self->user = 0;
	if (self->password)
		free(self->password);
	self->password = 0;
	if (self->db)
		free(self->db);
	self->db = 0;
	if (self->host)
		free(self->host);
	self->host = 0;
	if (self->port)
		free(self->port);
	self->port = 0;
}

int st_db_postgresql_ping(struct st_database * db) {
	if (!db)
		return -1;

	struct st_db_postgresql_private * self = db->data;

	PGconn * con = PQsetdbLogin(self->host, self->port, 0, 0, self->db, self->user, self->password);
	ConnStatusType status = PQstatus(con);
	PQfinish(con);

	if (status == CONNECTION_OK)
		st_log_write_all(st_log_level_info, st_log_type_plugin_db, "Postgresql: ping => Ok");
	else
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: ping => Failed");

	return status == CONNECTION_OK ? 1 : -1;
}

int st_db_postgresql_setup(struct st_database * db, struct st_hashtable * params) {
	if (!db)
		return 1;

	struct st_db_postgresql_private * self = db->data;
	if (self)
		st_db_postgresql_pr_free(self);
	else
		db->data = self = malloc(sizeof(struct st_db_postgresql_private));

	char * user = st_hashtable_value(params, "user");
	if (user)
		self->user = strdup(user);
	else
		self->user = 0;

	char * password = st_hashtable_value(params, "password");
	if (password)
		self->password = strdup(password);
	else
		self->password = 0;

	char * database = st_hashtable_value(params, "db");
	if (database)
		self->db = strdup(database);
	else
		self->db = 0;

	char * host = st_hashtable_value(params, "host");
	if (host)
		self->host = strdup(host);
	else
		self->host = 0;

	char * port = st_hashtable_value(params, "port");
	if (port)
		self->port = strdup(port);
	else
		self->port = 0;

	return 0;
}

