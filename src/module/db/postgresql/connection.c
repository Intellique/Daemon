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
*  Last modified: Fri, 13 Jul 2012 10:16:24 +0200                         *
\*************************************************************************/

// PQclear, PQexec, PQfinish, PQresultStatus, PQsetErrorVerbosity
// PQtransactionStatus
#include <postgresql/libpq-fe.h>
// free, malloc
#include <stdlib.h>
// strdup, strtok_r
#include <string.h>

#include <libstone/log.h>
#include <libstone/util/hashtable.h>
#include <libstone/util/string.h>
#include <libstone/util/util.h>

#include "common.h"

struct st_db_postgresql_connection_private {
	PGconn * connect;
	struct st_hashtable * cached_query;
};

static int st_db_postgresql_close(struct st_database_connection * connect);
static int st_db_postgresql_free(struct st_database_connection * connect);

static int st_db_postgresql_cancel_transaction(struct st_database_connection * connect);
static int st_db_postgresql_finish_transaction(struct st_database_connection * connect);
static int st_db_postgresql_start_transaction(struct st_database_connection * connect);

static void st_db_postgresql_get_error(PGresult * result, const char * prepared_query);

static struct st_database_connection_ops st_db_postgresql_connection_ops = {
	.close = st_db_postgresql_close,
	.free  = st_db_postgresql_free,

	.cancel_transaction = st_db_postgresql_cancel_transaction,
	.finish_transaction = st_db_postgresql_finish_transaction,
	.start_transaction  = st_db_postgresql_start_transaction,
};


int st_db_postgresql_close(struct st_database_connection * connect) {
	struct st_db_postgresql_connection_private * self = connect->data;

	if (self->connect)
		PQfinish(self->connect);
	self->connect = 0;

	if (self->cached_query)
		st_hashtable_free(self->cached_query);
	self->cached_query = 0;

	return 0;
}

int st_db_postgresql_free(struct st_database_connection * connect) {
	struct st_db_postgresql_connection_private * self = connect->data;

	st_db_postgresql_close(connect);
	free(self);

	connect->data = 0;
	connect->driver = 0;
	connect->config = 0;
	free(connect);

	return 0;
}

struct st_database_connection * st_db_postgresql_connnect_init(PGconn * pg_connect) {
	PQsetErrorVerbosity(pg_connect, PQERRORS_VERBOSE);

	struct st_db_postgresql_connection_private * self = malloc(sizeof(struct st_db_postgresql_connection_private));
	self->connect = pg_connect;
	self->cached_query = st_hashtable_new2(st_util_string_compute_hash, st_util_basic_free);

	struct st_database_connection * connect = malloc(sizeof(struct st_database_connection));
	connect->ops = &st_db_postgresql_connection_ops;

	return connect;
}


int st_db_postgresql_cancel_transaction(struct st_database_connection * connect) {
	struct st_db_postgresql_connection_private * self = connect->data;

	PGTransactionStatusType status = PQtransactionStatus(self->connect);
	switch (status) {
		case PQTRANS_INERROR:
		case PQTRANS_INTRANS: {
			PGresult * result = PQexec(self->connect, "ROLL BACK");
			ExecStatusType roll_back_status = PQresultStatus(result);
			if (roll_back_status != PGRES_COMMAND_OK)
				st_db_postgresql_get_error(result, 0);
			PQclear(result);
			return roll_back_status != PGRES_COMMAND_OK;
		}
		break;

		default:
			return 0;
	}
}

int st_db_postgresql_finish_transaction(struct st_database_connection * connect) {
	struct st_db_postgresql_connection_private * self = connect->data;

	PGTransactionStatusType status = PQtransactionStatus(self->connect);
	switch (status) {
		case PQTRANS_INERROR: {
			PGresult * result = PQexec(self->connect, "ROLL BACK");
			PQclear(result);
			return 1;
		}
		break;

		case PQTRANS_INTRANS: {
			PGresult * result = PQexec(self->connect, "COMMIT");
			ExecStatusType commit_status = PQresultStatus(result);
			if (commit_status != PGRES_COMMAND_OK)
				st_db_postgresql_get_error(result, 0);
			PQclear(result);
			return commit_status != PGRES_COMMAND_OK;
		}
		break;

		default:
			return 0;
	}
}

int st_db_postgresql_start_transaction(struct st_database_connection * connect) {
	struct st_db_postgresql_connection_private * self = connect->data;

	PGTransactionStatusType status = PQtransactionStatus(self->connect);
	switch (status) {
		case PQTRANS_IDLE: {
			PGresult * result = PQexec(self->connect, "COMMIT");
			ExecStatusType commit_status = PQresultStatus(result);
			if (commit_status != PGRES_COMMAND_OK)
				st_db_postgresql_get_error(result, 0);
			PQclear(result);
			return commit_status != PGRES_COMMAND_OK;
		}
		break;

		default:
			return 1;
	}
}


void st_db_postgresql_get_error(PGresult * result, const char * prepared_query) {
	char * error = PQresultErrorField(result, PG_DIAG_MESSAGE_PRIMARY);
	if (prepared_query)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: error {%s} => %s", prepared_query, error);
	else
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: error => %s", error);

	error = PQresultErrorField(result, PG_DIAG_MESSAGE_DETAIL);
	if (error) {
		error = strdup(error);
		char * ptr;
		char * line = strtok_r(error, "\n", &ptr);
		while (line) {
			st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: detail => %s", line);
			line = strtok_r(0, "\n", &ptr);
		}
		free(error);
	}

	error = PQresultErrorField(result, PG_DIAG_MESSAGE_HINT);
	if (error) {
		error = strdup(error);
		char * ptr;
		char * line = strtok_r(error, "\n", &ptr);
		while (line) {
			st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: hint => %s", line);
			line = strtok_r(0, "\n", &ptr);
		}
		free(error);
	}
}

