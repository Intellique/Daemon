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
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strchr, strdup
#include <string.h>
// bzero
#include <strings.h>

#include <libstone/value.h>

#include "common.h"

static struct st_database_connection * st_database_mariadb_connect(struct st_database_config * db_config);
static int st_database_mariadb_ping(struct st_database_config * db_config);

static struct st_database_config_ops st_database_mariadb_config_ops = {
	.connect = st_database_mariadb_connect,
	.ping    = st_database_mariadb_ping,
};


static struct st_database_connection * st_database_mariadb_connect(struct st_database_config * db_config) {
	struct st_database_mariadb_config_private * self = db_config->data;

	struct st_database_mariadb_connection_private * connect = malloc(sizeof(struct st_database_mariadb_connection_private)); 
	bzero(connect, sizeof(struct st_database_mariadb_connection_private));

	struct st_database_connection * connection = st_database_mariadb_connnect_init(self, connect);
	if (connection == NULL) {
		free(connect);
		return NULL;
	}

	connection->driver = db_config->driver;
	connection->config = db_config;
	return connection;
}

void st_database_mariadb_config_free(void * data) {
	struct st_database_config * config = data;
	free(config->name);

	struct st_database_mariadb_config_private * self = config->data;
	free(self->user);
	free(self->password);
	free(self->db);
	free(self->host);
	free(self);

	free(config);
}

struct st_database_config * st_database_mariadb_config_init(struct st_value * params) {
	struct st_value * name = st_value_hashtable_get2(params, "name", false, false);
	struct st_value * user = st_value_hashtable_get2(params, "user", false, false);
	struct st_value * password = st_value_hashtable_get2(params, "password", false, false);
	struct st_value * db = st_value_hashtable_get2(params, "db", false, false);
	struct st_value * host = st_value_hashtable_get2(params, "host", false, false);
	struct st_value * port = st_value_hashtable_get2(params, "port", false, false);

	struct st_database_mariadb_config_private * self = malloc(sizeof(struct st_database_mariadb_config_private));
	bzero(self, sizeof(struct st_database_mariadb_config_private));

	if (user != NULL && user->type == st_value_string)
		self->user = strdup(st_value_string_get(user));

	if (password != NULL && password->type == st_value_string)
		self->password = strdup(st_value_string_get(password));

	if (db != NULL && db->type == st_value_string)
		self->db = strdup(st_value_string_get(db));

	if (host != NULL && host->type == st_value_string)
		self->host = strdup(st_value_string_get(host));

	if (port != NULL && port->type == st_value_integer)
		self->port = st_value_integer_get(port);

	struct st_database_config * config = malloc(sizeof(struct st_database_config));
	if (name != NULL && name->type == st_value_string)
		config->name = strdup(st_value_string_get(name));
	else {
		static int n_config = 1;
		asprintf(&config->name, "config_%d", n_config);
		n_config++;
	}
	config->ops = &st_database_mariadb_config_ops;
	config->data = self;
	config->driver = NULL;

	return config;
}

static int st_database_mariadb_ping(struct st_database_config * db_config) {
	struct st_database_mariadb_config_private * self = db_config->data;

	return 1;
}

