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
*  Copyright (C) 2013-2015, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Fri, 29 Nov 2013 00:07:21 +0100                            *
\****************************************************************************/

// PQfinish, PQsetdbLogin, PQstatus
#include <postgresql/libpq-fe.h>
// malloc
#include <stdlib.h>
// bzero, strchr, strdup
#include <string.h>
// uname
#include <sys/utsname.h>

#include <libstone/log.h>
#include <libstone/util/hashtable.h>

#include "common.h"

static struct st_stream_reader * st_db_postgresql_backup_db(struct st_database_config * db_config);
static struct st_database_connection * st_db_postgresql_connect(struct st_database_config * db_config);
static void st_db_postgresql_config_free(struct st_database_config * db_config);
static int st_db_postgresql_ping(struct st_database_config * db_config);

static struct st_database_config_ops st_db_postgresql_config_ops = {
	.backup_db = st_db_postgresql_backup_db,
	.connect   = st_db_postgresql_connect,
	.free      = st_db_postgresql_config_free,
	.ping      = st_db_postgresql_ping,
};


static struct st_stream_reader * st_db_postgresql_backup_db(struct st_database_config * db_config) {
	return st_db_postgresql_backup_init(db_config->data);
}

static struct st_database_connection * st_db_postgresql_connect(struct st_database_config * db_config) {
	struct st_db_postgresql_config_private * self = db_config->data;

	PGconn * connect = PQsetdbLogin(self->host, self->port, NULL, NULL, self->db, self->user, self->password);
	ConnStatusType status = PQstatus(connect);

	if (status == CONNECTION_BAD) {
		PQfinish(connect);
		return NULL;
	}

	struct st_database_connection * connection = st_db_postgresql_connnect_init(connect);
	if (connection == NULL) {
		PQfinish(connect);
		return NULL;
	}

	connection->driver = db_config->driver;
	connection->config = db_config;
	return connection;
}

static void st_db_postgresql_config_free(struct st_database_config * config) {
	struct st_db_postgresql_config_private * self = config->data;
	free(self->user);
	free(self->password);
	free(self->db);
	free(self->host);
	free(self->port);
	free(self);

	free(config->name);
	config->name = NULL;
	config->ops = NULL;
}

int st_db_postgresql_config_init(struct st_database_config * config, const struct st_hashtable * params) {
	struct st_db_postgresql_config_private * self = malloc(sizeof(struct st_db_postgresql_config_private));
	bzero(self, sizeof(struct st_db_postgresql_config_private));

	const char * value = st_hashtable_get(params, "user").value.string;
	if (value != NULL)
		self->user = strdup(value);

	value = st_hashtable_get(params, "password").value.string;
	if (value != NULL)
		self->password = strdup(value);

	value = st_hashtable_get(params, "db").value.string;
	if (value != NULL)
		self->db = strdup(value);

	value = st_hashtable_get(params, "host").value.string;
	if (value != NULL)
		self->host = strdup(value);

	value = st_hashtable_get(params, "port").value.string;
	if (value != NULL)
		self->port = strdup(value);

	config->name = strdup(st_hashtable_get(params, "alias").value.string);
	config->ops = &st_db_postgresql_config_ops;
	config->data = self;

	return 0;
}

static int st_db_postgresql_ping(struct st_database_config * db_config) {
	struct st_db_postgresql_config_private * self = db_config->data;

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

	st_log_write_all(st_log_level_info, st_log_type_plugin_db, "Ping postgresql v.%d.%d.%d (using protocol version %d)", server_version_major, server_version_minor, server_version_rev, protocol_version);

	PQfinish(connect);

	return 1;
}

