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
*  Copyright (C) 2013-2018, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// asprintf
#include <stdio.h>
// malloc
#include <stdlib.h>
// strlen
#include <string.h>
// bzero
#include <strings.h>
// uname
#include <sys/utsname.h>

#include "common.h"

static int st_database_mariadb_close(struct st_database_connection * connect);
static int st_database_mariadb_free(struct st_database_connection * connect);
static bool st_database_mariadb_is_connected(struct st_database_connection * connect);

static int st_database_mariadb_cancel_checkpoint(struct st_database_connection * connect, const char * checkpoint);
static int st_database_mariadb_cancel_transaction(struct st_database_connection * connect);
static int st_database_mariadb_create_checkpoint(struct st_database_connection * connect, const char * checkpoint);
static int st_database_mariadb_finish_transaction(struct st_database_connection * connect);
static int st_database_mariadb_start_transaction(struct st_database_connection * connect);

static int st_database_mariadb_add_host(struct st_database_connection * connect, const char * uuid, const char * name, const char * domaine, const char * description);
static bool st_database_mariadb_find_host(struct st_database_connection * connect, const char * uuid, const char * hostname);
static unsigned int st_database_mariadb_get_host(struct st_database_connection * connect);
static struct st_value * st_database_mariadb_get_host_by_name(struct st_database_connection * connect, const char * name);

static int st_database_mariadb_sync_changer(struct st_database_connection * connect, struct st_value * changer, bool init);
static int st_database_mariadb_sync_drive(struct st_database_connection * connect, struct st_value * drive, bool init);
static int st_database_mariadb_sync_slots(struct st_database_connection * connect, struct st_value * slot, bool init);

static void st_database_mariadb_free_prepared(void * data);
static MYSQL_STMT * st_database_mariadb_prepare(struct st_database_mariadb_connection_private * self, const char * statement_name, const char * query);

static struct st_database_connection_ops st_database_mariadb_connection_ops = {
	.close        = st_database_mariadb_close,
	.free         = st_database_mariadb_free,
	.is_connected = st_database_mariadb_is_connected,

	.cancel_transaction = st_database_mariadb_cancel_transaction,
	.finish_transaction = st_database_mariadb_finish_transaction,
	.start_transaction  = st_database_mariadb_start_transaction,

	.add_host         = st_database_mariadb_add_host,
	.find_host        = st_database_mariadb_find_host,
	.get_host_by_name = st_database_mariadb_get_host_by_name,

	.sync_changer = st_database_mariadb_sync_changer,
	.sync_drive   = st_database_mariadb_sync_drive,
};


struct st_database_connection * st_database_mariadb_connnect_init(struct st_database_mariadb_config_private * config, struct st_database_mariadb_connection_private * self) {
	mysql_init(&self->handler);

	void * ok = mysql_real_connect(&self->handler, config->host, config->user, config->password, config->db, config->port, NULL, 0);
	if (ok == NULL)
		return NULL;

	mysql_autocommit(&self->handler, 0);

	self->closed = false;
	self->cached_query = st_value_new_hashtable2();

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
		st_value_free(self->cached_query);
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
	return mysql_rollback(&self->handler);
}


static int st_database_mariadb_add_host(struct st_database_connection * connect, const char * uuid, const char * name, const char * domaine, const char * description) {
	if (connect == NULL || uuid == NULL || name == NULL)
		return 1;

	struct st_database_mariadb_connection_private * self = connect->data;

	const char * query = "insert_host";
	MYSQL_STMT * stmt = st_database_mariadb_prepare(self, query, "INSERT INTO Host(uuid, name, domaine, description) VALUES (?, ?, ?, ?)");

	MYSQL_BIND params[4];
	bzero(params, sizeof(params));

	st_database_mariadb_util_prepare_string(params, uuid);
	st_database_mariadb_util_prepare_string(params + 1, name);
	st_database_mariadb_util_prepare_string(params + 2, domaine);
	st_database_mariadb_util_prepare_string(params + 3, description);

	mysql_stmt_bind_param(stmt, params);
	int failed = mysql_stmt_execute(stmt);

	mysql_stmt_reset(stmt);

	return failed;
}

static bool st_database_mariadb_find_host(struct st_database_connection * connect, const char * uuid, const char * hostname) {
	struct st_database_mariadb_connection_private * self = connect->data;

	const char * query = "select_host_by_name_or_uuid";
	MYSQL_STMT * stmt = st_database_mariadb_prepare(self, query, "SELECT id FROM Host WHERE uuid = ? OR name = ? OR CONCAT(name, '.', domaine) = ? LIMIT 1");

	MYSQL_BIND params[3];
	bzero(params, sizeof(params));

	st_database_mariadb_util_prepare_string(params, uuid);
	st_database_mariadb_util_prepare_string(params + 1, hostname);
	st_database_mariadb_util_prepare_string(params + 2, hostname);

	mysql_stmt_bind_param(stmt, params);
	int failed = mysql_stmt_execute(stmt);

	bool found = false;
	if (failed == 0 && mysql_stmt_num_rows(stmt) > 0)
		found = true;

	mysql_stmt_reset(stmt);

	return found;
}

static unsigned int st_database_mariadb_get_host(struct st_database_connection * connect) {
	struct st_database_mariadb_connection_private * self = connect->data;

	const char * query = "select_host_by_name";
	MYSQL_STMT * stmt = st_database_mariadb_prepare(self, query, "SELECT id FROM Host WHERE name = ? OR CONCAT(name, '.', domaine) = ? LIMIT 1");

	struct utsname name;
	uname(&name);

	MYSQL_BIND params[2];
	bzero(params, sizeof(params));

	st_database_mariadb_util_prepare_string(params, name.nodename);
	st_database_mariadb_util_prepare_string(params + 1, name.nodename);

	mysql_stmt_bind_param(stmt, params);
	int failed = mysql_stmt_execute(stmt);

	unsigned int hostid = 0;
	if (failed == 0 && mysql_stmt_num_rows(stmt) > 0) {
		MYSQL_BIND row;
		bzero(&row, sizeof(row));

		row.buffer_type = MYSQL_TYPE_LONG;
		row.buffer = (char *) &hostid;
		row.buffer_length = sizeof(hostid);

		mysql_stmt_bind_result(stmt, &row);

		mysql_stmt_fetch(stmt);
	}

	mysql_stmt_reset(stmt);

	return hostid;
}

static struct st_value * st_database_mariadb_get_host_by_name(struct st_database_connection * connect, const char * name) {
	struct st_database_mariadb_connection_private * self = connect->data;

	const char * query = "select_host_by_name_by_name";
	MYSQL_STMT * stmt = st_database_mariadb_prepare(self, query, "SELECT CASE WHEN domain IS NULL THEN name ELSE CONCAT(name, '.', domaine) END, uuid FROM Host WHERE name = ? OR CONCAT(name, '.', domaine) = ? LIMIT 1");

	MYSQL_BIND params[2];
	bzero(params, sizeof(params));

	st_database_mariadb_util_prepare_string(params, name);
	st_database_mariadb_util_prepare_string(params + 1, name);

	mysql_stmt_bind_param(stmt, params);
	int failed = mysql_stmt_execute(stmt);

	struct st_value * vresult = NULL;

	if (failed != 0) {
		vresult = st_value_pack("{sbss}", "error", true, "message", mysql_stmt_error(stmt));
	} else if (failed == 0 && mysql_stmt_num_rows(stmt) == 1) {
		char host[32], uuid[37];

		MYSQL_BIND result[2];
		bzero(result, sizeof(result));

		result[0].buffer_type = MYSQL_TYPE_STRING;
		result[0].buffer = host;
		result[0].buffer_length = 32;

		result[1].buffer_type = MYSQL_TYPE_STRING;
		result[1].buffer = uuid;
		result[1].buffer_length = 37;

		mysql_stmt_bind_result(stmt, result);

		mysql_stmt_fetch(stmt);

		vresult = st_value_pack("{sss{ssss}}", "error", false, "host", "hostname", host, "uuid", uuid);
	}

	mysql_stmt_reset(stmt);

	return vresult;
}


static int st_database_mariadb_sync_changer(struct st_database_connection * connect, struct st_value * changer, bool init) {
	if (connect == NULL || changer == NULL)
		return -1;

	struct st_database_mariadb_connection_private * self = connect->data;
}

static int st_database_mariadb_sync_drive(struct st_database_connection * connect, struct st_value * drive, bool init) {
}

static int st_database_mariadb_sync_slots(struct st_database_connection * connect, struct st_value * slot, bool init) {
}


static void st_database_mariadb_free_prepared(void * data) {
	mysql_stmt_close(data);
}

static MYSQL_STMT * st_database_mariadb_prepare(struct st_database_mariadb_connection_private * self, const char * statement_name, const char * query) {
	if (st_value_hashtable_has_key2(self->cached_query, statement_name)) {
		struct st_value * vstmt = st_value_hashtable_get2(self->cached_query, statement_name, false, false);
		return st_value_custom_get(vstmt);
	} else {
		MYSQL_STMT * stmt = mysql_stmt_init(&self->handler);
		mysql_stmt_prepare(stmt, query, strlen(query));

		st_value_hashtable_put2(self->cached_query, statement_name, st_value_new_custom(stmt, st_database_mariadb_free_prepared), true);

		return stmt;
	}
}
