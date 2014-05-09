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
#include <libstone/time.h>
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

static int st_database_postgresql_sync_changer(struct st_database_connection * connect, struct st_value * changer, bool init);
static int st_database_postgresql_sync_drive(struct st_database_connection * connect, struct st_value * drive, bool init);
static int st_database_postgresql_sync_slots(struct st_database_connection * connect, struct st_value * slot, bool init);

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

	.sync_changer = st_database_postgresql_sync_changer,
	.sync_drive   = st_database_postgresql_sync_drive,
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


static int st_database_postgresql_sync_changer(struct st_database_connection * connect, struct st_value * changer, bool init) {
	if (connect == NULL || changer == NULL)
		return -1;

	struct st_database_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType transStatus = PQtransactionStatus(self->connect);
	if (transStatus == PQTRANS_INERROR)
		return 1;

	char * hostid = st_database_postgresql_get_host(connect);
	if (hostid == NULL)
		return 2;

	if (transStatus == PQTRANS_IDLE)
		st_database_postgresql_start_transaction(connect);

	struct st_value * db = NULL;
	st_value_unpack(changer, "{so}", "db", &db);

	char * changer_id = NULL;
	st_value_unpack(db, "{ss}", "id", &changer_id);

	bool enabled = true;
	if (changer_id != NULL) {
		const char * query = "select_changer_by_id";
		st_database_postgresql_prepare(self, query, "SELECT enable FROM changer WHERE id = $1 FOR UPDATE");

		const char * param[] = { changer_id };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);
		int nb_result = PQntuples(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && nb_result == 1)
			st_database_postgresql_get_bool(result, 0, 0, &enabled);
		else if (status == PGRES_TUPLES_OK && nb_result == 0)
			enabled = false;

		PQclear(result);

		if (status == PGRES_FATAL_ERROR || nb_result == 0) {
			free(hostid);
			free(changer_id);

			if (transStatus == PQTRANS_IDLE)
				st_database_postgresql_cancel_transaction(connect);
			return 3;
		}
	} else {
		const char * query;
		PGresult * result;

		char * model = NULL, * vendor = NULL, * serial_number = NULL, * wwn = NULL;
		st_value_unpack(changer, "{ssssssss}", "model", &model, "vendor", &vendor, "serial number", &serial_number, "wwn", &wwn);

		if (wwn != NULL) {
			query = "select_changer_by_model_vendor_serialnumber_wwn";
			st_database_postgresql_prepare(self, query, "SELECT id, enable FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 AND wwn = $4 FOR UPDATE");

			const char * param[] = { model, vendor, serial_number, wwn };
			result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
		} else {
			query = "select_changer_by_model_vendor_serialnumber";
			st_database_postgresql_prepare(self, query, "SELECT id, enable FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 AND wwn IS NULL FOR UPDATE");

			const char * param[] = { model, vendor, serial_number };
			result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
		}
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_database_postgresql_get_string_dup(result, 0, 0, &changer_id);
			st_database_postgresql_get_bool(result, 0, 1, &enabled);

			st_value_hashtable_put2(changer, "enable", st_value_new_boolean(enabled), true);
			st_value_hashtable_put2(db, "id", st_value_new_string(changer_id), true);
		}

		PQclear(result);
		free(model);
		free(vendor);
		free(serial_number);
		free(wwn);

		if (status == PGRES_FATAL_ERROR) {
			free(hostid);
			free(changer_id);

			if (transStatus == PQTRANS_IDLE)
				st_database_postgresql_cancel_transaction(connect);
			return 3;
		}
	}

	if (changer_id != NULL) {
		const char * query = "update_changer";
		st_database_postgresql_prepare(self, query, "UPDATE changer SET device = $1, status = $2, firmwarerev = $3 WHERE id = $4");

		char * device = NULL, * sstatus = NULL, * revision = NULL;
		st_value_unpack(changer, "{ssssss}", "device", &device, "status", &sstatus, "revision", &revision);

		const char * params[] = { device, sstatus, revision, changer_id };
		PGresult * result = PQexecPrepared(self->connect, query, 4, params, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		PQclear(result);

		free(device);
		free(revision);
		free(sstatus);

		if (status == PGRES_FATAL_ERROR) {
			st_database_postgresql_get_error(result, query);

			free(hostid);

			if (transStatus == PQTRANS_IDLE)
				st_database_postgresql_cancel_transaction(connect);
			return -2;
		}
	} else if (init) {
		const char * query = "insert_changer";
		st_database_postgresql_prepare(self, query, "INSERT INTO changer(device, status, barcode, model, vendor, firmwarerev, serialnumber, wwn, host) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9) RETURNING id");

		char * device = NULL, * sstatus = NULL, * revision = NULL, * model = NULL, * vendor = NULL, * serial_number = NULL, * wwn = NULL;
		bool barcode = false;

		st_value_unpack(changer, "{sssssbssssssssss}", "device", &device, "status", &sstatus, "barcode", &barcode, "model", &model, "vendor", &vendor, "revision", &revision, "serial number", &serial_number, "wwn", &wwn);

		const char * param[] = {
			device, sstatus, barcode ? "true" : "false", model, vendor, revision, serial_number, wwn, hostid,
		};

		PGresult * result = PQexecPrepared(self->connect, query, 9, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_database_postgresql_get_string_dup(result, 0, 0, &changer_id);
			st_value_hashtable_put2(db, "id", st_value_new_string(changer_id), true);
		}

		PQclear(result);

		free(device);
		free(model);
		free(revision);
		free(serial_number);
		free(sstatus);
		free(vendor);
		free(wwn);

		if (status == PGRES_FATAL_ERROR) {
			free(hostid);
			if (transStatus == PQTRANS_IDLE)
				st_database_postgresql_cancel_transaction(connect);
			return -1;
		}
	} else {
		if (transStatus == PQTRANS_IDLE)
			st_database_postgresql_cancel_transaction(connect);
		return -1;
	}

	free(hostid);

	int failed = 0;

	if (init) {
		struct st_value * drives;
		st_value_unpack(changer, "{so}", "drives", &drives);

		struct st_value_iterator * iter = st_value_list_get_iterator(drives);
		while (failed == 0 && st_value_iterator_has_next(iter)) {
			struct st_value * drive = st_value_iterator_get_value(iter, false);
			failed = st_database_postgresql_sync_drive(connect, drive, init);
		}
		st_value_iterator_free(iter);


		const char * query = "delete_from_changerslot_by_changer";
		st_database_postgresql_prepare(self, query, "DELETE FROM changerslot WHERE changer = $1");

		const char * param[] = { changer_id };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		PQclear(result);
	}

	free(changer_id);

	struct st_value * slots = NULL;
	st_value_unpack(changer, "{so}", "slots", &slots);

	if (failed == 0 && slots != NULL) {
		struct st_value_iterator * iter = st_value_list_get_iterator(slots);
		while (st_value_iterator_has_next(iter) && failed == 0) {
			struct st_value * slot = st_value_iterator_get_value(iter, false);
			failed = st_database_postgresql_sync_slots(connect, slot, init);
		}
		st_value_iterator_free(iter);
	}

	if (transStatus == PQTRANS_IDLE && failed == 0)
		st_database_postgresql_finish_transaction(connect);

	return 0;
}

static int st_database_postgresql_sync_drive(struct st_database_connection * connect, struct st_value * drive, bool init) {
	if (connect == NULL || drive == NULL)
		return -1;

	struct st_database_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType transStatus = PQtransactionStatus(self->connect);
	if (transStatus == PQTRANS_INERROR)
		return 2;
	else if (transStatus == PQTRANS_IDLE)
		st_database_postgresql_start_transaction(connect);

	struct st_value * db;
	char * drive_id = NULL;
	st_value_unpack(drive, "{so}", "db", &db);
	st_value_unpack(db, "{ss}", "db", "id", &drive_id);

	char * driveformat_id = NULL;

	if (drive_id != NULL) {
		const char * query = "select_drive_by_id";
		st_database_postgresql_prepare(self, query, "SELECT driveformat, enable FROM drive WHERE id = $1 FOR UPDATE");

		const char * param[] = { drive_id };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);
		int nb_result = PQntuples(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && nb_result == 1) {
			if (!PQgetisnull(result, 0, 0)) {
				st_database_postgresql_get_string_dup(result, 0, 0, &driveformat_id);
				st_value_hashtable_put2(db, "driveformat_id", st_value_new_string(driveformat_id), true);
			}

			bool enabled;
			st_database_postgresql_get_bool(result, 0, 1, &enabled);
			st_value_hashtable_put2(drive, "enable", st_value_new_boolean(enabled), true);
		} else if (status == PGRES_TUPLES_OK && nb_result == 1)
			st_value_hashtable_put2(drive, "enable", st_value_new_boolean(false), true);

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			free(drive_id);
			if (transStatus == PQTRANS_IDLE)
				st_database_postgresql_cancel_transaction(connect);
			return 3;
		}
	} else {
		const char * query = "select_drive_by_model_vendor_serialnumber";
		st_database_postgresql_prepare(self, query, "SELECT id, operationduration, lastclean, driveformat, enable FROM drive WHERE model = $1 AND vendor = $2 AND serialnumber = $3 FOR UPDATE");

		char * model = NULL, * vendor = NULL, * serial_number = NULL;
		st_value_unpack(drive, "{ssssss}", "model", &model, "vendor", &vendor, "serial number", &serial_number);

		const char * param[] = { model, vendor, serial_number };
		PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_database_postgresql_get_string_dup(result, 0, 0, &drive_id);

			double old_operation_duration = 0;
			st_database_postgresql_get_double(result, 0, 1, &old_operation_duration);
			double current_operation_duration = 0;
			st_value_unpack(drive, "{sf}", "operation duration", &current_operation_duration);
			st_value_hashtable_put2(drive, "operation duration", st_value_new_float(old_operation_duration + current_operation_duration), true);

			time_t last_clean = 0;
			if (!PQgetisnull(result, 0, 2)) {
				st_database_postgresql_get_time(result, 0, 2, &last_clean);
				st_value_hashtable_put2(drive, "last clean", st_value_new_integer(last_clean), true);
			}

			if (!PQgetisnull(result, 0, 3)) {
				st_database_postgresql_get_string_dup(result, 0, 3, &driveformat_id);
				st_value_hashtable_put2(db, "driveformat_id", st_value_new_string(driveformat_id), true);
			}

			bool enabled;
			st_database_postgresql_get_bool(result, 0, 4, &enabled);
			st_value_hashtable_put2(drive, "enable", st_value_new_boolean(enabled), true);
		}

		PQclear(result);
		free(model);
		free(vendor);
		free(serial_number);

		if (status == PGRES_FATAL_ERROR) {
			if (transStatus == PQTRANS_IDLE)
				st_database_postgresql_cancel_transaction(connect);
			return 3;
		}
	}

	if (driveformat_id != NULL) {
		const char * query = "select_driveformat_by_id";
		st_database_postgresql_prepare(self, query, "SELECT densitycode FROM driveformat WHERE id = $1 LIMIT 1");

		const char * param[] = { driveformat_id };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		unsigned char density_code = 0;

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_database_postgresql_get_uchar(result, 0, 0, &density_code);

		PQclear(result);

		long long current_density_code = 0;
		st_value_unpack(drive, "{si}", "density code", &current_density_code);

		if (current_density_code > density_code) {
			free(driveformat_id);
			driveformat_id = NULL;
		}
	}

	if (driveformat_id == NULL) {
		const char * query = "select_driveformat_by_densitycode";
		st_database_postgresql_prepare(self, query, "SELECT id FROM driveformat WHERE densitycode = $1 AND mode = $2 LIMIT 1");

		long long current_density_code = 0;
		char * drive_mode = NULL;
		st_value_unpack(drive, "{siss}", "density code", &current_density_code, "mode", &drive_mode);

		char * sdensity_code = NULL;
		asprintf(&sdensity_code, "%lld", current_density_code);

		const char * param[] = { sdensity_code, drive_mode };
		PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_database_postgresql_get_string_dup(result, 0, 0, &driveformat_id);
			st_value_hashtable_put2(db, "driveformat_id", st_value_new_string(driveformat_id), true);
		}

		PQclear(result);
		free(sdensity_code);
		free(drive_mode);
	}

	if (drive_id != NULL) {
		const char * query = "update_drive";
		st_database_postgresql_prepare(self, query, "UPDATE drive SET device = $1, scsidevice = $2, status = $3, operationduration = $4, lastclean = $5, firmwarerev = $6, driveformat = $7 WHERE id = $8");

		double drive_operation_duration = 0;
		long long drive_last_clean = 0;
		char * drive_device = NULL, * drive_scsi_device = NULL, * drive_status = NULL, * drive_revision = NULL;
		st_value_unpack(drive, "{sfsissssssss}", "operation duration", &drive_operation_duration, "last clean", &drive_last_clean, "device", &drive_device, "scsi device", &drive_scsi_device, "status", &drive_status, "revision", &drive_revision);

		char * op_duration = NULL, * last_clean = NULL;
		asprintf(&op_duration, "%.3f", drive_operation_duration);

		if (drive_last_clean > 0) {
			time_t drive_last_clean2 = drive_last_clean;
			last_clean = malloc(24);
			st_time_convert(&drive_last_clean2, "%F %T", last_clean, 24);
		}

		const char * param[] = {
			drive_device, drive_scsi_device, drive_status, op_duration, last_clean,
			drive_revision, driveformat_id, drive_id
		};
		PGresult * result = PQexecPrepared(self->connect, query, 8, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);

		PQclear(result);
		free(last_clean);
		free(op_duration);
		free(drive_device);
		free(drive_scsi_device);
		free(drive_status);
		free(drive_revision);

		if (status == PGRES_FATAL_ERROR) {
			free(drive_id);

			if (transStatus == PQTRANS_IDLE)
				st_database_postgresql_cancel_transaction(connect);
			return 3;
		}
	} else if (init) {
		const char * query = "insert_drive";
		st_database_postgresql_prepare(self, query, "INSERT INTO drive(device, scsidevice, status, operationduration, lastclean, model, vendor, firmwarerev, serialnumber, changer, driveformat) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11) RETURNING id");

		double drive_operation_duration = 0;
		long long drive_last_clean = 0;
		char * drive_device = NULL, * drive_scsi_device = NULL, * drive_status = NULL, * drive_revision = NULL, * drive_model = NULL, * drive_vendor = NULL, * drive_serial_number = NULL;
		st_value_unpack(drive, "{sfsissssssssssssss}", "operation duration", &drive_operation_duration, "last clean", &drive_last_clean, "device", &drive_device, "scsi device", &drive_scsi_device, "revision", &drive_revision, "status", &drive_status, "model", &drive_model, "vendor", &drive_vendor, "serial number", &drive_serial_number);

		char * op_duration = NULL, * last_clean = NULL;
		asprintf(&op_duration, "%.3f", drive_operation_duration);

		char * changer_id = NULL;
		st_value_unpack(drive, "{s{s{ss}}}", "changer", "db", "id", &changer_id);

		if (drive_last_clean > 0) {
			time_t drive_last_clean2 = drive_last_clean;
			last_clean = malloc(24);
			st_time_convert(&drive_last_clean2, "%F %T", last_clean, 24);
		}

		const char * param[] = {
			drive_device, drive_scsi_device, drive_status, op_duration, last_clean,
			drive_model, drive_vendor, drive_revision, drive_serial_number, changer_id, driveformat_id
		};
		PGresult * result = PQexecPrepared(self->connect, query, 11, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_database_postgresql_get_string_dup(result, 0, 0, &drive_id);
			st_value_hashtable_put2(db, "id", st_value_new_string(drive_id), true);
		}

		PQclear(result);
		free(changer_id);
		free(last_clean);
		free(op_duration);
		free(drive_device);
		free(drive_scsi_device);
		free(drive_status);
		free(drive_revision);
		free(drive_model);
		free(drive_vendor);
		free(drive_serial_number);

		if (status == PGRES_FATAL_ERROR) {
			free(drive_id);

			if (transStatus == PQTRANS_IDLE)
				st_database_postgresql_cancel_transaction(connect);
			return 3;
		}
	} else {
	}

	free(drive_id);

	if (transStatus == PQTRANS_IDLE)
		st_database_postgresql_finish_transaction(connect);

	return 0;
}

static int st_database_postgresql_sync_slots(struct st_database_connection * connect, struct st_value * slot, bool init) {
	struct st_database_postgresql_connection_private * self = connect->data;

	struct st_value * db = NULL, * changer = NULL, * drive = NULL;
	long long int index = -1;
	bool isImportExport = false;
	bool enabled = true;
	st_value_unpack(slot, "{sosisbsosbso}", "db", &db, "index", &index, "enable", &enabled, "import export", &isImportExport, "changer", &changer, "drive", &drive);

	char * slot_id = NULL;
	st_value_unpack(db, "{ss}", "id", &slot_id);

	char * type = "storage";
	if (drive != NULL)
		type = "drive";
	else if (isImportExport)
		type = "import / export";

	char * changer_id = NULL, * drive_id = NULL;
	st_value_unpack(changer, "{s{ss}}", "db", "id", &changer_id);
	if (drive != NULL)
		st_value_unpack(drive, "{s{ss}}", "db", "id", &drive_id);

	int failed = 0;

	if (!init && slot_id == NULL) {
		const char * query = "select_changerslot_by_index";
		st_database_postgresql_prepare(self, query, "SELECT id, enable FROM changerslot WHERE changer = $1 AND index = $2 LIMIT 1");

		char * sindex;
		asprintf(&sindex, "%lld", index);

		const char * param[] = { changer_id, sindex };

		PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_database_postgresql_get_string_dup(result, 0, 0, &slot_id);
			st_value_hashtable_put2(db, "id", st_value_new_string(slot_id), true);

			st_database_postgresql_get_bool(result, 0, 1, &enabled);
			st_value_hashtable_put2(slot, "enable", st_value_new_boolean(enabled), true);
		}

		PQclear(result);
		free(sindex);
	}

	if (init) {
		const char * query = "insert_into_changerslot";
		st_database_postgresql_prepare(self, query, "INSERT INTO changerslot(index, changer, drive, type) VALUES ($1, $2, $3, $4, $5) RETURNING id");

		char * sindex;
		asprintf(&sindex, "%lld", index);

		const char * param[] = { sindex, changer_id, drive_id, type };

		PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_value_hashtable_put2(db, "id", st_value_new_string(PQgetvalue(result, 0, 0)), true);

		PQclear(result);
		free(sindex);
	} else {
		const char * query = "update_changerslot";
		st_database_postgresql_prepare(self, query, "UDPATE changerslot SET media = $1, enable = $2 WHERE id = $3");

		const char * param[] = { NULL, enabled ? "true" : "false", slot_id };

		PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);

		PQclear(result);
	}

	return failed;
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

