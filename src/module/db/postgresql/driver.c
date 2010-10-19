/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2010, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Fri, 15 Oct 2010 17:24:42 +0200                       *
\***********************************************************************/

// free, malloc
#include <malloc.h>
// strdup
#include <string.h>
// PQfinish, PQsetdbLogin, PQstatus
#include <postgresql/libpq-fe.h>

#include <storiqArchiver/log.h>
#include <storiqArchiver/util/hashtable.h>

#include "common.h"

static struct database_connection * db_postgresql_connect(struct database * db, struct database_connection * connection);
static int db_postgresql_ping(struct database * db);
static int db_postgresql_setup(struct database * db, struct hashtable * params);


static struct database_ops db_postgresql_ops = {
	.connect =	db_postgresql_connect,
	.ping =		db_postgresql_ping,
	.setup =	db_postgresql_setup,
};

static struct database db_postgresql = {
	.driverName =	"postgresql",
	.ops =			&db_postgresql_ops,
	.data =			0,
	.cookie =		0,
};


struct database_connection * db_postgresql_connect(struct database * db, struct database_connection * connection) {
	if (!db)
		return 0;

	short allocated = 0;

	if (!connection) {
		allocated = 1;
		connection = malloc(sizeof(struct database_connection));
	}

	connection->driver = db;

	int failed = db_postgresql_initConnection(connection, db->data);

	if (failed && allocated) {
		free(connection);
		return 0;
	}

	return connection;
}

__attribute__((constructor))
static void db_postgresql_init() {
	db_registerDb(&db_postgresql);
}

void db_postgresql_prFree(struct db_postgresql_private * self) {
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

int db_postgresql_ping(struct database * db) {
	if (!db)
		return -1;

	struct db_postgresql_private * self = db->data;

	PGconn * con = PQsetdbLogin(self->host, self->port, 0, 0, self->db, self->user, self->password);
	ConnStatusType status = PQstatus(con);
	PQfinish(con);

	if (status == CONNECTION_OK)
		log_writeAll(Log_level_info, "db: Postgresql: ping => Ok");
	else
		log_writeAll(Log_level_error, "db: Postgresql: ping => Failed");

	return status == CONNECTION_OK ? 1 : -1;
}

int db_postgresql_setup(struct database * db, struct hashtable * params) {
	if (!db)
		return 1;

	struct db_postgresql_private * self = db->data;
	if (self)
		db_postgresql_prFree(self);
	else
		db->data = self = malloc(sizeof(struct db_postgresql_private));

	char * user = hashtable_value(params, "user");
	if (user)
		self->user = strdup(user);
	else
		self->user = 0;

	char * password = hashtable_value(params, "password");
	if (password)
		self->password = strdup(password);
	else
		self->password = 0;

	char * database = hashtable_value(params, "db");
	if (database)
		self->db = strdup(database);
	else
		self->db = 0;

	char * host = hashtable_value(params, "host");
	if (host)
		self->host = strdup(host);
	else
		self->host = 0;

	char * port = hashtable_value(params, "port");
	if (port)
		self->port = strdup(port);
	else
		self->port = 0;

	return 0;
}

