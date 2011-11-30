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
*  Last modified: Wed, 30 Nov 2011 19:27:57 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
// free, malloc
#include <malloc.h>
// PQbackendPID, PQclear, PQerrorMessage, PQexec, PQfinish, PQfnumber, PQgetvalue, PQntuples, PQsetdbLogin, PQresultStatus, PQstatus
#include <postgresql/libpq-fe.h>
// sscanf, snprintf
#include <stdio.h>
// strcmp, strdup
#include <string.h>
// strptime
#include <time.h>
// sleep
#include <unistd.h>

#include <storiqArchiver/library/tape.h>
#include <storiqArchiver/log.h>

#include "common.h"

static void sa_db_postgresql_check(struct sa_db_postgresql_connetion_private * self);
static int sa_db_postgresql_con_close(struct sa_database_connection * connection);
static int sa_db_postgresql_con_free(struct sa_database_connection * connection);

static int sa_db_postgresql_cancel_transaction(struct sa_database_connection * connection);
static int sa_db_postgresql_finish_transaction(struct sa_database_connection * connection);
static int sa_db_postgresql_start_transaction(struct sa_database_connection * connection, short read_only);

static int sa_db_postgresql_get_tape_format(struct sa_database_connection * db, struct sa_tape_format * tape_format, unsigned char density_code);
static int sa_db_postgresql_sync_tape(struct sa_database_connection * db, struct sa_tape * tape);

static int sa_db_postgresql_get_bool(PGresult * result, int row, int column, char * value);
static void sa_db_postgresql_get_error(PGresult * result);
//static int sa_db_postgresql_get_int(PGresult * result, int row, int column, int * value);
static int sa_db_postgresql_get_long(PGresult * result, int row, int column, long * value);
static int sa_db_postgresql_get_string(PGresult * result, int row, int column, char * value);
static int sa_db_postgresql_get_time(PGresult * result, int row, int column, time_t * value);

static const char * sa_db_postgresql_wrapper_status_to_string(enum sa_tape_status status);

static struct sa_database_connection_ops sa_db_postgresql_con_ops = {
	.close = sa_db_postgresql_con_close,
	.free  = sa_db_postgresql_con_free,

	.cancel_transaction = sa_db_postgresql_cancel_transaction,
	.finish_transaction = sa_db_postgresql_finish_transaction,
	.start_transaction  = sa_db_postgresql_start_transaction,

	.get_tape_format = sa_db_postgresql_get_tape_format,
	.sync_tape       = sa_db_postgresql_sync_tape,
};


int sa_db_postgresql_cancel_transaction(struct sa_database_connection * connection) {
	if (!connection)
		return -1;

	struct sa_db_postgresql_connetion_private * self = connection->data;
	sa_db_postgresql_check(self);

	PGresult * result = PQexec(self->db_con, "ROLLBACK;");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		sa_log_write_all(sa_log_level_error, "Db: Postgresql: error while cancelling a transaction (%s)", PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

void sa_db_postgresql_check(struct sa_db_postgresql_connetion_private * self) {
	if (PQstatus(self->db_con) != CONNECTION_OK) {
		int i;
		for (i = 0; i < 3; i++) {
			PQreset(self->db_con);
			if (PQstatus(self->db_con) != CONNECTION_OK) {
				sa_log_write_all(sa_log_level_error, "Db: Postgresql: Failed to reset database connection");
				sleep(30);
			} else
				break;
		}
	}
}

int sa_db_postgresql_con_close(struct sa_database_connection * connection) {
	if (!connection)
		return -1;

	struct sa_db_postgresql_connetion_private * self = connection->data;

	if (!self->db_con)
		return 0;

	PQfinish(self->db_con);

	self->db_con = 0;

	return 0;
}

int sa_db_postgresql_con_free(struct sa_database_connection * connection) {
	if (!connection)
		return -1;

	struct sa_db_postgresql_connetion_private * self = connection->data;

	if (!self->db_con)
		return 0;

	ConnStatusType status = PQstatus(self->db_con);

	if (status == CONNECTION_OK) {
		PQfinish(self->db_con);
		self->db_con = 0;
	}

	connection->id = 0;
	connection->driver = 0;
	connection->ops = 0;
	connection->data = 0;

	free(self);

	return 0;
}

int sa_db_postgresql_get_tape_format(struct sa_database_connection * connection, struct sa_tape_format * tape_format, unsigned char density_code) {
	char * query = 0;
	asprintf(&query, "SELECT * FROM tapeformat WHERE densityCode = %d LIMIT 1", density_code);

	struct sa_db_postgresql_connetion_private * self = connection->data;
	sa_db_postgresql_check(self);

	PGresult * result = PQexec(self->db_con, query);
	free(query);

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		sa_db_postgresql_get_long(result, 0, 0, &tape_format->id);
		sa_db_postgresql_get_string(result, 0, 1, tape_format->name);
		tape_format->density_code = density_code;
		tape_format->type = sa_tape_string_to_format_data(PQgetvalue(result, 0, 2));
		tape_format->mode = sa_tape_string_to_format_mode(PQgetvalue(result, 0, 3));
		sa_db_postgresql_get_long(result, 0, 4, &tape_format->max_load_count);
		sa_db_postgresql_get_long(result, 0, 5, &tape_format->max_read_count);
		sa_db_postgresql_get_long(result, 0, 6, &tape_format->max_write_count);
		sa_db_postgresql_get_long(result, 0, 7, &tape_format->max_op_count);
		sa_db_postgresql_get_long(result, 0, 8, &tape_format->life_span);
		sa_db_postgresql_get_long(result, 0, 9, &tape_format->capacity);
		sa_db_postgresql_get_long(result, 0, 10, &tape_format->block_size);
		sa_db_postgresql_get_bool(result, 0, 11, &tape_format->support_partition);

		PQclear(result);
		return 0;
	}

	PQclear(result);
	return 1;
}

int sa_db_postgresql_finish_transaction(struct sa_database_connection * connection) {
	if (!connection)
		return -1;

	struct sa_db_postgresql_connetion_private * self = connection->data;
	sa_db_postgresql_check(self);

	PGresult * result = PQexec(self->db_con, "COMMIT;");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		sa_log_write_all(sa_log_level_error, "Db: Postgresql: error while committing a transaction (%s)", PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

int sa_db_postgresql_init_connection(struct sa_database_connection * connection, struct sa_db_postgresql_private * driver_private) {
	if (!connection || !connection->driver)
		return -1;

	PGconn * con = PQsetdbLogin(driver_private->host, driver_private->port, 0, 0, driver_private->db, driver_private->user, driver_private->password);
	ConnStatusType status = PQstatus(con);

	if (status == CONNECTION_BAD) {
		PQfinish(con);
		return -2;
	}

	connection->id = PQbackendPID(con);
	connection->ops = &sa_db_postgresql_con_ops;

	struct sa_db_postgresql_connetion_private * self = malloc(sizeof(struct sa_db_postgresql_connetion_private));
	self->db_con = con;

	connection->data = self;

	return 0;
}

// TODO: check this function
int sa_db_postgresql_start_transaction(struct sa_database_connection * connection, short readOnly) {
	if (!connection)
		return -1;

	struct sa_db_postgresql_connetion_private * self = connection->data;
	sa_db_postgresql_check(self);

	PGresult * result = PQexec(self->db_con, readOnly ? "BEGIN READ ONLY;" : "BEGIN;");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		sa_log_write_all(sa_log_level_error, "Db: Postgresql: error while starting a transaction (%s)", PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

int sa_db_postgresql_sync_tape(struct sa_database_connection * connection, struct sa_tape * tape) {
	struct sa_db_postgresql_connetion_private * self = connection->data;
	sa_db_postgresql_check(self);

	char * query = 0;
	if (tape->id < 0 && tape->label[0] != '\0') {
		asprintf(&query, "SELECT * FROM tape WHERE label = '%s' LIMIT 1", tape->label);

		PGresult * result = PQexec(self->db_con, query);
		if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			sa_db_postgresql_get_long(result, 0, 0, &tape->id);
			sa_db_postgresql_get_string(result, 0, 2, tape->name);
			sa_db_postgresql_get_time(result, 0, 5, &tape->first_used);
			sa_db_postgresql_get_time(result, 0, 6, &tape->use_before);

			long old_value = tape->load_count;
			sa_db_postgresql_get_long(result, 0, 7, &tape->load_count);
			tape->load_count += old_value;

			old_value = tape->read_count;
			sa_db_postgresql_get_long(result, 0, 8, &tape->read_count);
			tape->read_count += old_value;

			old_value = tape->write_count;
			sa_db_postgresql_get_long(result, 0, 9, &tape->write_count);
			tape->write_count += old_value;

			sa_db_postgresql_get_long(result, 0, 12, &tape->block_size);
		}

		free(query);
		PQclear(result);
	}

	if (tape->id < 0 && tape->name[0] != '\0') {
		asprintf(&query, "SELECT * FROM tape WHERE name = '%s' LIMIT 1", tape->name);

		PGresult * result = PQexec(self->db_con, query);
		if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			sa_db_postgresql_get_long(result, 0, 0, &tape->id);
			sa_db_postgresql_get_string(result, 0, 1, tape->label);
			sa_db_postgresql_get_time(result, 0, 5, &tape->first_used);
			sa_db_postgresql_get_time(result, 0, 6, &tape->use_before);

			long old_value = tape->load_count;
			sa_db_postgresql_get_long(result, 0, 7, &tape->load_count);
			tape->load_count += old_value;

			old_value = tape->read_count;
			sa_db_postgresql_get_long(result, 0, 8, &tape->read_count);
			tape->read_count += old_value;

			old_value = tape->write_count;
			sa_db_postgresql_get_long(result, 0, 9, &tape->write_count);
			tape->write_count += old_value;

			sa_db_postgresql_get_long(result, 0, 12, &tape->block_size);
		}

		free(query);
		PQclear(result);
	}

	if (tape->id > -1) {
		asprintf(&query, "UPDATE tape SET status = '%s', location = '%s', loadcount = %ld, readcount = %ld, writecount = %ld, endpos = %zd, nbfiles = %u, blocksize = %zd, haspartition = %s WHERE id = %ld", sa_db_postgresql_wrapper_status_to_string(tape->status), sa_tape_location_to_string(tape->location), tape->load_count, tape->read_count, tape->write_count, tape->end_position, tape->nb_files, tape->block_size, tape->has_partition ? "true" : "false", tape->id);

		PGresult * result = PQexec(self->db_con, query);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			sa_db_postgresql_get_error(result);

		free(query);
		PQclear(result);

		return status != PGRES_COMMAND_OK;
	} else {
		char label[39];
		if (tape->label[0] != '\0')
			snprintf(label, 39, "'%s'", tape->label);
		else
			strcpy(label, "NULL");

		char buffer_first_used[32];
		char buffer_use_before[32];

		struct tm tv;
		localtime_r(&tape->first_used, &tv);
		strftime(buffer_first_used, 32, "%F %T", &tv);

		localtime_r(&tape->use_before, &tv);
		strftime(buffer_use_before, 32, "%F %T", &tv);

		asprintf(&query, "INSERT INTO tape VALUES (DEFAULT, %s, '%s', '%s', '%s', '%s', '%s', %ld, %ld, %ld, %zd, %u, %zd, '%s', %ld, NULL)", label, tape->name, sa_db_postgresql_wrapper_status_to_string(tape->status), sa_tape_location_to_string(tape->location), buffer_first_used, buffer_use_before, tape->load_count, tape->read_count, tape->write_count, tape->end_position, tape->nb_files, tape->block_size, tape->has_partition ? "true" : "false", tape->format->id);

		PGresult * result = PQexec(self->db_con, query);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			sa_db_postgresql_get_error(result);

		free(query);
		PQclear(result);

		return status != PGRES_COMMAND_OK;
	}
}


int sa_db_postgresql_get_bool(PGresult * result, int row, int column, char * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value)
		*val = strcmp(value, "t") ? 1 : 0;

	return value != 0;
}

void sa_db_postgresql_get_error(PGresult * result) {
	char * error = PQresultErrorMessage(result);

	sa_log_write_all(sa_log_level_error, "Db: Postgresql: error => %s", error);
}

/*int sa_db_postgresql_get_int(PGresult * result, int row, int column, int * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%d", val) == 1)
		return 0;
	return -1;
}*/

int sa_db_postgresql_get_long(PGresult * result, int row, int column, long * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%ld", val) == 1)
		return 0;
	return -1;
}

int sa_db_postgresql_get_string(PGresult * result, int row, int column, char * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value) {
		strcpy(val, value);
		return 0;
	}
	return -1;
}

int sa_db_postgresql_get_time(PGresult * result, int row, int column, time_t * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	struct tm tv;

	int failed = strptime(value, "%F %T", &tv) ? 0 : 1;
	if (!failed)
		*val = mktime(&tv);

	return failed;
}


const char * sa_db_postgresql_wrapper_status_to_string(enum sa_tape_status status) {
	switch (status) {
		case SA_TAPE_STATUS_IN_USE:
			return "in_use";

		case SA_TAPE_STATUS_NEEDS_REPLACEMENT:
			return "needs_replacement";

		default:
			return sa_tape_status_to_string(status);
	}
}

