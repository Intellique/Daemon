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
// PQclear, PQexec, PQexecPrepared, PQfinish, PQresultStatus
// PQsetErrorVerbosity, PQtransactionStatus
#include <postgresql/libpq-fe.h>
// asprintf
#include <stdio.h>
// malloc
#include <stdlib.h>
// uname
#include <sys/utsname.h>

#include <libstone/log.h>
#include <libstone/value.h>

#include "common.h"

struct st_database_postgresql_connection_private {
	PGconn * connect;
	struct st_value * cached_query;
};

static int st_database_postgresql_close(struct st_database_connection * connect);
static int st_database_postgresql_free(struct st_database_connection * connect);
static bool st_database_postgresql_is_connected(struct st_database_connection * connect);

static int st_database_postgresql_cancel_checkpoint(struct st_database_connection * connect, const char * checkpoint);
static int st_database_postgresql_cancel_transaction(struct st_database_connection * connect);
static int st_database_postgresql_create_checkpoint(struct st_database_connection * connect, const char * checkpoint);
static int st_database_postgresql_finish_transaction(struct st_database_connection * connect);
static int st_database_postgresql_start_transaction(struct st_database_connection * connect);

static int st_database_postgresql_add_host(struct st_database_connection * connect, const char * uuid, const char * name, const char * domaine, const char * description);
static bool st_database_postgresql_find_host(struct st_database_connection * connect, const char * uuid, const char * hostname);
static char * st_database_postgresql_get_host(struct st_database_connection * connect);
static struct st_value * st_database_postgresql_get_host_by_name(struct st_database_connection * connect, const char * name);

static void st_database_postgresql_prepare(struct st_database_postgresql_connection_private * self, const char * statement_name, const char * query);

static struct st_database_connection_ops st_database_postgresql_connection_ops = {
	.close        = st_database_postgresql_close,
	.free         = st_database_postgresql_free,
	.is_connected = st_database_postgresql_is_connected,

	.cancel_transaction = st_database_postgresql_cancel_transaction,
	.finish_transaction = st_database_postgresql_finish_transaction,
	.start_transaction  = st_database_postgresql_start_transaction,

	.add_host         = st_database_postgresql_add_host,
	.find_host        = st_database_postgresql_find_host,
	.get_host_by_name = st_database_postgresql_get_host_by_name,
};


struct st_database_connection * st_database_postgresql_connect_init(PGconn * pg_connect) {
	PQsetErrorVerbosity(pg_connect, PQERRORS_VERBOSE);

	struct st_database_postgresql_connection_private * self = malloc(sizeof(struct st_database_postgresql_connection_private));
	self->connect = pg_connect;
	self->cached_query = st_value_new_hashtable2();

	struct st_database_connection * connect = malloc(sizeof(struct st_database_connection));
	connect->data = self;
	connect->ops = &st_database_postgresql_connection_ops;

	return connect;
}


static int st_database_postgresql_close(struct st_database_connection * connect) {
	struct st_database_postgresql_connection_private * self = connect->data;

	if (self->connect != NULL)
		PQfinish(self->connect);
	self->connect = NULL;

	if (self->cached_query != NULL)
		st_value_free(self->cached_query);
	self->cached_query = NULL;

	return 0;
}

static int st_database_postgresql_free(struct st_database_connection * connect) {
	st_database_postgresql_close(connect);
	free(connect->data);

	connect->data = NULL;
	connect->driver = NULL;
	connect->config = NULL;
	free(connect);

	return 0;
}

static bool st_database_postgresql_is_connected(struct st_database_connection * connect) {
	struct st_database_postgresql_connection_private * self = connect->data;

	if (self->connect == NULL)
		return false;

	return PQstatus(self->connect) != CONNECTION_OK;
}


static int st_database_postgresql_cancel_checkpoint(struct st_database_connection * connect, const char * checkpoint) {
	if (connect == NULL || checkpoint == NULL)
		return -1;

	struct st_database_postgresql_connection_private * self = connect->data;
	char * query = NULL;
	asprintf(&query, "ROLLBACK TO %s", checkpoint);

	PGresult * result = PQexec(self->connect, query);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, NULL);

	if (status != PGRES_COMMAND_OK)
		st_log_write2(st_log_level_error, st_log_type_plugin_db, "Postgresql: error while rollbacking a savepoint => %s", PQerrorMessage(self->connect));

	PQclear(result);
	free(query);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

static int st_database_postgresql_cancel_transaction(struct st_database_connection * connect) {
	if (!connect)
		return -1;

	struct st_database_postgresql_connection_private * self = connect->data;

	PGTransactionStatusType status = PQtransactionStatus(self->connect);
	switch (status) {
		case PQTRANS_INERROR:
		case PQTRANS_INTRANS: {
			PGresult * result = PQexec(self->connect, "ROLL BACK");
			ExecStatusType roll_back_status = PQresultStatus(result);
			if (roll_back_status != PGRES_COMMAND_OK)
				st_database_postgresql_get_error(result, NULL);
			PQclear(result);
			return roll_back_status != PGRES_COMMAND_OK;
		}
		break;

		default:
			return 0;
	}
}

static int st_db_postgresql_create_checkpoint(struct st_database_connection * connect, const char * checkpoint) {
	if (connect == NULL || checkpoint == NULL)
		return -1;

	struct st_database_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType transStatus = PQtransactionStatus(self->connect);

	if (transStatus == PQTRANS_INERROR) {
		st_log_write2(st_log_level_error, st_log_type_plugin_db, "Postgresql: Can't create checkpoint because there is an error into current transaction");
		return -1;
	} else if (transStatus == PQTRANS_IDLE) {
		st_log_write2(st_log_level_error, st_log_type_plugin_db, "Postgresql: Can't create checkpoint because there is no active transaction");
		return -1;
	}

	char * query = NULL;
	asprintf(&query, "SAVEPOINT %s", checkpoint);

	PGresult * result = PQexec(self->connect, query);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, NULL);

	if (status != PGRES_COMMAND_OK)
		st_log_write2(st_log_level_error, st_log_type_plugin_db, "Postgresql: error while creating a savepoint => %s", PQerrorMessage(self->connect));

	PQclear(result);
	free(query);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

static int st_database_postgresql_finish_transaction(struct st_database_connection * connect) {
	if (connect == NULL)
		return -1;

	struct st_database_postgresql_connection_private * self = connect->data;

	PGTransactionStatusType status = PQtransactionStatus(self->connect);
	switch (status) {
		case PQTRANS_INERROR: {
			st_log_write2(st_log_level_error, st_log_type_plugin_db, "Postgresql: Rolling back transaction because current transaction is in error");

			PGresult * result = PQexec(self->connect, "ROLL BACK");
			PQclear(result);
			return 1;
		}
		break;

		case PQTRANS_INTRANS: {
			PGresult * result = PQexec(self->connect, "COMMIT");
			ExecStatusType commit_status = PQresultStatus(result);
			if (commit_status != PGRES_COMMAND_OK)
				st_database_postgresql_get_error(result, NULL);
			PQclear(result);
			return commit_status != PGRES_COMMAND_OK;
		}
		break;

		default:
			return 0;
	}
}

static int st_database_postgresql_start_transaction(struct st_database_connection * connect) {
	if (connect == NULL)
		return -1;

	struct st_database_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType status = PQtransactionStatus(self->connect);
	switch (status) {
		case PQTRANS_IDLE: {
			PGresult * result = PQexec(self->connect, "BEGIN");
			ExecStatusType commit_status = PQresultStatus(result);
			if (commit_status != PGRES_COMMAND_OK)
				st_database_postgresql_get_error(result, NULL);
			PQclear(result);
			return commit_status != PGRES_COMMAND_OK;
		}
		break;

		default:
			return 1;
	}
}


static int st_database_postgresql_add_host(struct st_database_connection * connect, const char * uuid, const char * name, const char * domaine, const char * description) {
	if (connect == NULL || uuid == NULL || name == NULL)
		return 1;

	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "insert_host";
	st_database_postgresql_prepare(self, query, "INSERT INTO host(uuid, name, domaine, description) VALUES ($1, $2, $3, $4)");

	const char * param[] = { uuid, name, domaine, description };
	PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);

	PQclear(result);

	return status == PGRES_FATAL_ERROR;
}

static bool st_database_postgresql_find_host(struct st_database_connection * connect, const char * uuid, const char * hostname) {
	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_host_by_name_or_uuid";
	st_database_postgresql_prepare(self, query, "SELECT id FROM host WHERE uuid::TEXT = $1 OR name = $2 OR name || '.' || domaine = $2 LIMIT 1");

	const char * param[] = { uuid, hostname };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	bool found = false;
	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (PQntuples(result) == 1)
		found = true;

	PQclear(result);

	return found;
}

static char * st_database_postgresql_get_host(struct st_database_connection * connect) {
	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_host_by_name";
	st_database_postgresql_prepare(self, query, "SELECT id FROM host WHERE name = $1 OR name || '.' || domaine = $1 LIMIT 1");

	struct utsname name;
	uname(&name);

	const char * param[] = { name.nodename };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);

	char * hostid = NULL;

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_database_postgresql_get_string_dup(result, 0, 0, &hostid);
	else
		st_log_write2(st_log_level_error, st_log_type_plugin_db, "Postgresql: Host not found into database (%s)", name.nodename);

	PQclear(result);

	return hostid;
}

static struct st_value * st_database_postgresql_get_host_by_name(struct st_database_connection * connect, const char * name) {
	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_host_by_name_by_name";
	st_database_postgresql_prepare(self, query, "SELECT CASE WHEN domaine IS NULL THEN name ELSE name || '.' || domaine END, uuid FROM host WHERE name = $1 OR name || '.' || domaine = $1 LIMIT 1");

	const char * param[] = { name };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	struct st_value * vresult;
	if (status == PGRES_FATAL_ERROR) {
		st_database_postgresql_get_error(result, query);

		char * error = PQresultErrorField(result, PG_DIAG_MESSAGE_PRIMARY);
		vresult = st_value_pack("{sbss}", "error", true, "message", error);
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		char * host = PQgetvalue(result, 0, 0);
		char * uuid = PQgetvalue(result, 0, 1);

		vresult = st_value_pack("{sss{ssss}}", "error", false, "host", "hostname", host, "uuid", uuid);
	}

	PQclear(result);

	return vresult;
}


static void st_database_postgresql_prepare(struct st_database_postgresql_connection_private * self, const char * statement_name, const char * query) {
	if (!st_value_hashtable_has_key2(self->cached_query, statement_name)) {
		PGresult * prepare = PQprepare(self->connect, statement_name, query, 0, NULL);
		ExecStatusType status = PQresultStatus(prepare);
		if (status == PGRES_FATAL_ERROR) {
			st_database_postgresql_get_error(prepare, statement_name);
			st_log_write2(status == PGRES_COMMAND_OK ? st_log_level_debug : st_log_level_error, st_log_type_plugin_db, "Postgresql: new query prepared (%s) => {%s}, status: %s", statement_name, query, PQresStatus(status));
		} else
			st_value_hashtable_put2(self->cached_query, statement_name, st_value_new_string(query), true);
		PQclear(prepare);
	}
}

