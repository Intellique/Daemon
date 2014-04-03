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
// asprintf
#include <stdio.h>
// malloc
#include <stdlib.h>

#include "common.h"

static int st_database_mariadb_close(struct st_database_connection * connect);
static int st_database_mariadb_free(struct st_database_connection * connect);
static bool st_database_mariadb_is_connected(struct st_database_connection * connect);

static int st_database_mariadb_cancel_checkpoint(struct st_database_connection * connect, const char * checkpoint);
static int st_database_mariadb_cancel_transaction(struct st_database_connection * connect);
static int st_database_mariadb_create_checkpoint(struct st_database_connection * connect, const char * checkpoint);
static int st_database_mariadb_finish_transaction(struct st_database_connection * connect);
static int st_database_mariadb_start_transaction(struct st_database_connection * connect);

static struct st_database_connection_ops st_database_mariadb_connection_ops = {
	.close        = st_database_mariadb_close,
	.free         = st_database_mariadb_free,
	.is_connected = st_database_mariadb_is_connected,

	.cancel_transaction = st_database_mariadb_cancel_transaction,
	.finish_transaction = st_database_mariadb_finish_transaction,
	.start_transaction  = st_database_mariadb_start_transaction,
};


struct st_database_connection * st_database_mariadb_connnect_init(struct st_database_mariadb_config_private * config, struct st_database_mariadb_connection_private * self) {
	mysql_init(&self->handler);

	void * ok = mysql_real_connect(&self->handler, config->host, config->user, config->password, config->db, config->port, NULL, 0);
	if (ok == NULL)
		return NULL;

	mysql_autocommit(&self->handler, 0);

	self->closed = false;

	struct st_database_connection * connect = malloc(sizeof(struct st_database_connection));
	connect->data = self;
	connect->ops = &st_database_mariadb_connection_ops;

	return connect;
}


static int st_database_mariadb_close(struct st_database_connection * connect) {
	struct st_database_mariadb_connection_private * self = connect->data;

	if (!self->closed) {
		mysql_close(&self->handler);
		self->closed = true;
	}

	return 0;
}

static int st_database_mariadb_free(struct st_database_connection * connect) {
	st_database_mariadb_close(connect);
	free(connect->data);

	connect->data = NULL;
	connect->driver = NULL;
	connect->config = NULL;
	free(connect);

	return 0;
}

static bool st_database_mariadb_is_connected(struct st_database_connection * connect) {
	struct st_database_mariadb_connection_private * self = connect->data;
	return !self->closed;
}


static int st_database_mariadb_cancel_checkpoint(struct st_database_connection * connect, const char * checkpoint) {
	if (connect == NULL || checkpoint == NULL)
		return -1;

	struct st_database_mariadb_connection_private * self = connect->data;

	char * query;
	asprintf(&query, "ROLLBACK TO \"%s\"", checkpoint);

	int failed = mysql_query(&self->handler, query);

	free(query);

	return failed;
}

static int st_database_mariadb_cancel_transaction(struct st_database_connection * connect) {
	if (connect == NULL)
		return -1;

	struct st_database_mariadb_connection_private * self = connect->data;
	return mysql_query(&self->handler, "ROLLBACK");
}

static int st_database_mariadb_create_checkpoint(struct st_database_connection * connect, const char * checkpoint) {
	if (connect == NULL || checkpoint == NULL)
		return -1;

	struct st_database_mariadb_connection_private * self = connect->data;

	char * query;
	asprintf(&query, "SAVEPOINT \"%s\"", checkpoint);

	int failed = mysql_query(&self->handler, query);

	free(query);

	return failed;
}

static int st_database_mariadb_finish_transaction(struct st_database_connection * connect) {
	if (connect == NULL)
		return -1;

	struct st_database_mariadb_connection_private * self = connect->data;
	return mysql_commit(&self->handler);
}

static int st_database_mariadb_start_transaction(struct st_database_connection * connect) {
	if (connect == NULL)
		return -1;

	struct st_database_mariadb_connection_private * self = connect->data;
	return mysql_query(&self->handler, "BEGIN");
}

