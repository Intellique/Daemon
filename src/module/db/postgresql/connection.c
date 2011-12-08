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
*  Last modified: Thu, 08 Dec 2011 17:22:23 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
// free, malloc
#include <malloc.h>
// PQbackendPID, PQclear, PQerrorMessage, PQexec, PQfinish, PQfnumber, PQgetvalue, PQntuples, PQsetdbLogin, PQresultStatus, PQstatus
#include <postgresql/libpq-fe.h>
// uname
#include <sys/utsname.h>
// sscanf, snprintf
#include <stdio.h>
// strcmp, strdup
#include <string.h>
// strptime
#include <time.h>
// sleep
#include <unistd.h>

#include <storiqArchiver/library/changer.h>
#include <storiqArchiver/library/drive.h>
#include <storiqArchiver/library/tape.h>
#include <storiqArchiver/log.h>

#include "common.h"

static void sa_db_postgresql_check(struct sa_database_connection * connection);
static int sa_db_postgresql_con_close(struct sa_database_connection * connection);
static int sa_db_postgresql_con_free(struct sa_database_connection * connection);

static int sa_db_postgresql_cancel_transaction(struct sa_database_connection * connection);
static int sa_db_postgresql_finish_transaction(struct sa_database_connection * connection);
static int sa_db_postgresql_start_transaction(struct sa_database_connection * connection, short read_only);

static int sa_db_postgresql_get_tape_format(struct sa_database_connection * db, struct sa_tape_format * tape_format, unsigned char density_code);
static int sa_db_postgresql_sync_changer(struct sa_database_connection * db, struct sa_changer * changer);
static int sa_db_postgresql_sync_drive(struct sa_database_connection * db, struct sa_drive * drive);
static int sa_db_postgresql_sync_slot(struct sa_database_connection * db, struct sa_slot * slot);
static int sa_db_postgresql_sync_tape(struct sa_database_connection * db, struct sa_tape * tape);

static int sa_db_postgresql_get_bool(PGresult * result, int row, int column, char * value);
static int sa_db_postgresql_get_double(PGresult * result, int row, int column, double * value);
static void sa_db_postgresql_get_error(PGresult * result);
//static int sa_db_postgresql_get_int(PGresult * result, int row, int column, int * value);
static int sa_db_postgresql_get_long(PGresult * result, int row, int column, long * value);
static int sa_db_postgresql_get_string(PGresult * result, int row, int column, char * value);
static int sa_db_postgresql_get_string_dup(PGresult * result, int row, int column, char ** value);
static int sa_db_postgresql_get_time(PGresult * result, int row, int column, time_t * value);
static void sa_db_postgresql_prepare(PGconn * connection, const char * statement_name, const char * query);

static struct sa_database_connection_ops sa_db_postgresql_con_ops = {
	.close = sa_db_postgresql_con_close,
	.free  = sa_db_postgresql_con_free,

	.cancel_transaction = sa_db_postgresql_cancel_transaction,
	.finish_transaction = sa_db_postgresql_finish_transaction,
	.start_transaction  = sa_db_postgresql_start_transaction,

	.get_tape_format = sa_db_postgresql_get_tape_format,
	.sync_changer    = sa_db_postgresql_sync_changer,
	.sync_drive      = sa_db_postgresql_sync_drive,
	.sync_tape       = sa_db_postgresql_sync_tape,
};


int sa_db_postgresql_cancel_transaction(struct sa_database_connection * connection) {
	if (!connection)
		return -1;

	struct sa_db_postgresql_connetion_private * self = connection->data;
	sa_db_postgresql_check(connection);

	PGresult * result = PQexec(self->db_con, "ROLLBACK;");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		sa_log_write_all(sa_log_level_error, "Db: Postgresql (cid #%ld): error while cancelling a transaction => %s", connection->id, PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

void sa_db_postgresql_check(struct sa_database_connection * connection) {
	struct sa_db_postgresql_connetion_private * self = connection->data;
	if (PQstatus(self->db_con) != CONNECTION_OK) {
		int i;
		for (i = 0; i < 3; i++) {
			PQreset(self->db_con);
			if (PQstatus(self->db_con) != CONNECTION_OK) {
				sa_log_write_all(sa_log_level_error, "Db: Postgresql (cid #%ld): Failed to reset database connection", connection->id);
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
	struct sa_db_postgresql_connetion_private * self = connection->data;
	sa_db_postgresql_check(connection);

	sa_db_postgresql_prepare(self->db_con, "select_tapeformat_by_densitycode", "SELECT * FROM tapeformat WHERE densityCode = $1 LIMIT 1");

	char * densitycode = 0;
	asprintf(&densitycode, "%d", density_code);
	const char * params[] = { densitycode };
	PGresult * result = PQexecPrepared(self->db_con, "select_tapeformat_by_densitycode", 1, params, 0, 0, 0);

	free(densitycode);

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
	sa_db_postgresql_check(connection);

	PGresult * result = PQexec(self->db_con, "COMMIT;");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		sa_log_write_all(sa_log_level_error, "Db: Postgresql (cid #%ld): error while committing a transaction (%s)", connection->id, PQerrorMessage(self->db_con));

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
	sa_db_postgresql_check(connection);

	PGresult * result = PQexec(self->db_con, readOnly ? "BEGIN READ ONLY;" : "BEGIN;");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		sa_log_write_all(sa_log_level_error, "Db: Postgresql (cid #%ld): error while starting a transaction (%s)", connection->id, PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

int sa_db_postgresql_sync_changer(struct sa_database_connection * connection, struct sa_changer * changer) {
	struct sa_db_postgresql_connetion_private * self = connection->data;
	sa_db_postgresql_check(connection);

	struct utsname name;
	uname(&name);

	sa_db_postgresql_prepare(self->db_con, "select_host_by_name", "SELECT id FROM host WHERE name = $1 LIMIT 1");

	const char * params1[] = { name.nodename };
	PGresult * result = PQexecPrepared(self->db_con, "select_host_by_name", 1, params1, 0, 0, 0);

	char * hostid = 0, * changerid = 0;
	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		sa_db_postgresql_get_string_dup(result, 0, 0, &hostid);
		PQclear(result);
	} else {
		PQclear(result);
		sa_log_write_all(sa_log_level_error, "Db: Postgresql (cid #%ld): Host not found into database (%s)", connection->id, name.nodename);
		return -1;
	}

	if (changer->id < 0) {
		sa_db_postgresql_prepare(self->db_con, "select_changer_by_model_vendor_host_drive", "SELECT c.id FROM changer c, drive d WHERE c.id = d.changer AND c.model = $1 AND c.vendor = $2 AND c.host = $3 AND d.serialnumber = $4 LIMIT 1");

		const char * params2[] = { changer->model, changer->vendor, hostid, changer->drives[0].serial_number };
		result = PQexecPrepared(self->db_con, "select_changer_by_model_vendor_host_drive", 4, params2, 0, 0, 0);

		if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			sa_db_postgresql_get_long(result, 0, 0, &changer->id);
			sa_db_postgresql_get_string_dup(result, 0, 0, &changerid);
		}

		PQclear(result);
	}

	if (changer->id < 0) {
		sa_db_postgresql_prepare(self->db_con, "insert_changer", "INSERT INTO changer VALUES (DEFAULT, $1, $2, $3, $4, $5, $6, $7, NOW(), $8)");

		const char * params2[] = {
			changer->device, sa_changer_status_to_string(changer->status), changer->barcode ? "true" : "false", "false",
			changer->model, changer->vendor, changer->revision, hostid
		};

		result = PQexecPrepared(self->db_con, "insert_changer", 8, params2, 0, 0, 0);
		if (PQresultStatus(result) == PGRES_FATAL_ERROR) {
			sa_db_postgresql_get_error(result);
			free(result);
			free(hostid);
			return -1;
		}
		PQclear(result);

		sa_db_postgresql_prepare(self->db_con, "select_changer_by_model_vendor_host", "SELECT id FROM changer WHERE model = $1 AND vendor = $2 AND host = $3 LIMIT 1");

		const char * params3[] = { changer->model, changer->vendor, hostid };
		result = PQexecPrepared(self->db_con, "select_changer_by_model_vendor_host", 3, params3, 0, 0, 0);

		if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
			sa_db_postgresql_get_long(result, 0, 0, &changer->id);

		if (changer->id < 0)
			sa_log_write_all(sa_log_level_error, "Db: Postgresql (cid #%ld): An expected probleme occured: failed to retreive changer id after insert it", connection->id);

		PQclear(result);
	} else {
		sa_db_postgresql_prepare(self->db_con, "update_changer", "UPDATE changer SET device = $1, status = $2, firmwarerev = $3, update = NOW() WHERE id = $4");

		const char * params2[] = { changer->device, sa_changer_status_to_string(changer->status), changer->revision, changerid };
		result = PQexecPrepared(self->db_con, "update_changer", 4, params2, 0, 0, 0);

		if (PQresultStatus(result) == PGRES_FATAL_ERROR) {
			sa_db_postgresql_get_error(result);
			free(changerid);
			free(hostid);
			PQclear(result);
			return -2;
		}

		free(changerid);
		PQclear(result);
	}
	free(hostid);

	unsigned int i;
	for (i = 0; i < changer->nb_drives; i++)
		sa_db_postgresql_sync_drive(connection, changer->drives + i);

	for (i = changer->nb_drives; i < changer->nb_slots; i++)
		sa_db_postgresql_sync_slot(connection, changer->slots + i);

	return 0;
}

int sa_db_postgresql_sync_drive(struct sa_database_connection * connection, struct sa_drive * drive) {
	struct sa_db_postgresql_connetion_private * self = connection->data;
	sa_db_postgresql_check(connection);

	char * changerid = 0;
	asprintf(&changerid, "%ld", drive->changer->id);

	if (drive->id < 0) {
		sa_db_postgresql_prepare(self->db_con, "select_drive_by_changer", "SELECT id, operationduration, lastclean FROM drive WHERE serialnumber = $1 AND changer = $2 LIMIT 1");

		const char * params[] = { drive->serial_number, changerid };
		PGresult * result = PQexecPrepared(self->db_con, "select_drive_by_changer", 2, params, 0, 0, 0);

		if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			sa_db_postgresql_get_long(result, 0, 0, &drive->id);

			double old_operation_duration;
			sa_db_postgresql_get_double(result, 0, 1, &old_operation_duration);
			drive->operation_duration += old_operation_duration;

			sa_db_postgresql_get_time(result, 0, 2, &drive->last_clean);
		}

		PQclear(result);
	}

	if (drive->id < 0 && drive->density_code < 1) {
		free(changerid);
		sa_log_write_all(sa_log_level_error, "Db: Postgresql (cid #%ld): Unable to complete update database without any tape", connection->id);
		return 1;
	} else if (drive->id < 0) {
		sa_db_postgresql_prepare(self->db_con, "select_driveformat_by_densitycode", "SELECT id FROM driveformat WHERE densitycode = $1 LIMIT 1");

		char * densitycode = 0;
		asprintf(&densitycode, "%u", drive->density_code);

		const char * params1[] = { densitycode };
		PGresult * result = PQexecPrepared(self->db_con, "select_driveformat_by_densitycode", 1, params1, 0, 0, 0);

		char * driveformat_id = 0;
		ExecStatusType status = PQresultStatus(result);
		if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			sa_db_postgresql_get_string_dup(result, 0, 0, &driveformat_id);
			PQclear(result);
		} else {
			if (status == PGRES_FATAL_ERROR)
				sa_db_postgresql_get_error(result);

			PQclear(result);
			free(densitycode);
			free(changerid);

			sa_log_write_all(sa_log_level_error, "Db: Postgresql (cid #%ld): Unable to complete update database without any tape", connection->id);
			return 1;
		}

		sa_db_postgresql_prepare(self->db_con, "insert_drive", "INSERT INTO drive VALUES (DEFAULT, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)");

		char * changer_num = 0, * op_duration = 0;
		asprintf(&changer_num, "%ld", drive - drive->changer->drives);
		asprintf(&op_duration, "%.3f", drive->operation_duration);

		char last_clean[24];
		struct tm tm_last_clean;
		localtime_r(&drive->last_clean, &tm_last_clean);
		strftime(last_clean, 23, "%F %T", &tm_last_clean);

		const char * params2[] = {
			drive->device, sa_drive_status_to_string(drive->status), changer_num, op_duration, last_clean,
			drive->model, drive->vendor, drive->revision, drive->serial_number, changerid, driveformat_id
		};
		result = PQexecPrepared(self->db_con, "insert_drive", 11, params2, 0, 0, 0);

		if (PQresultStatus(result) == PGRES_FATAL_ERROR)
			sa_db_postgresql_get_error(result);

		PQclear(result);
		free(op_duration);
		free(changer_num);
		free(driveformat_id);
		free(densitycode);
	} else {
		sa_db_postgresql_prepare(self->db_con, "update_drive", "UPDATE drive SET device = $1, status = $2, operationduration = $3, lastclean = $4, firmwarerev = $5 WHERE id = $6");

		char * driveid = 0, * op_duration = 0;
		asprintf(&driveid, "%ld", drive->id);
		asprintf(&op_duration, "%.3f", drive->operation_duration);

		char last_clean[24];
		struct tm tm_last_clean;
		localtime_r(&drive->last_clean, &tm_last_clean);
		strftime(last_clean, 23, "%F %T", &tm_last_clean);

		const char * params[] = {
			drive->device, sa_drive_status_to_string(drive->status), op_duration, last_clean, drive->revision, driveid
		};
		PGresult * result = PQexecPrepared(self->db_con, "update_drive", 6, params, 0, 0, 0);

		if (PQresultStatus(result) == PGRES_FATAL_ERROR)
			sa_db_postgresql_get_error(result);

		PQclear(result);
		free(op_duration);
		free(driveid);
	}

	free(changerid);

	sa_db_postgresql_sync_slot(connection, drive->slot);

	return 0;
}

int sa_db_postgresql_sync_slot(struct sa_database_connection * connection, struct sa_slot * slot) {
	struct sa_db_postgresql_connetion_private * self = connection->data;
	sa_db_postgresql_check(connection);

	if (slot->tape && sa_db_postgresql_sync_tape(connection, slot->tape))
		return 1;

	if (slot->id < 0) {
		char * changer_id = 0, * slot_index = 0;
		asprintf(&changer_id, "%ld", slot->changer->id);
		asprintf(&slot_index, "%ld", slot - slot->changer->slots);

		sa_db_postgresql_prepare(self->db_con, "select_slot_by_index_changer", "SELECT id FROM changerslot WHERE index = $1 AND changer = $2 LIMIT 1");

		const char * params[] = { slot_index, changer_id };
		PGresult * result = PQexecPrepared(self->db_con, "select_slot_by_index_changer", 2, params, 0, 0, 0);

		if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
			sa_db_postgresql_get_long(result, 0, 0, &slot->id);

		PQclear(result);
		free(slot_index);
		free(changer_id);
	}

	if (slot->id > -1) {
		char * slot_id = 0, * tape_id = 0;
		asprintf(&slot_id, "%ld", slot->id);
		if (slot->tape)
			asprintf(&tape_id, "%ld", slot->tape->id);

		sa_db_postgresql_prepare(self->db_con, "update_slot", "UPDATE changerslot SET tape = $1 WHERE id = $2");

		const char * params[] = { tape_id, slot_id };
		PGresult * result = PQexecPrepared(self->db_con, "update_slot", 2, params, 0, 0, 0);

		ExecStatusType status = PQresultStatus(result);
		if (status == PGRES_FATAL_ERROR)
			sa_db_postgresql_get_error(result);

		PQclear(result);
		free(slot_id);
		if (slot->tape)
			free(tape_id);

		return status != PGRES_COMMAND_OK;
	} else {
		char * changer_id = 0, * slot_index = 0, * tape_id = 0;
		asprintf(&changer_id, "%ld", slot->changer->id);
		asprintf(&slot_index, "%ld", slot - slot->changer->slots);
		if (slot->tape)
			asprintf(&tape_id, "%ld", slot->tape->id);

		sa_db_postgresql_prepare(self->db_con, "insert_slot", "INSERT INTO changerslot VALUES (DEFAULT, $1, $2, $3)");

		const char * params[] = { slot_index, changer_id, tape_id };
		PGresult * result = PQexecPrepared(self->db_con, "insert_slot", 3, params, 0, 0, 0);

		ExecStatusType status = PQresultStatus(result);
		if (status == PGRES_FATAL_ERROR)
			sa_db_postgresql_get_error(result);

		PQclear(result);

		if (status == PGRES_COMMAND_OK) {
			const char * params2[] = { changer_id, slot_index };
			result = PQexecPrepared(self->db_con, "select_slot_by_index_changer", 2, params2, 0, 0, 0);
			status = PQresultStatus(result);

			if (status == PGRES_COMMAND_OK)
				sa_db_postgresql_get_long(result, 0, 0, &slot->id);

			PQclear(result);
		}

		if (slot->tape)
			free(tape_id);
		free(slot_index);
		free(changer_id);

		return status != PGRES_COMMAND_OK;
	}
}

int sa_db_postgresql_sync_tape(struct sa_database_connection * connection, struct sa_tape * tape) {
	struct sa_db_postgresql_connetion_private * self = connection->data;
	sa_db_postgresql_check(connection);

	if (tape->id < 0 && tape->label[0] != '\0') {
		sa_db_postgresql_prepare(self->db_con, "select_tape_by_label", "SELECT * FROM tape WHERE label = $1 LIMIT 1");

		const char * params[] = { tape->label };
		PGresult * result = PQexecPrepared(self->db_con, "select_tape_by_label", 1, params, 0, 0, 0);

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

		PQclear(result);
	}

	if (tape->id < 0 && tape->name[0] != '\0') {
		sa_db_postgresql_prepare(self->db_con, "select_tape_by_name", "SELECT * FROM tape WHERE name = $1 LIMIT 1");

		const char * params[] = { tape->name };
		PGresult * result = PQexecPrepared(self->db_con, "select_tape_by_name", 1, params, 0, 0, 0);

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

		PQclear(result);
	}

	if (tape->id > -1) {
		sa_db_postgresql_prepare(self->db_con, "update_tape", "UPDATE tape SET status = $1, location = $2, loadcount = $3, readcount = $4, writecount = $5, endpos = $6, nbfiles = $7, blocksize = $8, haspartition = $9 WHERE id = $10");

		char * load, * read, * write, * endpos, * nbfiles, * blocksize, * id;
		asprintf(&load, "%ld", tape->load_count);
		asprintf(&read, "%ld", tape->read_count);
		asprintf(&write, "%ld", tape->write_count);
		asprintf(&endpos, "%ld", tape->end_position);
		asprintf(&nbfiles, "%u", tape->nb_files);
		asprintf(&blocksize, "%zd", tape->block_size);
		asprintf(&id, "%ld", tape->id);

		const char * params[] = {
			sa_tape_status_to_string(tape->status),
			sa_tape_location_to_string(tape->location),
			load, read, write, endpos, nbfiles, blocksize,
			tape->has_partition ? "true" : "false", id
		};
		PGresult * result = PQexecPrepared(self->db_con, "update_tape", 10, params, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			sa_db_postgresql_get_error(result);

		PQclear(result);
		free(load);
		free(read);
		free(write);
		free(endpos);
		free(nbfiles);
		free(blocksize);
		free(id);

		return status != PGRES_COMMAND_OK;
	} else {
		sa_db_postgresql_prepare(self->db_con, "insert_tape", "INSERT INTO tape VALUES (DEFAULT, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15)");

		char buffer_first_used[32];
		char buffer_use_before[32];

		struct tm tv;
		localtime_r(&tape->first_used, &tv);
		strftime(buffer_first_used, 32, "%F %T", &tv);

		localtime_r(&tape->use_before, &tv);
		strftime(buffer_use_before, 32, "%F %T", &tv);

		char * load, * read, * write, * endpos, * nbfiles, * blocksize, * id;
		asprintf(&load, "%ld", tape->load_count);
		asprintf(&read, "%ld", tape->read_count);
		asprintf(&write, "%ld", tape->write_count);
		asprintf(&endpos, "%ld", tape->end_position);
		asprintf(&nbfiles, "%u", tape->nb_files);
		asprintf(&blocksize, "%zd", tape->block_size);
		asprintf(&id, "%ld", tape->format->id);

		const char * params[] = {
			*tape->label ? tape->label : 0, tape->name,
			sa_tape_status_to_string(tape->status),
			sa_tape_location_to_string(tape->location),
			buffer_first_used, buffer_use_before,
			load, read, write, endpos, nbfiles, blocksize,
			tape->has_partition ? "true" : "false", id, 0
		};
		PGresult * result = PQexecPrepared(self->db_con, "insert_tape", 15, params, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			sa_db_postgresql_get_error(result);

		PQclear(result);
		free(load);
		free(read);
		free(write);
		free(endpos);
		free(nbfiles);
		free(blocksize);
		free(id);

		if (status == PGRES_FATAL_ERROR)
			return status != PGRES_COMMAND_OK;

		if (*tape->label) {
			sa_db_postgresql_prepare(self->db_con, "select_tape_id_by_label", "SELECT id FROM tape WHERE label = $1 LIMIT 1");

			const char * params[] = { tape->label };
			PGresult * result = PQexecPrepared(self->db_con, "select_tape_id_by_label", 1, params, 0, 0, 0);

			if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
				sa_db_postgresql_get_long(result, 0, 0, &tape->id);

			PQclear(result);
		} else {
			sa_db_postgresql_prepare(self->db_con, "select_tape_id_by_name", "SELECT id FROM tape WHERE name = $1 LIMIT 1");

			const char * params[] = { tape->name };
			PGresult * result = PQexecPrepared(self->db_con, "select_tape_by_name", 1, params, 0, 0, 0);

			if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
				sa_db_postgresql_get_long(result, 0, 0, &tape->id);

			PQclear(result);
		}

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

int sa_db_postgresql_get_double(PGresult * result, int row, int column, double * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%lg", val) == 1)
		return 0;
	return -1;
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

int sa_db_postgresql_get_string_dup(PGresult * result, int row, int column, char ** val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value) {
		*val = strdup(value);
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

void sa_db_postgresql_prepare(PGconn * connection, const char * statement_name, const char * query) {
	PGresult * prepared = PQdescribePrepared(connection, statement_name);
	if (PQresultStatus(prepared) != PGRES_COMMAND_OK) {
		PGresult * prepare = PQprepare(connection, statement_name, query, 0, 0);
		ExecStatusType status = PQresultStatus(prepare);
		if (status == PGRES_FATAL_ERROR)
			sa_db_postgresql_get_error(prepare);
		PQclear(prepare);

		sa_log_write_all(status == PGRES_COMMAND_OK ? sa_log_level_debug : sa_log_level_error, "Db: Postgresql: new query prepared (%s) => {%s}, status: %s", statement_name, query, PQresStatus(status));
	}
	PQclear(prepared);
}

