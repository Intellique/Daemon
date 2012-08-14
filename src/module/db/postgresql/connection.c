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
*  Last modified: Tue, 14 Aug 2012 15:29:08 +0200                         *
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
// uname
#include <sys/utsname.h>
// localtime_r, strftime
#include <time.h>

#include <libstone/library/changer.h>
#include <libstone/library/drive.h>
#include <libstone/library/media.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>
#include <libstone/util/hashtable.h>
#include <libstone/util/string.h>
#include <libstone/util/util.h>

#include "common.h"

struct st_db_postgresql_connection_private {
	PGconn * connect;
	struct st_hashtable * cached_query;
};

struct st_db_postgresql_changer_data {
	long id;
};

struct st_db_postgresql_drive_data {
	long id;
};

struct st_db_postgresql_media_data {
	long id;
};

struct st_db_postgresql_slot_data {
	long id;
};

static int st_db_postgresql_close(struct st_database_connection * connect);
static int st_db_postgresql_free(struct st_database_connection * connect);

static int st_db_postgresql_cancel_checkpoint(struct st_database_connection * connect, const char * checkpoint);
static int st_db_postgresql_cancel_transaction(struct st_database_connection * connect);
static int st_db_postgresql_create_checkpoint(struct st_database_connection * connect, const char * checkpoint);
static int st_db_postgresql_finish_transaction(struct st_database_connection * connect);
static int st_db_postgresql_start_transaction(struct st_database_connection * connect);

static int st_db_postgresql_sync_plugin_checksum(struct st_database_connection * connect, const char * name);

static char * st_db_postgresql_get_host(struct st_database_connection * connect);
static int st_db_postgresql_is_changer_contain_drive(struct st_database_connection * connect, struct st_changer * changer, struct st_drive * drive);
static int st_db_postgresql_sync_changer(struct st_database_connection * connect, struct st_changer * changer);
static int st_db_postgresql_sync_drive(struct st_database_connection * connect, struct st_drive * drive);
static int st_db_postgresql_sync_media(struct st_database_connection * connect, struct st_media * media);
static int st_db_postgresql_sync_slot(struct st_database_connection * connect, struct st_slot * slot);

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

	.is_changer_contain_drive = st_db_postgresql_is_changer_contain_drive,
	.sync_changer             = st_db_postgresql_sync_changer,
	.sync_drive               = st_db_postgresql_sync_drive,

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


int st_db_postgresql_cancel_checkpoint(struct st_database_connection * connect, const char * checkpoint) {
	if (!connect || !checkpoint)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;
	char * query = 0;
	asprintf(&query, "ROLLBACK TO %s", checkpoint);

	PGresult * result = PQexec(self->connect, query);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, 0);

	if (status != PGRES_COMMAND_OK)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: error while rollbacking a savepoint => %s", PQerrorMessage(self->connect));

	PQclear(result);
	free(query);

	return status == PGRES_COMMAND_OK ? 0 : -1;
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

int st_db_postgresql_create_checkpoint(struct st_database_connection * connect, const char * checkpoint) {
	if (!connect || !checkpoint)
		return -1;

	st_log_write_all(st_log_level_debug, st_log_type_plugin_db, "Create savepoint '%s'", checkpoint);

	struct st_db_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType transStatus = PQtransactionStatus(self->connect);

	if (transStatus == PQTRANS_INERROR) {
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: Can't create checkpoint because there is an error into current transaction");
		return -1;
	} else if (transStatus == PQTRANS_IDLE) {
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: Can't create checkpoint because there is no active transaction");
		return -1;
	}

	char * query = 0;
	asprintf(&query, "SAVEPOINT %s", checkpoint);

	PGresult * result = PQexec(self->connect, query);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, 0);

	if (status != PGRES_COMMAND_OK)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: error while creating a savepoint => %s", PQerrorMessage(self->connect));

	PQclear(result);
	free(query);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

int st_db_postgresql_finish_transaction(struct st_database_connection * connect) {
	if (!connect)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;

	PGTransactionStatusType status = PQtransactionStatus(self->connect);
	switch (status) {
		case PQTRANS_INERROR: {
			st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: Rolling back transaction because current transaction is in error");

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
			PGresult * result = PQexec(self->connect, "BEGIN");
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


char * st_db_postgresql_get_host(struct st_database_connection * connect) {
	struct st_db_postgresql_connection_private * self = connect->data;

	const char * query = "select_host_by_name";
	st_db_postgresql_prepare(self, query, "SELECT id FROM host WHERE name = $1 OR name || '.' || domaine = $1 LIMIT 1");

	struct utsname name;
	uname(&name);

	const char * param[] = { name.nodename };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	char * hostid = 0;

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_string_dup(result, 0, 0, &hostid);
	else
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: Host not found into database (%s)", name.nodename);

	PQclear(result);

	return hostid;
}

int st_db_postgresql_is_changer_contain_drive(struct st_database_connection * connect, struct st_changer * changer, struct st_drive * drive) {
	if (!connect || !changer || !drive)
		return 0;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query = "select_changer_of_drive_by_model_vendor_serialnumber";
	st_db_postgresql_prepare(self, query, "SELECT changer FROM drive WHERE model = $1 AND vendor = $2 AND serialnumber = $3 LIMIT 1");

	char * changerid = 0;

	const char * param[] = { drive->model, drive->vendor, drive->serial_number };
	PGresult * result = PQexecPrepared(self->connect, query, 3, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result, query);
		PQclear(result);
		return 0;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_string_dup(result, 0, 0, &changerid);
		PQclear(result);
	} else {
		PQclear(result);
		return 0;
	}

	query = "select_changer_by_model_vendor_serialnumber_id";
	st_db_postgresql_prepare(self, query, "SELECT id FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 AND id = $4 LIMIT 1");

	const char * params2[] = { changer->model, changer->vendor, changer->serial_number, changerid };
	result = PQexecPrepared(self->connect, query, 4, params2, 0, 0, 0);
	status = PQresultStatus(result);

	int ok = 0;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (PQntuples(result) == 1)
		ok = 1;

	PQclear(result);
	free(changerid);

	return ok;
}

int st_db_postgresql_sync_changer(struct st_database_connection * connect, struct st_changer * changer) {
	if (!connect || !changer)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType transStatus = PQtransactionStatus(self->connect);
	if (transStatus == PQTRANS_INERROR)
		return 2;

	char * hostid = st_db_postgresql_get_host(connect);
	if (!hostid)
		return 1;

	if (transStatus == PQTRANS_IDLE)
		st_db_postgresql_start_transaction(connect);

	struct st_db_postgresql_changer_data * changer_data = changer->db_data;
	if (!changer_data) {
		changer->db_data = changer_data = malloc(sizeof(struct st_db_postgresql_changer_data));
		changer_data->id = -1;
	}

	char * changerid = 0;
	if (changer_data->id >= 0) {
		asprintf(&changerid, "%ld", changer_data->id);

		const char * query = "select_changer_by_id";
		st_db_postgresql_prepare(self, query, "SELECT enable FROM changer WHERE id = $1 FOR UPDATE NOWAIT");

		const char * param[] = { changerid };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);
		int nb_result = PQntuples(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && nb_result == 1)
			st_db_postgresql_get_bool(result, 0, 0, &changer->enabled);
		else if (status == PGRES_TUPLES_OK && nb_result == 0)
			changer->enabled = 0;

		PQclear(result);

		if (status == PGRES_FATAL_ERROR || nb_result == 0) {
			free(hostid);
			free(changerid);
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connect);
			return 3;
		}
	} else {
		const char * query = "select_changer_by_model_vendor_serialnumber";
		st_db_postgresql_prepare(self, query, "SELECT id, enable FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 FOR UPDATE NOWAIT");

		const char * param[] = { changer->model, changer->vendor, changer->serial_number };
		PGresult * result = PQexecPrepared(self->connect, query, 3, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_long(result, 0, 0, &changer_data->id);
			st_db_postgresql_get_string_dup(result, 0, 0, &changerid);
			st_db_postgresql_get_bool(result, 0, 1, &changer->enabled);
		}

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			free(hostid);
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connect);
			return 3;
		}
	}

	if (changer_data->id >= 0) {
		const char * query = "update_changer";
		st_db_postgresql_prepare(self, query, "UPDATE changer SET device = $1, status = $2, firmwarerev = $3 WHERE id = $4");

		const char * params[] = { changer->device, st_changer_status_to_string(changer->status), changer->revision, changerid };
		PGresult * result = PQexecPrepared(self->connect, query, 4, params, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		free(changerid);

		if (status == PGRES_FATAL_ERROR) {
			st_db_postgresql_get_error(result, query);

			free(hostid);
			PQclear(result);

			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connect);
			return -2;
		}

		PQclear(result);
	} else {
		const char * query = "insert_changer";
		st_db_postgresql_prepare(self, query, "INSERT INTO changer(device, status, barcode, model, vendor, firmwarerev, serialnumber, host) VALUES ($1, $2, $3, $4, $5, $6, $7, $8) RETURNING id");

		const char * param[] = {
			changer->device, st_changer_status_to_string(changer->status), changer->barcode ? "true" : "false",
			changer->model, changer->vendor, changer->revision, changer->serial_number, hostid,
		};

		PGresult * result = PQexecPrepared(self->connect, query, 8, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &changer_data->id);

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			free(hostid);
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connect);
			return -1;
		}
	}

	free(hostid);

	st_db_postgresql_create_checkpoint(connect, "sync_changer_before_drive");

	unsigned int i;
	int status2 = 0;
	for (i = 0; i < changer->nb_drives && !status2; i++)
		status2 = st_db_postgresql_sync_drive(connect, changer->drives + i);

	if (status2)
		st_db_postgresql_cancel_checkpoint(connect, "sync_changer_before_drive");

	status2 = 0;
	st_db_postgresql_create_checkpoint(connect, "sync_changer_before_slot");

	for (i = changer->nb_drives; i < changer->nb_slots && !status2; i++)
		status2 = st_db_postgresql_sync_slot(connect, changer->slots + i);

	if (status2)
		st_db_postgresql_cancel_checkpoint(connect, "sync_changer_before_slot");

	if (transStatus == PQTRANS_IDLE)
		st_db_postgresql_finish_transaction(connect);

	return 0;
}

int st_db_postgresql_sync_drive(struct st_database_connection * connect, struct st_drive * drive) {
	if (!connect || !drive)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType transStatus = PQtransactionStatus(self->connect);
	if (transStatus == PQTRANS_INERROR)
		return 2;
	else if (transStatus == PQTRANS_IDLE)
		st_db_postgresql_start_transaction(connect);

	struct st_db_postgresql_changer_data * changer_data = drive->changer->db_data;
	struct st_db_postgresql_drive_data * drive_data = drive->db_data;
	if (!drive_data) {
		drive->db_data = drive_data = malloc(sizeof(struct st_db_postgresql_drive_data));
		drive_data->id = -1;
	}

	char * changerid = 0, * driveid = 0, * driveformatid = 0;
	asprintf(&changerid, "%ld", changer_data->id);

	if (drive_data->id >= 0) {
		asprintf(&driveid, "%ld", drive_data->id);

		const char * query = "select_drive_by_id";
		st_db_postgresql_prepare(self, query, "SELECT driveformat, enable FROM drive WHERE id = $1 FOR UPDATE NOWAIT");

		const char * param[] = { driveid };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);
		int nb_result = PQntuples(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && nb_result == 1) {
			if (!PQgetisnull(result, 0, 0))
				st_db_postgresql_get_string_dup(result, 0, 0, &driveformatid);
			st_db_postgresql_get_bool(result, 0, 1, &drive->enabled);
		} else if (status == PGRES_TUPLES_OK && nb_result == 1)
			drive->enabled = 0;

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			free(changerid);
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connect);
			return 3;
		}
	} else {
		const char * query = "select_drive_by_model_vendor_serialnumber";
		st_db_postgresql_prepare(self, query, "SELECT id, operationduration, lastclean, driveformat, enable FROM drive WHERE model = $1 AND vendor = $2 AND serialnumber = $3 FOR UPDATE NOWAIT");

		const char * param[] = { drive->model, drive->vendor, drive->serial_number };
		PGresult * result = PQexecPrepared(self->connect, query, 3, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_string_dup(result, 0, 0, &driveid);
			st_db_postgresql_get_long(result, 0, 0, &drive_data->id);

			double old_operation_duration;
			st_db_postgresql_get_double(result, 0, 1, &old_operation_duration);
			drive->operation_duration += old_operation_duration;

			st_db_postgresql_get_time(result, 0, 2, &drive->last_clean);
			if (!PQgetisnull(result, 0, 3))
				st_db_postgresql_get_string_dup(result, 0, 3, &driveformatid);
			st_db_postgresql_get_bool(result, 0, 4, &drive->enabled);
		}

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			free(changerid);
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connect);
			return 3;
		}
	}

	if (!driveformatid) {
		const char * query = "select_driveformat_by_densitycode";
		st_db_postgresql_prepare(self, query, "SELECT id FROM driveformat WHERE densitycode = $1 AND mode = $2 LIMIT 1");

		char * densitycode = 0;
		asprintf(&densitycode, "%u", drive->density_code);

		const char * param[] = { densitycode, st_media_format_mode_to_string(drive->mode), };
		PGresult * result = PQexecPrepared(self->connect, query, 2, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_string_dup(result, 0, 0, &driveformatid);

		PQclear(result);
		free(densitycode);
	}

	if (drive_data->id >= 0) {
		const char * query = "update_drive";
		st_db_postgresql_prepare(self, query, "UPDATE drive SET device = $1, scsidevice = $2, status = $3, operationduration = $4, lastclean = $5, firmwarerev = $6, driveformat = $7 WHERE id = $8");

		char * op_duration = 0;
		asprintf(&op_duration, "%.3Lf", drive->operation_duration);

		char last_clean[24];
		struct tm tm_last_clean;
		localtime_r(&drive->last_clean, &tm_last_clean);
		strftime(last_clean, 23, "%F %T", &tm_last_clean);

		const char * param[] = {
			drive->device, drive->scsi_device, st_drive_status_to_string(drive->status),
			op_duration, last_clean, drive->revision, driveformatid, driveid,
		};
		PGresult * result = PQexecPrepared(self->connect, query, 8, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);

		PQclear(result);
		free(changerid);
		free(driveid);
		free(driveformatid);
		free(op_duration);

		if (status == PGRES_FATAL_ERROR) {
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connect);
			return 3;
		}
	} else {
		const char * query = "insert_drive";
		st_db_postgresql_prepare(self, query, "INSERT INTO drive(device, scsidevice, status, changernum, operationduration, lastclean, model, vendor, firmwarerev, serialnumber, changer, driveformat) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12) RETURNING id");
		char * changer_num = 0, * op_duration = 0;
		if (drive->changer)
			asprintf(&changer_num, "%td", drive - drive->changer->drives);
		else
			changer_num = strdup("0");
		asprintf(&op_duration, "%.3Lf", drive->operation_duration);
		char last_clean[24];
		struct tm tm_last_clean;
		localtime_r(&drive->last_clean, &tm_last_clean);
		strftime(last_clean, 23, "%F %T", &tm_last_clean);

		const char * param[] = {
			drive->device, drive->scsi_device, st_drive_status_to_string(drive->status), changer_num, op_duration, last_clean,
			drive->model, drive->vendor, drive->revision, drive->serial_number, changerid, driveformatid,
		};
		PGresult * result = PQexecPrepared(self->connect, query, 12, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &drive_data->id);

		PQclear(result);
		free(changerid);
		free(changer_num);
		free(driveformatid);
		free(op_duration);

		if (status == PGRES_FATAL_ERROR) {
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connect);
			return 3;
		}
	}

	st_db_postgresql_create_checkpoint(connect, "sync_drive_before_slot");

	if (drive->slot && st_db_postgresql_sync_slot(connect, drive->slot))
		st_db_postgresql_cancel_checkpoint(connect, "sync_drive_before_slot");

	if (transStatus == PQTRANS_IDLE)
		st_db_postgresql_finish_transaction(connect);

	return 0;
}

int st_db_postgresql_sync_media(struct st_database_connection * connect, struct st_media * media) {
	struct st_db_postgresql_connection_private * self = connect->data;

	struct st_db_postgresql_media_data * media_data = media->db_data;
	if (!media_data) {
		media->db_data = media_data = malloc(sizeof(struct st_db_postgresql_media_data));
		media_data->id = -1;
	}

	char * mediaid = 0, * mediaformatid = 0;
	if (media_data->id < 0 && media->medium_serial_number) {
		const char * query = "select_tape_by_medium_serial_number";
		st_db_postgresql_prepare(self, query, "SELECT id, uuid, label, name, firstused, usebefore, loadcount, readcount, writecount, endpos, blocksize, tapeformat FROM tape WHERE mediumserialnumber = $1 FOR UPDATE NOWAIT");

		const char * param[] = { media->medium_serial_number };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_string_dup(result, 0, 0, &mediaid);
			st_db_postgresql_get_long(result, 0, 0, &media_data->id);
			st_db_postgresql_get_string(result, 0, 1, media->uuid, 37);
			st_db_postgresql_get_string_dup(result, 0, 2, &media->label);
			st_db_postgresql_get_string_dup(result, 0, 3, &media->name);
			st_db_postgresql_get_time(result, 0, 4, &media->first_used);
			st_db_postgresql_get_time(result, 0, 5, &media->use_before);

			long old_value = media->load_count;
			st_db_postgresql_get_long(result, 0, 6, &media->load_count);
			media->load_count += old_value;

			old_value = media->read_count;
			st_db_postgresql_get_long(result, 0, 7, &media->read_count);
			media->read_count += old_value;

			old_value = media->write_count;
			st_db_postgresql_get_long(result, 0, 8, &media->write_count);
			media->write_count += old_value;

			if (media->end_position == 0)
				st_db_postgresql_get_ssize(result, 0, 9, &media->end_position);
			st_db_postgresql_get_ssize(result, 0, 10, &media->block_size);

			st_db_postgresql_get_string_dup(result, 0, 11, &mediaformatid);
		}

		PQclear(result);
	}

	if (media_data->id < 0 && media->uuid[0] != '\0') {
		const char * query = "select_tape_by_uuid";
		st_db_postgresql_prepare(self, query, "SELECT id, label, name, firstused, usebefore, loadcount, readcount, writecount, endpos, blocksize, tapeformat FROM tape WHERE uuid = $1 FOR UPDATE NOWAIT");

		const char * param[] = { media->uuid };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_string_dup(result, 0, 0, &mediaid);
			st_db_postgresql_get_long(result, 0, 0, &media_data->id);
			st_db_postgresql_get_string_dup(result, 0, 1, &media->label);
			st_db_postgresql_get_string_dup(result, 0, 2, &media->name);
			st_db_postgresql_get_time(result, 0, 3, &media->first_used);
			st_db_postgresql_get_time(result, 0, 4, &media->use_before);

			long old_value = media->load_count;
			st_db_postgresql_get_long(result, 0, 5, &media->load_count);
			media->load_count += old_value;

			old_value = media->read_count;
			st_db_postgresql_get_long(result, 0, 6, &media->read_count);
			media->read_count += old_value;

			old_value = media->write_count;
			st_db_postgresql_get_long(result, 0, 7, &media->write_count);
			media->write_count += old_value;

			if (media->end_position == 0)
				st_db_postgresql_get_ssize(result, 0, 8, &media->end_position);
			st_db_postgresql_get_ssize(result, 0, 9, &media->block_size);

			st_db_postgresql_get_string_dup(result, 0, 11, &mediaformatid);
		}

		PQclear(result);
	}

	if (media_data->id < 0 && media->label) {
		const char * query = "select_tape_by_label";
		st_db_postgresql_prepare(self, query, "SELECT id, name, firstused, usebefore, loadcount, readcount, writecount, endpos, blocksize, tapeformat FROM tape WHERE label = $1 FOR UPDATE NOWAIT");

		const char * param[] = { media->label };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_string_dup(result, 0, 0, &mediaid);
			st_db_postgresql_get_long(result, 0, 0, &media_data->id);
			st_db_postgresql_get_string_dup(result, 0, 1, &media->name);
			st_db_postgresql_get_time(result, 0, 2, &media->first_used);
			st_db_postgresql_get_time(result, 0, 3, &media->use_before);

			long old_value = media->load_count;
			st_db_postgresql_get_long(result, 0, 5, &media->load_count);
			media->load_count += old_value;

			old_value = media->read_count;
			st_db_postgresql_get_long(result, 0, 6, &media->read_count);
			media->read_count += old_value;

			old_value = media->write_count;
			st_db_postgresql_get_long(result, 0, 7, &media->write_count);
			media->write_count += old_value;

			if (media->end_position == 0)
				st_db_postgresql_get_ssize(result, 0, 8, &media->end_position);
			st_db_postgresql_get_ssize(result, 0, 9, &media->block_size);

			st_db_postgresql_get_string_dup(result, 0, 11, &mediaformatid);
		}

		PQclear(result);
	}

	if (media_data->id < 0 && !mediaformatid) {
		const char * query = "select_tape_format_by_density";
		st_db_postgresql_prepare(self, query, "SELECT id FROM tapeformat WHERE densitycode = $1 AND mode = $2 FOR SHARE NOWAIT");

		char * densitycode = 0;
		asprintf(&densitycode, "%hhu", media->format->density_code);

		const char * param[] = { densitycode, st_media_format_mode_to_string(media->format->mode) };
		PGresult * result = PQexecPrepared(self->connect, query, 2, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_string_dup(result, 0, 0, &mediaformatid);

		free(densitycode);
		PQclear(result);
	}

	if (media_data->id < 0) {
		const char * query = "insert_tape";
		st_db_postgresql_prepare(self, query, "INSERT INTO tape(uuid, label, mediumserialnumber, name, status, location, firstused, usebefore, loadcount, readcount, writecount, endpos, nbfiles, blocksize, tapeformat, pool) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16) RETURNING id");

		char buffer_first_used[32];
		char buffer_use_before[32];

		struct tm tv;
		localtime_r(&media->first_used, &tv);
		strftime(buffer_first_used, 32, "%F %T", &tv);

		localtime_r(&media->use_before, &tv);
		strftime(buffer_use_before, 32, "%F %T", &tv);

		char * load, * read, * write, * endpos, * nbfiles, * blocksize, * pool = 0;
		asprintf(&load, "%ld", media->load_count);
		asprintf(&read, "%ld", media->read_count);
		asprintf(&write, "%ld", media->write_count);
		asprintf(&endpos, "%zd", media->end_position);
		asprintf(&nbfiles, "%u", media->nb_volumes);
		asprintf(&blocksize, "%zd", media->block_size);
		//if (media->pool)
		//	asprintf(&pool, "%ld", media->pool->id);

		const char * param[] = {
			*media->uuid ? media->uuid : 0,
			media->label, media->medium_serial_number, media->name,
			st_media_status_to_string(media->status),
			st_media_location_to_string(media->location),
			buffer_first_used, buffer_use_before,
			load, read, write, endpos, nbfiles, blocksize,
			mediaformatid, pool
		};
		PGresult * result = PQexecPrepared(self->connect, query, 16, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &media_data->id);

		PQclear(result);
		free(load);
		free(read);
		free(write);
		free(endpos);
		free(nbfiles);
		free(blocksize);
		free(mediaformatid);
		if (pool)
			free(pool);

		return status == PGRES_FATAL_ERROR;
	} else {
		const char * query = "update_tape";
		st_db_postgresql_prepare(self, query, "UPDATE tape SET uuid = $1, name = $2, status = $3, location = $4, loadcount = $5, readcount = $6, writecount = $7, endpos = $8, nbfiles = $9, blocksize = $10, pool = $11 WHERE id = $12");

		char * load, * read, * write, * endpos, * nbfiles, * blocksize, * pool = 0;
		asprintf(&load, "%ld", media->load_count);
		asprintf(&read, "%ld", media->read_count);
		asprintf(&write, "%ld", media->write_count);
		asprintf(&endpos, "%zd", media->end_position);
		asprintf(&nbfiles, "%u", media->nb_volumes);
		asprintf(&blocksize, "%zd", media->block_size);
		//if (tape->pool)
		//	asprintf(&pool, "%ld", tape->pool->id);

		const char * param[] = {
			*media->uuid ? media->uuid : 0, media->name, st_media_status_to_string(media->status),
			st_media_location_to_string(media->location),
			load, read, write, endpos, nbfiles, blocksize, pool, mediaid
		};
		PGresult * result = PQexecPrepared(self->connect, query, 12, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);

		PQclear(result);
		free(load);
		free(read);
		free(write);
		free(endpos);
		free(nbfiles);
		free(blocksize);
		free(mediaid);
		if (pool)
			free(pool);

		return status == PGRES_FATAL_ERROR;
	}

	return 0;
}

int st_db_postgresql_sync_slot(struct st_database_connection * connect, struct st_slot * slot) {
	struct st_db_postgresql_connection_private * self = connect->data;

	if (slot->media && st_db_postgresql_sync_media(connect, slot->media))
		return 1;

	struct st_db_postgresql_changer_data * changer_data = slot->changer->db_data;

	struct st_db_postgresql_drive_data * drive_data = 0;
	if (slot->drive)
		drive_data = slot->drive->db_data;

	struct st_db_postgresql_slot_data * slot_data = slot->db_data;
	if (!slot_data) {
		slot->db_data = slot_data = malloc(sizeof(struct st_db_postgresql_slot_data));
		slot_data->id = -1;
	}

	struct st_db_postgresql_media_data * media_data = 0;
	if (slot->media)
		media_data = slot->media->db_data;

	char * changer_id = 0, * drive_id = 0, * media_id = 0, * slot_id = 0;
	asprintf(&changer_id, "%ld", changer_data->id);
	if (drive_data)
		asprintf(&drive_id, "%ld", drive_data->id);
	if (slot_data->id >= 0)
		asprintf(&slot_id, "%ld", slot_data->id);

	if (slot_data->id >= 0) {
		const char * query = "select_slot_by_id";
		st_db_postgresql_prepare(self, query, "SELECT tape FROM changerslot WHERE id = $1 FOR UPDATE NOWAIT");

		const char * param[] = { slot_id };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1 && !PQgetisnull(result, 0, 0))
			st_db_postgresql_get_string_dup(result, 0, 0, &media_id);

		PQclear(result);
		free(changer_id);
		free(drive_id);
		free(slot_id);

		if (status == PGRES_FATAL_ERROR)
			return 2;
	} else {
		const char * query = "select_slot_by_index_changer";
		st_db_postgresql_prepare(self, query, "SELECT id, tape FROM changerslot WHERE index = $1 AND changer = $2 FOR UPDATE NOWAIT");

		char * slot_index = 0;
		asprintf(&slot_index, "%td", slot - slot->changer->slots);

		const char * param[] = { slot_index, changer_id };
		PGresult * result = PQexecPrepared(self->connect, query, 2, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_long(result, 0, 0, &slot_data->id);
			st_db_postgresql_get_string_dup(result, 0, 0, &slot_id);
			if (!PQgetisnull(result, 0, 1))
				st_db_postgresql_get_string_dup(result, 0, 1, &media_id);
		}

		PQclear(result);
		free(slot_index);
		free(changer_id);

		if (status == PGRES_FATAL_ERROR)
			return 2;
	}

	if (media_id) {
		long mediaid;
		sscanf(media_id, "%ld", &mediaid);

		if (!media_data || mediaid != media_data->id) {
			const char * query = "update_media_offline";
			st_db_postgresql_prepare(self, query, "UPDATE tape SET location = 'offline' WHERE id = $1");

			const char * param[] = { media_id };
			PGresult * result = PQexecPrepared(self->connect, query, 1, param, 0, 0, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result, query);

			PQclear(result);

			free(media_id);
			media_id = 0;
		}
	}

	if (media_data && media_data->id >= 0 && !media_id)
		asprintf(&media_id, "%ld", media_data->id);

	if (slot_data->id >= 0) {
		const char * query = "update_slot";
		st_db_postgresql_prepare(self, query, "UPDATE changerslot SET tape = $1 WHERE id = $2");

		const char * param[] = { media_id, slot_id };
		PGresult * result = PQexecPrepared(self->connect, query, 2, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);

		PQclear(result);
		free(slot_id);
		free(media_id);

		return status == PGRES_FATAL_ERROR;
	} else {
		const char * query = "insert_slot";
		st_db_postgresql_prepare(self, query, "INSERT INTO changerslot(index, changer, tape, type) VALUES ($1, $2, $3, $4) RETURNING id");

		char * changer_id = 0, * slot_index = 0;
		asprintf(&slot_index, "%td", slot - slot->changer->slots);
		char * type;
		switch (slot->media->type) {
			case st_slot_type_drive:
				type = "drive";
				break;

			case st_slot_type_import_export:
				type = "import / export";
				break;

			default:
				type = "default";
				break;
		}

		const char * param[] = { slot_index, changer_id, media_id, type };
		PGresult * result = PQexecPrepared(self->connect, query, 4, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &slot_data->id);

		PQclear(result);

		free(media_id);
		free(slot_index);
		free(changer_id);
		free(media_id);

		return status == PGRES_FATAL_ERROR;
	}

	return 0;
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

