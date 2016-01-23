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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// PQfinish, PQsetdbLogin, PQstatus
#include <libpq-fe.h>
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

#include <libstoriqone/log.h>
#include <libstoriqone/value.h>

#include "common.h"

static struct so_stream_reader * so_database_postgresql_backup_db(struct so_database_config * db_config);
static struct so_database_connection * so_database_postgresql_connect(struct so_database_config * db_config);
static int so_database_postgresql_ping(struct so_database_config * db_config);

static struct so_database_config_ops so_database_postgresql_config_ops = {
	.backup_db = so_database_postgresql_backup_db,
	.connect   = so_database_postgresql_connect,
	.ping      = so_database_postgresql_ping,
};


static struct so_stream_reader * so_database_postgresql_backup_db(struct so_database_config * db_config) {
	return so_database_postgresql_backup_init(db_config->data);
}

static struct so_database_connection * so_database_postgresql_connect(struct so_database_config * db_config) {
	struct so_database_postgresql_config_private * self = db_config->data;

	PGconn * connect = PQsetdbLogin(self->host, self->port, NULL, NULL, self->db, self->user, self->password);
	ConnStatusType status = PQstatus(connect);

	if (status == CONNECTION_BAD) {
		PQfinish(connect);
		return NULL;
	}

	struct so_database_connection * connection = so_database_postgresql_connect_init(connect);
	if (connection == NULL) {
		PQfinish(connect);
		return NULL;
	}

	connection->driver = db_config->driver;
	connection->config = db_config;
	return connection;
}

void so_database_postgresql_config_free(void * data) {
	struct so_database_config * config = data;
	free(config->name);

	struct so_database_postgresql_config_private * self = config->data;
	free(self->user);
	free(self->password);
	free(self->db);
	free(self->host);
	free(self->port);
	free(self);

	free(config);
}

struct so_database_config * so_database_postgresql_config_init(struct so_value * params) {
	struct so_database_postgresql_config_private * self = malloc(sizeof(struct so_database_postgresql_config_private));
	bzero(self, sizeof(struct so_database_postgresql_config_private));

	char * name = NULL;
	so_value_unpack(params, "{ss}", "name", &name);
	so_value_unpack(params, "{ss}", "user", &self->user);
	so_value_unpack(params, "{ss}", "password", &self->password);
	so_value_unpack(params, "{ss}", "db", &self->db);
	so_value_unpack(params, "{ss}", "host", &self->host);
	so_value_unpack(params, "{ss}", "port", &self->port);

	struct so_database_config * config = malloc(sizeof(struct so_database_config));
	config->ops = &so_database_postgresql_config_ops;
	config->data = self;
	config->driver = NULL;

	if (name == NULL) {
		static int n_config = 1;
		int size = asprintf(&config->name, "config_%d", n_config);
		if (size < 0)
			goto error_init;
		n_config++;
	}

	return config;

error_init:
	free(config);
	free(self->user);
	free(self->password);
	free(self->db);
	free(self->host);
	free(self->port);
	free(self);

	return NULL;
}

static int so_database_postgresql_ping(struct so_database_config * db_config) {
	struct so_database_postgresql_config_private * self = db_config->data;

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

	so_log_write2(so_log_level_info, so_log_type_plugin_db, "Ping postgresql v.%d.%d.%d (using protocol version %d)", server_version_major, server_version_minor, server_version_rev, protocol_version);

	PQfinish(connect);

	return 1;
}

