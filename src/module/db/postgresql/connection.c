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
*  Last modified: Sat, 04 Aug 2012 00:46:04 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// PQclear, PQexec, PQexecPrepared, PQfinish, PQresultStatus
// PQsetErrorVerbosity, PQtransactionStatus
#include <postgresql/libpq-fe.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>

#include <libstone/library/media.h>
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

static int st_db_postgresql_sync_plugin_checksum(struct st_database_connection * connect, const char * name);

static int st_db_postgresql_get_media(struct st_database_connection * connect, struct st_media * media, const char * uuid, const char * medium_serial_number, const char * label);
static int st_db_postgresql_get_media_format(struct st_database_connection * connect, struct st_media_format * media_format, unsigned char density_code, enum st_media_format_mode mode);

static void st_db_postgresql_prepare(struct st_db_postgresql_connection_private * self, const char * statement_name, const char * query);

static struct st_database_connection_ops st_db_postgresql_connection_ops = {
	.close = st_db_postgresql_close,
	.free  = st_db_postgresql_free,

	.cancel_transaction = st_db_postgresql_cancel_transaction,
	.finish_transaction = st_db_postgresql_finish_transaction,
	.start_transaction  = st_db_postgresql_start_transaction,

	.sync_plugin_checksum = st_db_postgresql_sync_plugin_checksum,

	.get_media        = st_db_postgresql_get_media,
	.get_media_format = st_db_postgresql_get_media_format,
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
	connect->data = self;
	connect->ops = &st_db_postgresql_connection_ops;

	return connect;
}


int st_db_postgresql_cancel_transaction(struct st_database_connection * connect) {
	if (!connect)
		return -1;

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
	if (!connect)
		return -1;

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
	if (!connect)
		return -1;

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


int st_db_postgresql_sync_plugin_checksum(struct st_database_connection * connect, const char * name) {
	if (!connect || !name)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query = "select_checksum_by_name";
	st_db_postgresql_prepare(self, query, "SELECT name FROM checksum WHERE name = $1 LIMIT 1");

	const char * param[] = { name};
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	int found = 0;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		found = 1;

	PQclear(result);

	if (found)
		return 0;

	query = "insert_checksum";
	st_db_postgresql_prepare(self, query, "INSERT INTO checksum(name) VALUES ($1)");

	result = PQexecPrepared(self->connect, query, 1, param, 0, 0, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	PQclear(result);

	return status == PGRES_FATAL_ERROR;
}


int st_db_postgresql_get_media(struct st_database_connection * connect, struct st_media * media, const char * uuid, const char * medium_serial_number, const char * label) {
	if (!connect || !media)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;

	const char * query;
	PGresult * result;

	if (uuid) {
		query = "select_media_by_uuid";
		st_db_postgresql_prepare(self, query, "SELECT uuid, label, mediumserialnumber, t.name, status, location, firstused, usebefore, loadcount, readcount, writecount, t.blocksize, endpos, nbfiles, densitycode, mode FROM tape t LEFT JOIN tapeformat tf ON t.tapeformat = tf.id WHERE uuid = $1 LIMIT 1");

		const char * param[] = { uuid };
		result = PQexecPrepared(self->connect, query, 1, param, 0, 0, 0);
	} else if (medium_serial_number) {
		query = "select_media_by_medium_serial_number";
		st_db_postgresql_prepare(self, query, "SELECT uuid, label, mediumserialnumber, t.name, status, location, firstused, usebefore, loadcount, readcount, writecount, t.blocksize, endpos, nbfiles, densitycode, mode FROM tape t LEFT JOIN tapeformat tf ON t.tapeformat = tf.id WHERE mediumserialnumber = $1 LIMIT 1");

		const char * param[] = { medium_serial_number };
		result = PQexecPrepared(self->connect, query, 1, param, 0, 0, 0);
	} else {
		query = "select_media_by_label";
		st_db_postgresql_prepare(self, query, "SELECT uuid, label, mediumserialnumber, t.name, status, location, firstused, usebefore, loadcount, readcount, writecount, t.blocksize, endpos, nbfiles, densitycode, mode FROM tape t LEFT JOIN tapeformat tf ON t.tapeformat = tf.id WHERE label = $1 LIMIT 1");

		const char * param[] = { label };
		result = PQexecPrepared(self->connect, query, 1, param, 0, 0, 0);
	}

	ExecStatusType status = PQresultStatus(result);
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_string(result, 0, 0, media->uuid, 37);
		st_db_postgresql_get_string_dup(result, 0, 1, &media->label);
		st_db_postgresql_get_string_dup(result, 0, 2, &media->medium_serial_number);
		st_db_postgresql_get_string_dup(result, 0, 3, &media->name);

		media->status = st_media_string_to_status(PQgetvalue(result, 0, 4));
		media->location = st_media_string_to_location(PQgetvalue(result, 0, 5));

		// media->first_used
		// media->use_before

		st_db_postgresql_get_long(result, 0, 8, &media->load_count);
		st_db_postgresql_get_long(result, 0, 9, &media->read_count);
		st_db_postgresql_get_long(result, 0, 10, &media->write_count);
		// st_db_postgresql_get_long(result, 0, 11, &media->operation_count);

		st_db_postgresql_get_ssize(result, 0, 11, &media->block_size);
		st_db_postgresql_get_ssize(result, 0, 12, &media->end_position);
		// media->available_block

		st_db_postgresql_get_uint(result, 0, 13, &media->nb_volumes);
		// media->type

		unsigned char density_code;
		st_db_postgresql_get_uchar(result, 0, 14, &density_code);
		enum st_media_format_mode mode = st_media_string_to_format_mode(PQgetvalue(result, 0, 15));
		media->format = st_media_format_get_by_density_code(density_code, mode);

		PQclear(result);
		return 0;
	}

	PQclear(result);
	return 1;
}

int st_db_postgresql_get_media_format(struct st_database_connection * connect, struct st_media_format * media_format, unsigned char density_code, enum st_media_format_mode mode) {
	if (!connect || !media_format)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;
	char * c_density_code = 0;
	asprintf(&c_density_code, "%d", density_code);

	const char * query = "select_media_format";
	st_db_postgresql_prepare(self, query, "SELECT name, datatype, maxloadcount, maxreadcount, maxwritecount, maxopcount, lifespan, capacity, blocksize, supportpartition, supportmam FROM tapeformat WHERE densitycode = $1 AND mode = $2 LIMIT 1");

	const char * param[] = { c_density_code, st_media_format_mode_to_string(mode) };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_string(result, 0, 0, media_format->name, 64);
		media_format->density_code = density_code;
		media_format->type = st_media_string_to_format_data(PQgetvalue(result, 0, 1));
		media_format->mode = mode;

		st_db_postgresql_get_long(result, 0, 2, &media_format->max_load_count);
		st_db_postgresql_get_long(result, 0, 3, &media_format->max_read_count);
		st_db_postgresql_get_long(result, 0, 4, &media_format->max_write_count);
		st_db_postgresql_get_long(result, 0, 5, &media_format->max_operation_count);

		media_format->life_span = 0;

		st_db_postgresql_get_ssize(result, 0, 7, &media_format->capacity);
		st_db_postgresql_get_ssize(result, 0, 8, &media_format->block_size);

		st_db_postgresql_get_bool(result, 0, 9, &media_format->support_partition);
		st_db_postgresql_get_bool(result, 0, 10, &media_format->support_mam);

		PQclear(result);
		return 0;
	}

	PQclear(result);
	return 1;
}


void st_db_postgresql_prepare(struct st_db_postgresql_connection_private * self, const char * statement_name, const char * query) {
	if (!st_hashtable_has_key(self->cached_query, statement_name)) {
		PGresult * prepare = PQprepare(self->connect, statement_name, query, 0, 0);
		ExecStatusType status = PQresultStatus(prepare);
		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(prepare, statement_name);
		else
			st_hashtable_put(self->cached_query, strdup(statement_name), 0);
		PQclear(prepare);

		st_log_write_all(status == PGRES_COMMAND_OK ? st_log_level_debug : st_log_level_error, st_log_type_plugin_db, "Postgresql: new query prepared (%s) => {%s}, status: %s", statement_name, query, PQresStatus(status));
	}
}

