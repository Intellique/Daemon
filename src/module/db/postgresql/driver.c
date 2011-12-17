/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sat, 17 Dec 2011 17:23:07 +0100                         *
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

static struct sa_database_connection * sa_db_postgresql_connect(struct sa_database * db, struct sa_database_connection * connection);
static void sa_db_postgresql_init(void) __attribute__((constructor));
static int sa_db_postgresql_ping(struct sa_database * db);
static int sa_db_postgresql_setup(struct sa_database * db, struct sa_hashtable * params);


static struct sa_database_ops sa_db_postgresql_ops = {
	.connect = sa_db_postgresql_connect,
	.ping    = sa_db_postgresql_ping,
	.setup   = sa_db_postgresql_setup,
};

static struct sa_database sa_db_postgresql = {
	.name        = "postgresql",
	.ops         = &sa_db_postgresql_ops,
	.data        = 0,
	.cookie      = 0,
	.api_version = STORIQARCHIVER_DATABASE_APIVERSION,
};


struct sa_database_connection * sa_db_postgresql_connect(struct sa_database * db, struct sa_database_connection * connection) {
	if (!db)
		return 0;

	short allocated = 0;

	if (!connection) {
		allocated = 1;
		connection = malloc(sizeof(struct sa_database_connection));
	}

	connection->driver = db;

	int failed = sa_db_postgresql_init_connection(connection, db->data);

	if (failed && allocated) {
		free(connection);
		return 0;
	}

	return connection;
}

void sa_db_postgresql_init() {
	sa_db_register_db(&sa_db_postgresql);
}

void sa_db_postgresql_pr_free(struct sa_db_postgresql_private * self) {
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

int sa_db_postgresql_ping(struct sa_database * db) {
	if (!db)
		return -1;

	struct sa_db_postgresql_private * self = db->data;

	PGconn * con = PQsetdbLogin(self->host, self->port, 0, 0, self->db, self->user, self->password);
	ConnStatusType status = PQstatus(con);
	PQfinish(con);

	if (status == CONNECTION_OK)
		sa_log_write_all(sa_log_level_info, "db: Postgresql: ping => Ok");
	else
		sa_log_write_all(sa_log_level_error, "db: Postgresql: ping => Failed");

	return status == CONNECTION_OK ? 1 : -1;
}

int sa_db_postgresql_setup(struct sa_database * db, struct sa_hashtable * params) {
	if (!db)
		return 1;

	struct sa_db_postgresql_private * self = db->data;
	if (self)
		sa_db_postgresql_pr_free(self);
	else
		db->data = self = malloc(sizeof(struct sa_db_postgresql_private));

	char * user = sa_hashtable_value(params, "user");
	if (user)
		self->user = strdup(user);
	else
		self->user = 0;

	char * password = sa_hashtable_value(params, "password");
	if (password)
		self->password = strdup(password);
	else
		self->password = 0;

	char * database = sa_hashtable_value(params, "db");
	if (database)
		self->db = strdup(database);
	else
		self->db = 0;

	char * host = sa_hashtable_value(params, "host");
	if (host)
		self->host = strdup(host);
	else
		self->host = 0;

	char * port = sa_hashtable_value(params, "port");
	if (port)
		self->port = strdup(port);
	else
		self->port = 0;

	return 0;
}

