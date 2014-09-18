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

#define _GNU_SOURCE
// PQfinish, PQsetdbLogin, PQstatus
#include <postgresql/libpq-fe.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strchr, strdup
#include <string.h>
// bzero
#include <strings.h>
// uname
#include <sys/utsname.h>

#include <libstone/log.h>
#include <libstone/value.h>

#include "common.h"

static struct st_stream_reader * st_database_postgresql_backup_db(struct st_database_config * db_config);
static struct st_database_connection * st_database_postgresql_connect(struct st_database_config * db_config);
static int st_database_postgresql_ping(struct st_database_config * db_config);

static struct st_database_config_ops st_database_postgresql_config_ops = {
	.backup_db = st_database_postgresql_backup_db,
	.connect   = st_database_postgresql_connect,
	.ping      = st_database_postgresql_ping,
};


static struct st_stream_reader * st_database_postgresql_backup_db(struct st_database_config * db_config) {
	return st_database_postgresql_backup_init(db_config->data);
}

static struct st_database_connection * st_database_postgresql_connect(struct st_database_config * db_config) {
	struct st_database_postgresql_config_private * self = db_config->data;

	PGconn * connect = PQsetdbLogin(self->host, self->port, NULL, NULL, self->db, self->user, self->password);
	ConnStatusType status = PQstatus(connect);

	if (status == CONNECTION_BAD) {
		PQfinish(connect);
		return NULL;
	}

	struct st_database_connection * connection = st_database_postgresql_connect_init(connect);
	if (connection == NULL) {
		PQfinish(connect);
		return NULL;
	}

	connection->driver = db_config->driver;
	connection->config = db_config;
	return connection;
}

void st_database_postgresql_config_free(void * data) {
	struct st_database_config * config = data;
	free(config->name);

	struct st_database_postgresql_config_private * self = config->data;
	free(self->user);
	free(self->password);
	free(self->db);
	free(self->host);
	free(self->port);
	free(self);

	free(config);
}

struct st_database_config * st_database_postgresql_config_init(struct st_value * params) {
	struct st_value * name = st_value_hashtable_get2(params, "name", false, false);
	struct st_value * user = st_value_hashtable_get2(params, "user", false, false);
	struct st_value * password = st_value_hashtable_get2(params, "password", false, false);
	struct st_value * db = st_value_hashtable_get2(params, "db", false, false);
	struct st_value * host = st_value_hashtable_get2(params, "host", false, false);
	struct st_value * port = st_value_hashtable_get2(params, "port", false, false);

	struct st_database_postgresql_config_private * self = malloc(sizeof(struct st_database_postgresql_config_private));
	bzero(self, sizeof(struct st_database_postgresql_config_private));

	if (user != NULL && user->type == st_value_string)
		self->user = strdup(st_value_string_get(user));

	if (password != NULL && password->type == st_value_string)
		self->password = strdup(st_value_string_get(password));

	if (db != NULL && db->type == st_value_string)
		self->db = strdup(st_value_string_get(db));

	if (host != NULL && host->type == st_value_string)
		self->host = strdup(st_value_string_get(host));

	if (port != NULL) {
		if (port->type == st_value_string)
			self->port = strdup(st_value_string_get(port));
		else if (port->type == st_value_integer)
			asprintf(&self->port, "%lld", st_value_integer_get(port));
	}

	struct st_database_config * config = malloc(sizeof(struct st_database_config));
	if (name != NULL && name->type == st_value_string)
		config->name = strdup(st_value_string_get(name));
	else {
		static int n_config = 1;
		asprintf(&config->name, "config_%d", n_config);
		n_config++;
	}
	config->ops = &st_database_postgresql_config_ops;
	config->data = self;
	config->driver = NULL;

	return config;
}

static int st_database_postgresql_ping(struct st_database_config * db_config) {
	struct st_database_postgresql_config_private * self = db_config->data;

	PGconn * connect = PQsetdbLogin(self->host, self->port, NULL, NULL, self->db, self->user, self->password);
	ConnStatusType status = PQstatus(connect);

	if (status == CONNECTION_BAD) {
		PQfinish(connect);
		return -1;
	}

	int protocol_version = PQprotocolVersion(connect);
	int server_version_major = PQserverVersion(connect);

	int server_version_rev = server_version_major % 100;
	server_version_major /= 100;
	int server_version_minor = server_version_major % 100;
	server_version_major /= 100;

	st_log_write2(st_log_level_info, st_log_type_plugin_db, "Ping postgresql v.%d.%d.%d (using protocol version %d)", server_version_major, server_version_minor, server_version_rev, protocol_version);

	PQfinish(connect);

	return 1;
}

