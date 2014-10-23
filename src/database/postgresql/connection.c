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
// gettext
#include <libintl.h>
// PQclear, PQexec, PQexecPrepared, PQfinish, PQresultStatus
// PQsetErrorVerbosity, PQtransactionStatus
#include <postgresql/libpq-fe.h>
// asprintf
#include <stdio.h>
// malloc
#include <stdlib.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>
// uname
#include <sys/utsname.h>

#include <libstone/changer.h>
#include <libstone/drive.h>
#include <libstone/host.h>
#include <libstone/job.h>
#include <libstone/json.h>
#include <libstone/log.h>
#include <libstone/media.h>
#include <libstone/script.h>
#include <libstone/slot.h>
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
static int st_database_postgresql_get_host_by_name(struct st_database_connection * connect, struct st_host * host, const char * name);

static struct st_value * st_database_postgresql_get_changers(struct st_database_connection * connect);
static struct st_value * st_database_postgresql_get_drives_by_changer(struct st_database_connection * connect, const char * changer_id);
static struct st_media * st_database_postgresql_get_media(struct st_database_connection * connect, const char * medium_serial_number, const char * label, struct st_job * job) __attribute__((nonnull,warn_unused_result));
static struct st_media_format * st_database_postgresql_get_media_format(struct st_database_connection * connect, unsigned int density_code, enum st_media_format_mode mode);
static struct st_pool * st_database_postgresql_get_pool(struct st_database_connection * connect, const char * uuid, struct st_job * job);
static struct st_value * st_database_postgresql_get_slot_by_drive(struct st_database_connection * connect, const char * drive_id);
static struct st_value * st_database_postgresql_get_standalone_drives(struct st_database_connection * connect);
static struct st_value * st_database_postgresql_get_vtls(struct st_database_connection * connect);
static int st_database_postgresql_sync_changer(struct st_database_connection * connect, struct st_changer * changer, enum st_database_sync_method method);
static int st_database_postgresql_sync_drive(struct st_database_connection * connect, struct st_drive * drive, enum st_database_sync_method method);
static int st_database_postgresql_sync_slots(struct st_database_connection * connect, struct st_slot * slot, enum st_database_sync_method method);
static int st_database_postgresql_sync_media(struct st_database_connection * connect, struct st_media * media, enum st_database_sync_method method);

static int st_database_postgresql_add_job_record(struct st_database_connection * connect, struct st_job * job, enum st_log_level level, enum st_job_record_notif notif, const char * message);
static int st_database_postgresql_start_job(struct st_database_connection * connect, struct st_job * job);
static int st_database_postgresql_stop_job(struct st_database_connection * connect, struct st_job * job);
static int st_database_postgresql_sync_job(struct st_database_connection * connect, struct st_job * job);
static int st_database_postgresql_sync_jobs(struct st_database_connection * connect, struct st_value * jobs);

static int st_database_postgresql_get_nb_scripts(struct st_database_connection * connect, const char * job_type, enum st_script_type type, struct st_pool * pool);
static char * st_database_postgresql_get_script(struct st_database_connection * connect, const char * job_type, unsigned int sequence, enum st_script_type type, struct st_pool * pool);

static int st_database_postgresql_sync_plugin_job(struct st_database_connection * connect, const char * job);

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

	.get_changers          = st_database_postgresql_get_changers,
	.get_media             = st_database_postgresql_get_media,
	.get_media_format      = st_database_postgresql_get_media_format,
	.get_pool              = st_database_postgresql_get_pool,
	.get_standalone_drives = st_database_postgresql_get_standalone_drives,
	.get_vtls              = st_database_postgresql_get_vtls,
	.sync_changer          = st_database_postgresql_sync_changer,
	.sync_drive            = st_database_postgresql_sync_drive,

	.add_job_record = st_database_postgresql_add_job_record,
	.start_job      = st_database_postgresql_start_job,
	.stop_job       = st_database_postgresql_stop_job,
	.sync_job       = st_database_postgresql_sync_job,
	.sync_jobs      = st_database_postgresql_sync_jobs,

	.get_nb_scripts = st_database_postgresql_get_nb_scripts,
	.get_script     = st_database_postgresql_get_script,

	.sync_plugin_job = st_database_postgresql_sync_plugin_job,
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
		st_log_write2(st_log_level_error, st_log_type_plugin_db, gettext("PSQL: error while rolling back a savepoint => %s"), PQerrorMessage(self->connect));

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
			PGresult * result = PQexec(self->connect, "ROLLBACK");
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

static int st_database_postgresql_create_checkpoint(struct st_database_connection * connect, const char * checkpoint) {
	if (connect == NULL || checkpoint == NULL)
		return -1;

	struct st_database_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType transStatus = PQtransactionStatus(self->connect);

	if (transStatus == PQTRANS_INERROR) {
		st_log_write2(st_log_level_error, st_log_type_plugin_db, gettext("PSQL: Can't create checkpoint because there is an error into current transaction"));
		return -1;
	} else if (transStatus == PQTRANS_IDLE) {
		st_log_write2(st_log_level_error, st_log_type_plugin_db, gettext("PSQL: Can't create checkpoint because there is no active transaction"));
		return -1;
	}

	char * query = NULL;
	asprintf(&query, "SAVEPOINT %s", checkpoint);

	PGresult * result = PQexec(self->connect, query);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, NULL);

	if (status != PGRES_COMMAND_OK)
		st_log_write2(st_log_level_error, st_log_type_plugin_db, gettext("PSQL: error while creating a savepoint => %s"), PQerrorMessage(self->connect));

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
			st_log_write2(st_log_level_error, st_log_type_plugin_db, gettext("PSQL: Rolling back transaction because current transaction is in error"));

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
		st_log_write2(st_log_level_error, st_log_type_plugin_db, gettext("PSQL: Host not found into database (%s)"), name.nodename);

	PQclear(result);

	return hostid;
}

static int st_database_postgresql_get_host_by_name(struct st_database_connection * connect, struct st_host * host, const char * name) {
	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_host_by_name_by_name";
	st_database_postgresql_prepare(self, query, "SELECT CASE WHEN domaine IS NULL THEN name ELSE name || '.' || domaine END, uuid FROM host WHERE name = $1 OR name || '.' || domaine = $1 LIMIT 1");

	const char * param[] = { name };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		free(host->hostname);
		free(host->uuid);
		host->hostname = host->uuid = NULL;

		st_database_postgresql_get_string_dup(result, 0, 0, &host->hostname);
		st_database_postgresql_get_string_dup(result, 0, 1, &host->uuid);
	}

	PQclear(result);

	return status != PGRES_TUPLES_OK;
}


static struct st_value * st_database_postgresql_get_changers(struct st_database_connection * connect) {
	struct st_value * changers = st_value_new_linked_list();

	char * host_id = st_database_postgresql_get_host(connect);
	if (host_id == NULL)
		return changers;

	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_real_changer_by_host";
	st_database_postgresql_prepare(self, query, "SELECT DISTINCT c.id, c.model, c.vendor, c.firmwarerev, c.serialnumber, c.wwn, c.barcode, c.status, c.isonline, c.action, c.enable FROM changer c LEFT JOIN drive d ON c.id = d.changer AND c.serialnumber != d.serialnumber WHERE c.host = $1 AND c.serialnumber NOT IN (SELECT uuid::TEXT FROM vtl WHERE host = $1)");

	const char * param[] = { host_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		int i;
		for (i = 0; i < nb_result; i++) {
			char * changer_id;
			st_database_postgresql_get_string_dup(result, i, 0, &changer_id);

			bool barcode = false, isonline = false, enabled = false;
			st_database_postgresql_get_bool(result, i, 6, &barcode);
			st_database_postgresql_get_bool(result, i, 8, &isonline);
			st_database_postgresql_get_bool(result, i, 10, &enabled);

			struct st_value * changer = st_value_pack("{sssssssssssbsosssbsssb}",
					"model", PQgetvalue(result, i, 1),
					"vendor", PQgetvalue(result, i, 2),
					"firmwarerev", PQgetvalue(result, i, 3),
					"serial number", PQgetvalue(result, i, 4),
					"wwn", PQgetvalue(result, i, 5),
					"barcode", barcode,
					"drives", st_database_postgresql_get_drives_by_changer(connect, changer_id),
					"status", PQgetvalue(result, i, 7),
					"is online", isonline,
					"action", PQgetvalue(result, i, 9),
					"enable", enabled
			);

			st_value_list_push(changers, changer, true);

			free(changer_id);
		}
	}

	PQclear(result);
	free(host_id);

	return changers;
}

static struct st_value * st_database_postgresql_get_drives_by_changer(struct st_database_connection * connect, const char * changer_id) {
	struct st_value * drives = st_value_new_linked_list();

	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_drives_by_changer";
	st_database_postgresql_prepare(self, query, "SELECT id, model, vendor, firmwarerev, serialnumber, enable FROM drive WHERE changer = $1");

	const char * param[] = { changer_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		int i;
		for (i = 0; i < nb_result; i++) {
			char * drive_id = PQgetvalue(result, i, 0);
			bool enabled = false;
			st_database_postgresql_get_bool(result, i, 5, &enabled);

			struct st_value * drive = st_value_pack("{sssssssssbso}",
					"model", PQgetvalue(result, i, 1),
					"vendor", PQgetvalue(result, i, 2),
					"firmware revision", PQgetvalue(result, i, 3),
					"serial number", PQgetvalue(result, i, 4),
					"enable", enabled,
					"slot", st_database_postgresql_get_slot_by_drive(connect, drive_id)
			);

			st_value_list_push(drives, drive, true);
		}
	}

	PQclear(result);

	return drives;
}

static struct st_media * st_database_postgresql_get_media(struct st_database_connection * connect, const char * medium_serial_number, const char * label, struct st_job * job) {
	if (connect == NULL || (medium_serial_number == NULL && label == NULL && job == NULL))
		return NULL;

	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query;
	PGresult * result;

	char * job_id = NULL;

	if (medium_serial_number != NULL) {
		query = "select_media_by_medium_serial_number";
		st_database_postgresql_prepare(self, query, "SELECT m.id, mf.id, m.uuid, label, mediumserialnumber, m.name, m.status, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, operationcount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, m.blocksize, freeblock, totalblock, append, m.type, writelock, nbfiles, densitycode, mode, p.uuid FROM media m LEFT JOIN mediaformat mf ON m.mediaformat = mf.id LEFT JOIN pool p ON m.pool = p.id WHERE mediumserialnumber = $1 LIMIT 1");

		const char * param[] = { medium_serial_number };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	} else if (label != NULL) {
		query = "select_media_by_label";
		st_database_postgresql_prepare(self, query, "SELECT m.id, mf.id, m.uuid, label, mediumserialnumber, m.name, m.status, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, operationcount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, m.blocksize, freeblock, totalblock, append, m.type, writelock, nbfiles, densitycode, mode, p.uuid FROM media m LEFT JOIN mediaformat mf ON m.mediaformat = mf.id LEFT JOIN pool p ON m.pool = p.id WHERE label = $1 LIMIT 1");

		const char * param[] = { label };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	} else {
		query = "select_media_by_job";
		st_database_postgresql_prepare(self, query, "SELECT m.id, mf.id, m.uuid, label, mediumserialnumber, m.name, m.status, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, operationcount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, m.blocksize, freeblock, totalblock, append, m.type, writelock, nbfiles, densitycode, mode, p.uuid FROM job j LEFT JOIN media m ON j.media = m.id LEFT JOIN mediaformat mf ON m.mediaformat = mf.id LEFT JOIN pool p ON m.pool = p.id WHERE j.id = $1 LIMIT 1");

		struct st_value * key = st_value_new_custom(connect->config, NULL);
		struct st_value * job_data = st_value_hashtable_get(job->db_data, key, false, false);
		st_value_unpack(job_data, "{ss}", "id", &job_id);
		st_value_free(key);

		const char * param[] = { job_id };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	}

	struct st_media * media = NULL;

	ExecStatusType status = PQresultStatus(result);
	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		media = malloc(sizeof(struct st_media));
		bzero(media, sizeof(struct st_media));

		media->db_data = st_value_new_hashtable(st_value_custom_compute_hash);
		struct st_value * key = st_value_new_custom(connect->config, NULL);
		struct st_value * db = st_value_new_hashtable2();
		st_value_hashtable_put(media->db_data, key, true, db, true);

		st_value_hashtable_put2(db, "id", st_value_new_string(PQgetvalue(result, 0, 0)), true);
		st_value_hashtable_put2(db, "media format id", st_value_new_string(PQgetvalue(result, 0, 1)), true);

		st_database_postgresql_get_string(result, 0, 2, media->uuid, 37);
		st_database_postgresql_get_string_dup(result, 0, 3, &media->label);
		st_database_postgresql_get_string_dup(result, 0, 4, &media->medium_serial_number);
		st_database_postgresql_get_string_dup(result, 0, 5, &media->name);

		media->status = st_media_string_to_status(PQgetvalue(result, 0, 6));

		st_database_postgresql_get_time(result, 0, 7, &media->first_used);
		st_database_postgresql_get_time(result, 0, 8, &media->use_before);
		if (!PQgetisnull(result, 0, 9))
			st_database_postgresql_get_time(result, 0, 9, &media->last_read);
		if (!PQgetisnull(result, 0, 10))
			st_database_postgresql_get_time(result, 0, 10, &media->last_write);

		st_database_postgresql_get_long(result, 0, 11, &media->load_count);
		st_database_postgresql_get_long(result, 0, 12, &media->read_count);
		st_database_postgresql_get_long(result, 0, 13, &media->write_count);
		st_database_postgresql_get_long(result, 0, 14, &media->operation_count);

		st_database_postgresql_get_long(result, 0, 15, &media->nb_total_read);
		st_database_postgresql_get_long(result, 0, 16, &media->nb_total_write);

		st_database_postgresql_get_uint(result, 0, 17, &media->nb_read_errors);
		st_database_postgresql_get_uint(result, 0, 18, &media->nb_write_errors);

		st_database_postgresql_get_ssize(result, 0, 19, &media->block_size);
		st_database_postgresql_get_ssize(result, 0, 20, &media->free_block);
		st_database_postgresql_get_ssize(result, 0, 21, &media->total_block);

		st_database_postgresql_get_bool(result, 0, 22, &media->append);
		media->type = st_media_string_to_type(PQgetvalue(result, 0, 23));
		st_database_postgresql_get_bool(result, 0, 24, &media->write_lock);
		st_database_postgresql_get_uint(result, 0, 25, &media->nb_volumes);

		unsigned char density_code;
		st_database_postgresql_get_uchar(result, 0, 26, &density_code);
		enum st_media_format_mode mode = st_media_string_to_format_mode(PQgetvalue(result, 0, 27));
		media->format = st_database_postgresql_get_media_format(connect, density_code, mode);

		if (!PQgetisnull(result, 0, 28))
			media->pool = st_database_postgresql_get_pool(connect, PQgetvalue(result, 0, 28), NULL);
	}

	PQclear(result);
	free(job_id);
	return media;
}

static struct st_media_format * st_database_postgresql_get_media_format(struct st_database_connection * connect, unsigned int density_code, enum st_media_format_mode mode) {
	if (connect == NULL || density_code == 0 || mode == st_media_format_mode_unknown)
		return NULL;

	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_media_format_by_density_code";
	st_database_postgresql_prepare(self, query, "SELECT id, name, datatype, maxloadcount, maxreadcount, maxwritecount, maxopcount, EXTRACT('epoch' FROM lifespan), capacity, blocksize, densitycode, supportpartition, supportmam FROM mediaformat WHERE densitycode = $1 AND mode = $2 LIMIT 1");

	char * c_density_code = NULL;
	asprintf(&c_density_code, "%d", density_code);

	const char * param[] = { c_density_code, st_media_format_mode_to_string(mode, false) };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	free(c_density_code);

	struct st_media_format * format = NULL;

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		format = malloc(sizeof(struct st_media_format));
		bzero(format, sizeof(struct st_media_format));

		struct st_value * key = st_value_new_custom(connect->config, NULL);
		format->db_data = st_value_new_hashtable(st_value_custom_compute_hash);
		st_value_hashtable_put(format->db_data, key, true, st_value_pack("{ss}", "id", PQgetvalue(result, 0, 0)), true);

		st_database_postgresql_get_string(result, 0, 1, format->name, 64);
		format->density_code = density_code;
		format->type = st_media_string_to_format_data_type(PQgetvalue(result, 0, 2));
		format->mode = mode;

		st_database_postgresql_get_long(result, 0, 3, &format->max_load_count);
		st_database_postgresql_get_long(result, 0, 4, &format->max_read_count);
		st_database_postgresql_get_long(result, 0, 5, &format->max_write_count);
		st_database_postgresql_get_long(result, 0, 6, &format->max_operation_count);

		st_database_postgresql_get_long(result, 0, 7, &format->life_span);

		st_database_postgresql_get_ssize(result, 0, 8, &format->capacity);
		st_database_postgresql_get_ssize(result, 0, 9, &format->block_size);
		st_database_postgresql_get_uchar(result, 0, 10, &format->density_code);

		st_database_postgresql_get_bool(result, 0, 11, &format->support_partition);
		st_database_postgresql_get_bool(result, 0, 12, &format->support_mam);
	}

	PQclear(result);

	return format;
}

static struct st_pool * st_database_postgresql_get_pool(struct st_database_connection * connect, const char * uuid, struct st_job * job) {
	if (connect == NULL || (uuid == NULL && job == NULL))
		return NULL;

	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query;
	PGresult * result;

	if (uuid != NULL) {
		query = "select_pool_by_uuid";
		st_database_postgresql_prepare(self, query, "SELECT uuid, p.name, autocheck, growable, unbreakablelevel, rewritable, deleted, densitycode, mode FROM pool p LEFT JOIN mediaformat mf ON p.mediaformat = mf.id WHERE uuid = $1 LIMIT 1");

		const char * param[] = { uuid };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	} else {
		query = "select_pool_by_job";
		st_database_postgresql_prepare(self, query, "SELECT uuid, p.name, autocheck, growable, unbreakablelevel, rewritable, deleted, densitycode, mode FROM job j INNER JOIN pool p ON j.pool = p.id LEFT JOIN mediaformat mf ON p.mediaformat = mf.id WHERE j.id = $1 LIMIT 1");

		char * job_id = NULL;

		struct st_value * key = st_value_new_custom(connect->config, NULL);
		struct st_value * job_data = st_value_hashtable_get(job->db_data, key, false, false);
		st_value_unpack(job_data, "{ss}", "id", &job_id);
		st_value_free(key);

		const char * param[] = { job_id };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);

		free(job_id);
	}

	struct st_pool * pool = NULL;

	ExecStatusType status = PQresultStatus(result);
	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		pool = malloc(sizeof(struct st_pool));
		bzero(pool, sizeof(struct st_pool));

		st_database_postgresql_get_string(result, 0, 0, pool->uuid, 37);
		st_database_postgresql_get_string_dup(result, 0, 1, &pool->name);
		pool->auto_check = st_pool_string_to_autocheck_mode(PQgetvalue(result, 0, 2));
		st_database_postgresql_get_bool(result, 0, 3, &pool->growable);
		pool->unbreakable_level = st_pool_string_to_unbreakable_level(PQgetvalue(result, 0, 4));
		st_database_postgresql_get_bool(result, 0, 5, &pool->rewritable);
		st_database_postgresql_get_bool(result, 0, 6, &pool->deleted);

		unsigned char density_code;
		st_database_postgresql_get_uchar(result, 0, 7, &density_code);
		enum st_media_format_mode mode = st_media_string_to_format_mode(PQgetvalue(result, 0, 8));
		pool->format = st_database_postgresql_get_media_format(connect, density_code, mode);
	}

	PQclear(result);

	return pool;
}

static struct st_value * st_database_postgresql_get_standalone_drives(struct st_database_connection * connect) {
	struct st_value * changers = st_value_new_linked_list();

	char * host_id = st_database_postgresql_get_host(connect);
	if (host_id == NULL)
		return changers;

	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_real_changer_by_host";
	st_database_postgresql_prepare(self, query, "SELECT c.device, c.model, c.vendor, c.firmwarerev, c.serialnumber, c.status, c.isonline, c.action, c.enable FROM changer c LEFT JOIN drive d ON c.id = d.changer AND c.serialnumber = d.serialnumber WHERE c.host = $1 AND c.serialnumber NOT IN (SELECT uuid::TEXT FROM vtl WHERE host = $1)");

	const char * param[] = { host_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		int i;
		for (i = 0; i < nb_result; i++) {
			bool enabled = false;
			st_database_postgresql_get_bool(result, i, 11, &enabled);

			struct st_value * changer = st_value_pack("{sssssssssssnsbsssbsssb}",
					"device", PQgetvalue(result, i, 0),
					"model", PQgetvalue(result, i, 1),
					"vendor", PQgetvalue(result, i, 2),
					"firmwarerev", PQgetvalue(result, i, 3),
					"serialnumber", PQgetvalue(result, i, 4),
					"wwn",
					"barcode", false,
					"status", PQgetvalue(result, i, 5),
					"is online", true,
					"action", "none",
					"enable", enabled
			);

			st_value_list_push(changers, changer, true);
		}
	}

	PQclear(result);
	free(host_id);

	return changers;
}

static struct st_value * st_database_postgresql_get_slot_by_drive(struct st_database_connection * connect, const char * drive_id) {
	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_slot_by_drive";
	st_database_postgresql_prepare(self, query, "SELECT index, isieport, enable FROM changerslot WHERE drive = $1");

	const char * param[] = { drive_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	struct st_value * slot = st_value_new_null();
	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK) {
		int index = -1;
		bool enable = false;
		bool ie_port = false;
		st_database_postgresql_get_int(result, 0, 0, &index);
		st_database_postgresql_get_bool(result, 0, 1, &ie_port);
		st_database_postgresql_get_bool(result, 0, 2, &enable);

		slot = st_value_pack("{sisssb}",
			"index", index,
			"ie port", ie_port,
			"enable", enable
		);
	}

	PQclear(result);

	return slot;
}

static struct st_value * st_database_postgresql_get_vtls(struct st_database_connection * connect) {
	struct st_value * changers = st_value_new_linked_list();

	char * host_id = st_database_postgresql_get_host(connect);
	if (host_id == NULL)
		return changers;

	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_vtls";
	st_database_postgresql_prepare(self, query, "SELECT v.uuid, v.path, v.prefix, v.nbslots, v.nbdrives, v.deleted, mf.name, mf.densitycode, mf.mode, c.id FROM vtl v INNER JOIN mediaformat mf ON v.mediaformat = mf.id LEFT JOIN changer c ON v.uuid::TEXT = c.serialnumber WHERE v.host = $1");

	const char * params[] = { host_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, params, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		int i;
		for (i = 0; i < nb_result; i++) {
			char * changer_id = NULL;
			if (!PQgetisnull(result, i, 9))
				st_database_postgresql_get_string_dup(result, i, 9, &changer_id);

			unsigned int nb_slots, nb_drives;
			bool deleted;

			st_database_postgresql_get_uint(result, i, 3, &nb_slots);
			st_database_postgresql_get_uint(result, i, 4, &nb_drives);
			st_database_postgresql_get_bool(result, i, 5, &deleted);

			unsigned int density_code;
			st_database_postgresql_get_uint(result, i, 7, &density_code);
			enum st_media_format_mode mode = st_media_string_to_format_mode(PQgetvalue(result, i, 8));
			struct st_media_format * format = st_database_postgresql_get_media_format(connect, density_code, mode);

			struct st_value * changer = st_value_pack("{s[]sssssssisisbso}",
				"drives",
				"uuid", PQgetvalue(result, i, 0),
				"path", PQgetvalue(result, i, 1),
				"prefix", PQgetvalue(result, i, 2),
				"nb slots", (long long) nb_slots,
				"nb drives", (long long) nb_drives,
				"deleted", deleted,
				"format", st_media_format_convert(format)
			);

			st_value_list_push(changers, changer, true);

			st_media_format_free(format);
			free(changer_id);
		}
	}

	return changers;
}

static int st_database_postgresql_sync_changer(struct st_database_connection * connect, struct st_changer * changer, enum st_database_sync_method method) {
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

	struct st_value * key = st_value_new_custom(connect->config, NULL);
	struct st_value * db = NULL;
	if (changer->db_data == NULL) {
		changer->db_data = st_value_new_hashtable(st_value_custom_compute_hash);

		db = st_value_new_hashtable2();
		st_value_hashtable_put(changer->db_data, key, true, db, true);
	} else {
		db = st_value_hashtable_get(changer->db_data, key, false, false);
		st_value_free(key);
	}

	char * changer_id = NULL;
	st_value_unpack(db, "{ss}", "id", &changer_id);

	bool enabled = true;
	if (changer_id != NULL) {
		const char * query = "select_changer_by_id";
		st_database_postgresql_prepare(self, query, "SELECT action, enable FROM changer WHERE id = $1 FOR UPDATE");

		const char * param[] = { changer_id };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);
		int nb_result = PQntuples(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && nb_result == 1 && method != st_database_sync_id_only) {
			enum st_changer_action new_action = st_changer_string_to_action(PQgetvalue(result, 0, 0));
			if (changer->status == st_changer_status_idle) {
				if (changer->is_online && new_action == st_changer_action_put_offline)
					changer->next_action = new_action;
				else if (!changer->is_online && new_action == st_changer_action_put_online)
					changer->next_action = new_action;
				else
					changer->next_action = st_changer_action_none;
			}

			st_database_postgresql_get_bool(result, 0, 1, &enabled);
		} else if (status == PGRES_TUPLES_OK && nb_result == 0)
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

		if (changer->wwn != NULL) {
			query = "select_changer_by_model_vendor_serialnumber_wwn";
			st_database_postgresql_prepare(self, query, "SELECT id, enable FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 AND wwn = $4 FOR UPDATE");

			const char * param[] = { changer->model, changer->vendor, changer->serial_number, changer->wwn };
			result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
		} else {
			query = "select_changer_by_model_vendor_serialnumber";
			st_database_postgresql_prepare(self, query, "SELECT id, enable FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 AND wwn IS NULL FOR UPDATE");

			const char * param[] = { changer->model, changer->vendor, changer->serial_number };
			result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
		}
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_database_postgresql_get_string_dup(result, 0, 0, &changer_id);
			if (method != st_database_sync_id_only)
				st_database_postgresql_get_bool(result, 0, 1, &changer->enable);

			st_value_hashtable_put2(db, "id", st_value_new_string(changer_id), true);
		}

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			free(hostid);
			free(changer_id);

			if (transStatus == PQTRANS_IDLE)
				st_database_postgresql_cancel_transaction(connect);
			return 3;
		}
	}

	if (method != st_database_sync_id_only) {
		if (changer_id != NULL) {
			const char * query = "update_changer";
			st_database_postgresql_prepare(self, query, "UPDATE changer SET status = $1, firmwarerev = $2, isonline = $3, action = $4 WHERE id = $5");

			const char * params[] = {
				st_changer_status_to_string(changer->status, false), changer->revision, st_database_postgresql_bool_to_string(changer->is_online),
				st_changer_action_to_string(changer->next_action, false), changer_id
			};
			PGresult * result = PQexecPrepared(self->connect, query, 5, params, NULL, NULL, 0);
			ExecStatusType status = PQresultStatus(result);

			PQclear(result);

			if (status == PGRES_FATAL_ERROR) {
				st_database_postgresql_get_error(result, query);

				free(hostid);

				if (transStatus == PQTRANS_IDLE)
					st_database_postgresql_cancel_transaction(connect);
				return -2;
			}
		} else if (method == st_database_sync_init) {
			const char * query = "insert_changer";
			st_database_postgresql_prepare(self, query, "INSERT INTO changer(status, barcode, model, vendor, firmwarerev, serialnumber, wwn, host) VALUES ($1, $2, $3, $4, $5, $6, $7, $8) RETURNING id");

			const char * param[] = {
				st_changer_status_to_string(changer->status, false), changer->barcode ? "true" : "false",
				changer->model, changer->vendor, changer->revision, changer->serial_number, changer->wwn, hostid,
			};

			PGresult * result = PQexecPrepared(self->connect, query, 8, param, NULL, NULL, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_database_postgresql_get_error(result, query);
			else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
				st_database_postgresql_get_string_dup(result, 0, 0, &changer_id);
				st_value_hashtable_put2(db, "id", st_value_new_string(changer_id), true);
			}

			PQclear(result);

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
	}

	free(hostid);

	int failed = 0;
	unsigned int i;

	if (method == st_database_sync_id_only || method == st_database_sync_init) {
		st_database_postgresql_create_checkpoint(connect, "after_changers");

		for (i = 0; failed == 0 && i < changer->nb_drives; i++)
			if (changer->drives[i].enable)
				failed = st_database_postgresql_sync_drive(connect, changer->drives + i, method);
	}

	if (method == st_database_sync_init) {
		const char * query = "delete_from_changerslot_by_changer";
		st_database_postgresql_prepare(self, query, "DELETE FROM changerslot WHERE changer = $1");

		const char * param[] = { changer_id };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		PQclear(result);

		if (failed != 0) {
			st_database_postgresql_cancel_checkpoint(connect, "after_changers");
			failed = 0;
		}
	}

	free(changer_id);

	for (i = 0; failed == 0 && i < changer->nb_slots; i++)
		failed = st_database_postgresql_sync_slots(connect, changer->slots + i, method);

	if (transStatus == PQTRANS_IDLE && failed == 0)
		st_database_postgresql_finish_transaction(connect);

	return 0;
}

static int st_database_postgresql_sync_drive(struct st_database_connection * connect, struct st_drive * drive, enum st_database_sync_method method) {
	if (connect == NULL || drive == NULL)
		return -1;

	struct st_database_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType transStatus = PQtransactionStatus(self->connect);
	if (transStatus == PQTRANS_INERROR)
		return 2;
	else if (transStatus == PQTRANS_IDLE)
		st_database_postgresql_start_transaction(connect);

	struct st_value * key = st_value_new_custom(connect->config, NULL);
	struct st_value * db = NULL;
	if (drive->db_data == NULL) {
		drive->db_data = st_value_new_hashtable(st_value_custom_compute_hash);

		db = st_value_new_hashtable2();
		st_value_hashtable_put(drive->db_data, key, true, db, true);

		if (drive->changer != NULL) {
			struct st_value * changer_data = st_value_hashtable_get(drive->changer->db_data, key, false, false);
			struct st_value * changer_id = st_value_hashtable_get2(changer_data, "id", true, false);
			st_value_hashtable_put2(db, "changer id", changer_id, true);
		}
	} else {
		db = st_value_hashtable_get(drive->db_data, key, false, false);

		if (drive->changer != NULL && !st_value_hashtable_has_key2(db, "changer_id")) {
			struct st_value * changer_data = st_value_hashtable_get(drive->changer->db_data, key, false, false);
			struct st_value * changer_id = st_value_hashtable_get2(changer_data, "id", true, false);
			st_value_hashtable_put2(db, "changer id", changer_id, true);
		}

		st_value_free(key);
	}

	char * drive_id = NULL;
	st_value_unpack(db, "{ss}", "id", &drive_id);

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
				st_value_hashtable_put2(db, "driveformat id", st_value_new_string(driveformat_id), true);
			}

			st_database_postgresql_get_bool(result, 0, 1, &drive->enable);
		} else if (status == PGRES_TUPLES_OK && nb_result == 1)
			drive->enable = false;

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

		const char * param[] = { drive->model, drive->vendor, drive->serial_number };
		PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_database_postgresql_get_string_dup(result, 0, 0, &drive_id);
			st_value_hashtable_put2(db, "id", st_value_new_string(drive_id), true);

			if (method != st_database_sync_id_only) {
				double old_operation_duration = 0;
				st_database_postgresql_get_double(result, 0, 1, &old_operation_duration);
				drive->operation_duration += old_operation_duration;

				if (!PQgetisnull(result, 0, 2))
					st_database_postgresql_get_time(result, 0, 2, &drive->last_clean);
			}

			if (!PQgetisnull(result, 0, 3)) {
				st_database_postgresql_get_string_dup(result, 0, 3, &driveformat_id);
				st_value_hashtable_put2(db, "driveformat id", st_value_new_string(driveformat_id), true);
			}

			st_database_postgresql_get_bool(result, 0, 4, &drive->enable);
		}

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			if (transStatus == PQTRANS_IDLE)
				st_database_postgresql_cancel_transaction(connect);
			return 3;
		}
	}

	if (method != st_database_sync_id_only) {
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

			if (drive->density_code > density_code) {
				free(driveformat_id);
				driveformat_id = NULL;
			}
		}

		if (driveformat_id == NULL && drive->mode != st_media_format_mode_unknown) {
			const char * query = "select_driveformat_by_densitycode";
			st_database_postgresql_prepare(self, query, "SELECT id FROM driveformat WHERE densitycode = $1 AND mode = $2 LIMIT 1");

			char * densitycode = NULL;
			asprintf(&densitycode, "%u", drive->density_code);

			const char * param[] = { densitycode, st_media_format_mode_to_string(drive->mode, false), };
			PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_database_postgresql_get_error(result, query);
			else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
				st_database_postgresql_get_string_dup(result, 0, 0, &driveformat_id);
				st_value_hashtable_put2(db, "driveformat id", st_value_new_string(driveformat_id), true);
			}

			PQclear(result);
			free(densitycode);
		}

		if (drive_id != NULL) {
			const char * query = "update_drive";
			st_database_postgresql_prepare(self, query, "UPDATE drive SET status = $1, operationduration = $2, lastclean = $3, firmwarerev = $4, driveformat = $5 WHERE id = $6");

			char * op_duration = NULL, * last_clean = NULL;
			asprintf(&op_duration, "%.3f", drive->operation_duration);

			if (drive->last_clean > 0) {
				last_clean = malloc(24);
				st_time_convert(&drive->last_clean, "%F %T", last_clean, 24);
			}

			const char * param[] = {
				st_drive_status_to_string(drive->status, false), op_duration, last_clean, drive->revision, driveformat_id, drive_id,
			};
			PGresult * result = PQexecPrepared(self->connect, query, 6, param, NULL, NULL, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_database_postgresql_get_error(result, query);

			PQclear(result);
			free(last_clean);
			free(op_duration);

			if (status == PGRES_FATAL_ERROR) {
				free(drive_id);

				if (transStatus == PQTRANS_IDLE)
					st_database_postgresql_cancel_transaction(connect);
				return 3;
			}
		} else if (method == st_database_sync_init) {
			const char * query = "insert_drive";
			st_database_postgresql_prepare(self, query, "INSERT INTO drive (status, operationduration, lastclean, model, vendor, firmwarerev, serialnumber, changer, driveformat) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9) RETURNING id");

			char * op_duration = NULL, * last_clean = NULL;
			asprintf(&op_duration, "%.3f", drive->operation_duration);

			db = st_value_hashtable_get(drive->db_data, key, false, false);

			char * changer_id = NULL;
			st_value_unpack(db, "{ss}", "changer id", &changer_id);

			if (drive->last_clean > 0) {
				last_clean = malloc(24);
				st_time_convert(&drive->last_clean, "%F %T", last_clean, 24);
			}

			const char * param[] = {
				st_drive_status_to_string(drive->status, false), op_duration, last_clean,
				drive->model, drive->vendor, drive->revision, drive->serial_number, changer_id, driveformat_id
			};
			PGresult * result = PQexecPrepared(self->connect, query, 9, param, NULL, NULL, 0);
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

			if (status == PGRES_FATAL_ERROR) {
				free(drive_id);

				if (transStatus == PQTRANS_IDLE)
					st_database_postgresql_cancel_transaction(connect);
				return 3;
			}
		}
	}

	free(driveformat_id);
	free(drive_id);

	if (drive->slot != NULL && drive->slot->media != NULL)
		st_database_postgresql_sync_media(connect, drive->slot->media, method);

	if (transStatus == PQTRANS_IDLE)
		st_database_postgresql_finish_transaction(connect);

	return 0;
}

static int st_database_postgresql_sync_media(struct st_database_connection * connect, struct st_media * media, enum st_database_sync_method method) {
	struct st_database_postgresql_connection_private * self = connect->data;

	struct st_value * key = st_value_new_custom(connect->config, NULL);
	struct st_value * db = NULL;
	if (media->db_data == NULL) {
		media->db_data = st_value_new_hashtable(st_value_custom_compute_hash);

		db = st_value_new_hashtable2();
		st_value_hashtable_put(media->db_data, key, true, db, true);
	} else {
		db = st_value_hashtable_get(media->db_data, key, false, false);
		st_value_free(key);
	}

	char * media_id = NULL, * mediaformat_id = NULL, * pool_id = NULL;
	st_value_unpack(db, "{ssss}", "id", &media_id, "media format id", &mediaformat_id);

	int failed = 0;

	if (method != st_database_sync_init && media_id == NULL) {
		const char * query = "select_media_by_medium_serial_number";
		st_database_postgresql_prepare(self, query, "SELECT id, label, name, mediaformat, pool FROM media WHERE mediumserialnumber = $1 LIMIT 1");

		const char * param[] = { media->medium_serial_number };

		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			free(media->label);
			free(media->name);
			media->label = media->name = NULL;

			st_database_postgresql_get_string_dup(result, 0, 0, &media_id);
			st_value_hashtable_put2(db, "id", st_value_new_string(media_id), true);

			st_database_postgresql_get_string_dup(result, 0, 1, &media->label);
			st_database_postgresql_get_string_dup(result, 0, 2, &media->name);

			st_database_postgresql_get_string_dup(result, 0, 2, &mediaformat_id);
			st_value_hashtable_put2(db, "media format id", st_value_new_string(mediaformat_id), true);

			if (!PQgetisnull(result, 0, 4)) {
				st_database_postgresql_get_string_dup(result, 0, 4, &pool_id);
				st_value_hashtable_put2(db, "pool id", st_value_new_string(pool_id), true);
			}
		}

		PQclear(result);
	} else if (media != NULL) {
		const char * query = "select_media_for_sync";
		st_database_postgresql_prepare(self, query, "SELECT label, name FROM media WHERE id = $1 LIMIT 1");

		const char * param[] = { media_id };

		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			free(media->label);
			free(media->name);
			media->label = media->name = NULL;

			st_database_postgresql_get_string_dup(result, 0, 0, &media->label);
			st_database_postgresql_get_string_dup(result, 0, 1, &media->name);
		}

		PQclear(result);
	}

	if (media_id == NULL) {
		if (mediaformat_id == NULL) {
			if (media->format->db_data != NULL) {
				struct st_value * format_db = st_value_hashtable_get(media->format->db_data, key, false, false);
				st_value_unpack(format_db, "{ss}", "id", &mediaformat_id);
				st_value_hashtable_put2(db, "media format id", st_value_new_string(mediaformat_id), true);
			} else {
				const char * query = "select_media_format_by_density";
				st_database_postgresql_prepare(self, query, "SELECT id FROM mediaformat WHERE densitycode = $1 AND mode = $2");

				char * densitycode = NULL;
				asprintf(&densitycode, "%hhu", media->format->density_code);

				const char * param[] = { densitycode, st_media_format_mode_to_string(media->format->mode, false) };
				PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
				ExecStatusType status = PQresultStatus(result);

				if (status == PGRES_FATAL_ERROR)
					st_database_postgresql_get_error(result, query);
				else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
					st_database_postgresql_get_string_dup(result, 0, 0, &mediaformat_id);
					st_value_hashtable_put2(db, "media format id", st_value_new_string(mediaformat_id), true);
				}

				free(densitycode);
				PQclear(result);
			}
		}

		if (pool_id == NULL && media->pool != NULL) {
		}

		const char * query = "insert_into_media";
		st_database_postgresql_prepare(self, query, "INSERT INTO media(uuid, label, mediumserialnumber, name, status, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, type, nbfiles, blocksize, freeblock, totalblock, mediaformat, pool) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20, $21, $22, $23) RETURNING id");

		char buffer_first_used[32];
		char buffer_use_before[32];
		char buffer_last_read[32] = "";
		char buffer_last_write[32] = "";
		st_time_convert(&media->first_used, "%F %T", buffer_first_used, 32);
		st_time_convert(&media->use_before, "%F %T", buffer_use_before, 32);
		if (media->last_read > 0)
			st_time_convert(&media->last_read, "%F %T", buffer_last_read, 32);
		if (media->last_write > 0)
			st_time_convert(&media->last_write, "%F %T", buffer_last_write, 32);

		char * load, * read, * write, * nbfiles, * blocksize, * freeblock, * totalblock, * totalblockread, * totalblockwrite, * totalreaderror, * totalwriteerror;
		asprintf(&load, "%ld", media->load_count);
		asprintf(&read, "%ld", media->read_count);
		asprintf(&write, "%ld", media->write_count);
		asprintf(&nbfiles, "%u", media->nb_volumes);
		asprintf(&blocksize, "%zd", media->block_size);
		asprintf(&freeblock, "%zd", media->free_block);
		asprintf(&totalblock, "%zd", media->total_block);
		asprintf(&totalblockread, "%zd", media->nb_total_read);
		asprintf(&totalblockwrite, "%zd", media->nb_total_write);
		asprintf(&totalreaderror, "%u", media->nb_read_errors);
		asprintf(&totalwriteerror, "%u", media->nb_write_errors);

		const char * param[] = {
			*media->uuid ? media->uuid : NULL, media->label, media->medium_serial_number, media->name, st_media_status_to_string(media->status, false),
			buffer_first_used, buffer_use_before, media->last_read > 0 ? buffer_last_read : NULL, media->last_write > 0 ? buffer_last_write : NULL,
			load, read, write, totalblockread, totalblockwrite, totalreaderror, totalwriteerror, st_media_type_to_string(media->type, false), nbfiles,
			blocksize, freeblock, totalblock, mediaformat_id, pool_id
		};
		PGresult * result = PQexecPrepared(self->connect, query, 23, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_value_hashtable_put2(db, "id", st_value_new_string(PQgetvalue(result, 0, 0)), true);

		PQclear(result);
		free(load);
		free(read);
		free(write);
		free(nbfiles);
		free(blocksize);
		free(mediaformat_id);
		free(pool_id);
		free(totalblockread);
		free(totalblockwrite);
		free(totalreaderror);
		free(totalwriteerror);

		return status == PGRES_FATAL_ERROR;
	} else if (method != st_database_sync_id_only) {
		const char * query = "update_media";
		st_database_postgresql_prepare(self, query, "UPDATE media SET uuid = $1, name = $2, status = $3, lastread = $4, lastwrite = $5, loadcount = $6, readcount = $7, writecount = $8, nbtotalblockread = $9, nbtotalblockwrite = $10, nbreaderror = $11, nbwriteerror = $12, nbfiles = $13, blocksize = $14, freeblock = $15, totalblock = $16, pool = $17, type = $18 WHERE id = $19");

		char buffer_last_read[32] = "";
		char buffer_last_write[32] = "";
		if (media->last_read > 0)
			st_time_convert(&media->last_read, "%F %T", buffer_last_read, 32);
		if (media->last_write > 0)
			st_time_convert(&media->last_write, "%F %T", buffer_last_write, 32);

		char * load, * read, * write, * nbfiles, * blocksize, * freeblock, * totalblock, * totalblockread, * totalblockwrite, * totalreaderror, * totalwriteerror;
		asprintf(&load, "%ld", media->load_count);
		asprintf(&read, "%ld", media->read_count);
		asprintf(&write, "%ld", media->write_count);
		asprintf(&nbfiles, "%u", media->nb_volumes);
		asprintf(&blocksize, "%zd", media->block_size);
		asprintf(&freeblock, "%zd", media->free_block);
		asprintf(&totalblock, "%zd", media->total_block);
		asprintf(&totalblockread, "%zd", media->nb_total_read);
		asprintf(&totalblockwrite, "%zd", media->nb_total_write);
		asprintf(&totalreaderror, "%u", media->nb_read_errors);
		asprintf(&totalwriteerror, "%u", media->nb_write_errors);

		const char * param2[] = {
			*media->uuid ? media->uuid : NULL, media->name, st_media_status_to_string(media->status, false),
			media->last_read > 0 ? buffer_last_read : NULL, media->last_write > 0 ? buffer_last_write : NULL,
			load, read, write, totalblockread, totalblockwrite, totalreaderror, totalwriteerror, nbfiles,
			blocksize, freeblock, totalblock, pool_id, st_media_type_to_string(media->type, false), media_id
		};
		PGresult * result = PQexecPrepared(self->connect, query, 19, param2, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);

		PQclear(result);
		free(load);
		free(read);
		free(write);
		free(nbfiles);
		free(blocksize);
		free(freeblock);
		free(totalblock);
		free(media_id);
		free(mediaformat_id);
		free(pool_id);
		free(totalblockread);
		free(totalblockwrite);
		free(totalreaderror);
		free(totalwriteerror);

		return status == PGRES_FATAL_ERROR;
	}

	return failed;
}

static int st_database_postgresql_sync_slots(struct st_database_connection * connect, struct st_slot * slot, enum st_database_sync_method method) {
	int failed = 0;

	if (slot->media != NULL)
		failed = st_database_postgresql_sync_media(connect, slot->media, method);

	if (failed != 0)
		return failed;

	struct st_database_postgresql_connection_private * self = connect->data;

	struct st_value * key = st_value_new_custom(connect->config, NULL);
	struct st_value * db = NULL;

	if (slot->db_data != NULL)
		db = st_value_hashtable_get(slot->db_data, key, false, false);

	if (db == NULL || db->type == st_value_null) {
		slot->db_data = st_value_new_hashtable(st_value_custom_compute_hash);

		db = st_value_new_hashtable2();
		st_value_hashtable_put(slot->db_data, key, false, db, true);

		struct st_value * changer_data = st_value_hashtable_get(slot->changer->db_data, key, false, false);
		struct st_value * changer_id = st_value_hashtable_get2(changer_data, "id", true, false);
		st_value_hashtable_put2(db, "changer id", changer_id, true);

		if (slot->drive != NULL) {
			struct st_value * drive_data = st_value_hashtable_get(slot->drive->db_data, key, false, false);
			struct st_value * drive_id = st_value_hashtable_get2(drive_data, "id", true, false);
			st_value_hashtable_put2(db, "drive id", drive_id, true);
		}
	} else {
		if (!st_value_hashtable_has_key2(db, "changer id")) {
			struct st_value * changer_data = st_value_hashtable_get(slot->changer->db_data, key, false, false);
			struct st_value * changer_id = st_value_hashtable_get2(changer_data, "id", true, false);
			st_value_hashtable_put2(db, "changer id", changer_id, true);
		}

		if (slot->drive != NULL && !st_value_hashtable_has_key2(db, "drive id")) {
			struct st_value * drive_data = st_value_hashtable_get(slot->drive->db_data, key, false, false);
			struct st_value * drive_id = st_value_hashtable_get2(drive_data, "id", true, false);
			st_value_hashtable_put2(db, "drive id", drive_id, true);
		}
	}

	char * changer_id = NULL, * drive_id = NULL, * media_id = NULL;
	st_value_unpack(db, "{ssss}", "changer id", &changer_id, "drive id", &drive_id);

	if (slot->media != NULL) {
		struct st_value * media_data = st_value_hashtable_get(slot->media->db_data, key, false, false);
		st_value_unpack(media_data, "{ss}", "id", &media_id);
	}

	st_value_free(key);

	if (method != st_database_sync_init) {
		const char * query = "select_changerslot_by_index";
		st_database_postgresql_prepare(self, query, "SELECT enable FROM changerslot WHERE changer = $1 AND index = $2 LIMIT 1");

		char * sindex;
		asprintf(&sindex, "%u", slot->index);

		const char * param[] = { changer_id, sindex };

		PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_database_postgresql_get_bool(result, 0, 0, &slot->enable);

		PQclear(result);
		free(sindex);
	}

	if (method == st_database_sync_init) {
		const char * query = "insert_into_changerslot";
		st_database_postgresql_prepare(self, query, "INSERT INTO changerslot(changer, index, drive, media, isieport) VALUES ($1, $2, $3, $4, $5)");

		char * sindex;
		asprintf(&sindex, "%u", slot->index);

		const char * param[] = { changer_id, sindex, drive_id, media_id, st_database_postgresql_bool_to_string(slot->is_ie_port) };

		PGresult * result = PQexecPrepared(self->connect, query, 5, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);

		PQclear(result);
		free(sindex);
	} else if (method != st_database_sync_id_only) {
		const char * query = "update_changerslot";
		st_database_postgresql_prepare(self, query, "UPDATE changerslot SET media = $1, enable = $2 WHERE changer = $3 AND index = $4");

		char * sindex;
		asprintf(&sindex, "%u", slot->index);

		const char * param[] = { media_id, slot->enable ? "true" : "false", changer_id, sindex };

		PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_database_postgresql_get_error(result, query);

		PQclear(result);
		free(sindex);
	}

	free(changer_id);
	free(drive_id);
	free(media_id);

	return failed;
}


static int st_database_postgresql_add_job_record(struct st_database_connection * connect, struct st_job * job, enum st_log_level level, enum st_job_record_notif notif, const char * message) {
	if (connect == NULL || job == NULL || message == NULL)
		return 1;

	struct st_database_postgresql_connection_private * self = connect->data;

	struct st_value * key = st_value_new_custom(connect->config, NULL);
	struct st_value * db = st_value_hashtable_get(job->db_data, key, false, false);
	st_value_free(key);

	char * jobrun_id = NULL;
	st_value_unpack(db, "{ss}", "jobrun id", &jobrun_id);

	const char * query = "insert_new_jobrecord";
	st_database_postgresql_prepare(self, query, "INSERT INTO jobrecord(jobrun, status, level, message, notif) VALUES ($1, $2, $3, $4, $5)");

	const char * param[] = { jobrun_id, st_job_status_to_string(job->status, false), st_database_postgresql_log_level_to_string(level), message, st_job_report_notif_to_string(notif, false) };
	PGresult * result = PQexecPrepared(self->connect, query, 5, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);

	PQclear(result);
	free(jobrun_id);

	return status != PGRES_TUPLES_OK;
}

static int st_database_postgresql_start_job(struct st_database_connection * connect, struct st_job * job) {
	if (connect == NULL || job == NULL)
		return 1;

	struct st_database_postgresql_connection_private * self = connect->data;

	struct st_value * key = st_value_new_custom(connect->config, NULL);
	struct st_value * db = st_value_hashtable_get(job->db_data, key, false, false);
	st_value_free(key);

	char * job_id = NULL;
	st_value_unpack(db, "{ss}", "id", &job_id);

	const char * query = "insert_new_jobrun";
	st_database_postgresql_prepare(self, query, "INSERT INTO jobrun(job, numrun) VALUES ($1, $2) RETURNING id");

	char * numrun = NULL;
	asprintf(&numrun, "%ld", job->num_runs);

	const char * param[] = { job_id, numrun };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		char * jobrun_id = PQgetvalue(result, 0, 0);
		st_value_hashtable_put2(db, "jobrun id", st_value_new_string(jobrun_id), true);
	}

	PQclear(result);
	free(job_id);
	free(numrun);

	return status != PGRES_TUPLES_OK;
}

static int st_database_postgresql_stop_job(struct st_database_connection * connect, struct st_job * job) {
	if (connect == NULL || job == NULL)
		return 1;

	struct st_value * key = st_value_new_custom(connect->config, NULL);
	struct st_value * db = st_value_hashtable_get(job->db_data, key, false, false);
	st_value_free(key);

	char * jobrun_id = NULL;
	st_value_unpack(db, "{ss}", "jobrun id", &jobrun_id);

	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "finish_jobrun";
	st_database_postgresql_prepare(self, query, "UPDATE jobrun SET endtime = NOW(), status = $1, done = $2, exitcode = $3, stoppedbyuser = $4 WHERE id = $5");

	char * done = NULL, * exitcode = NULL;
	asprintf(&done, "%f", job->done);
	asprintf(&exitcode, "%d", job->exit_code);

	const char * param[] = { st_job_status_to_string(job->status, false), done, exitcode, st_database_postgresql_bool_to_string(job->stopped_by_user), jobrun_id };
	PGresult * result = PQexecPrepared(self->connect, query, 5, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);

	PQclear(result);
	free(exitcode);
	free(done);
	free(jobrun_id);

	return status != PGRES_COMMAND_OK;
}

static int st_database_postgresql_sync_job(struct st_database_connection * connect, struct st_job * job) {
	if (connect == NULL || job == NULL)
		return 1;

	struct st_database_postgresql_connection_private * self = connect->data;

	struct st_value * key = st_value_new_custom(connect->config, NULL);
	char * job_id = NULL;
	char * jobrun_id = NULL;

	if (job->db_data == NULL) {
		job->db_data = st_value_new_hashtable(st_value_custom_compute_hash);
		struct st_value * db = st_value_pack("{ss}", "id", job->key);
		st_value_hashtable_put(job->db_data, key, false, db, true);
		job_id = strdup(job->key);
	} else {
		struct st_value * db = st_value_hashtable_get(job->db_data, key, false, false);
		st_value_unpack(db, "{ssss}", "id", &job_id, "jobrun id", &jobrun_id);
	}

	st_value_free(key);

	const char * query = "check_if_user_stop_job";
	st_database_postgresql_prepare(self, query, "SELECT status FROM job WHERE id = $1 LIMIT 1");

	const char * param[] = { job_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		const enum st_job_status new_status = st_job_string_to_status(PQgetvalue(result, 0, 0));

		if ((job->status == st_job_status_running || job->status == st_job_status_pause) && new_status == st_job_status_stopped) {
			job->status = new_status;
			job->stopped_by_user = true;
		}
	}
	PQclear(result);

	// update job
	query = "update_job";
	st_database_postgresql_prepare(self, query, "UPDATE job SET status = $1, update = NOW() WHERE id = $2");

	const char * param2[] = { st_job_status_to_string(job->status, false), job_id };
	result = PQexecPrepared(self->connect, query, 2, param2, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);

	PQclear(result);
	free(job_id);

	if (jobrun_id == NULL)
		return status != PGRES_COMMAND_OK;

	// update jobrun
	query = "update_jobrun";
	st_database_postgresql_prepare(self, query, "UPDATE jobrun SET status = $1, done = $2 WHERE id = $3");

	char * done = NULL;
	asprintf(&done, "%f", job->done);

	const char * param3[] = { st_job_status_to_string(job->status, false), done, jobrun_id };
	result = PQexecPrepared(self->connect, query, 3, param3, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);

	PQclear(result);
	free(done);
	free(jobrun_id);

	return status != PGRES_COMMAND_OK;
}

static int st_database_postgresql_sync_jobs(struct st_database_connection * connect, struct st_value * jobs) {
	if (connect == NULL || jobs == NULL)
		return 1;

	char * host_id = st_database_postgresql_get_host(connect);
	if (host_id == NULL)
		return 2;

	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_scheduled_jobs";
	st_database_postgresql_prepare(self, query, "SELECT j.id, j.name, jt.name, nextstart, interval, repetition, metadata, options, COALESCE(MAX(jr.numrun) + 1, 1) FROM job j INNER JOIN jobtype jt ON j.type = jt.id LEFT JOIN jobrun jr ON j.id = jr.job WHERE j.status = 'scheduled' AND host = $1 GROUP BY j.id, j.name, jt.name, nextstart, interval, repetition, metadata::TEXT, options::TEXT");

	const char * param[] = { host_id };

	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		int i;
		for (i = 0; i < nb_result; i++) {
			char * job_id;
			st_database_postgresql_get_string_dup(result, i, 0, &job_id);

			struct st_job * job = NULL;
			bool has_job = st_value_hashtable_has_key2(jobs, job_id);
			if (has_job) {
				struct st_value * vj = st_value_hashtable_get2(jobs, job_id, false, false);
				job = st_value_custom_get(vj);
			} else {
				job = malloc(sizeof(struct st_job));
				bzero(job, sizeof(struct st_job));

				st_database_postgresql_get_string_dup(result, i, 0, &job->key);
				st_database_postgresql_get_string_dup(result, i, 1, &job->name);
				st_database_postgresql_get_string_dup(result, i, 2, &job->type);
				job->meta = st_json_parse_string(PQgetvalue(result, i, 6));
				job->option = st_json_parse_string(PQgetvalue(result, i, 7));
			}

			st_database_postgresql_get_time(result, i, 3, &job->next_start);
			st_database_postgresql_get_long(result, i, 4, &job->interval);
			st_database_postgresql_get_long(result, i, 5, &job->repetition);
			job->status = st_job_status_scheduled;
			st_database_postgresql_get_long(result, i, 8, &job->num_runs);

			if (!has_job) {
				job->db_data = st_value_new_hashtable(st_value_custom_compute_hash);
				struct st_value * key = st_value_new_custom(connect->config, NULL);
				struct st_value * db = st_value_pack("{ss}", "id", job_id);
				st_value_hashtable_put(job->db_data, key, true, db, true);

				struct st_value * vj = st_value_new_custom(job, st_job_free2);
				st_value_hashtable_put2(jobs, job_id, vj, true);
			}

			free(job_id);
		}
	}

	PQclear(result);
	free(host_id);

	return status == PGRES_FATAL_ERROR;
}


static int st_database_postgresql_get_nb_scripts(struct st_database_connection * connect, const char * job_type, enum st_script_type type, struct st_pool * pool) {
	if (connect == NULL || job_type == NULL || pool == NULL)
		return -1;

	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_nb_scripts_with_sequence_and_pool";
	st_database_postgresql_prepare(self, query, "SELECT COUNT(DISTINCT script) FROM scripts ss LEFT JOIN jobtype jt ON ss.jobtype = jt.id AND jt.name = $1 LEFT JOIN pool p ON ss.pool = p.id AND p.uuid = $2 WHERE scripttype = $3");

	const char * param[] = { job_type, pool->uuid, st_script_type_to_string(type, false) };
	PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	int nb_result = -1;

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_database_postgresql_get_int(result, 0, 0, &nb_result);

	PQclear(result);

	return nb_result;
}

static char * st_database_postgresql_get_script(struct st_database_connection * connect, const char * job_type, unsigned int sequence, enum st_script_type type, struct st_pool * pool) {
	if (connect == NULL || job_type == NULL || pool == NULL)
		return NULL;

	struct st_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_script_with_sequence_and_pool";
	st_database_postgresql_prepare(self, query, "SELECT s.path FROM scripts ss LEFT JOIN jobtype jt ON ss.jobtype = jt.id LEFT JOIN script s ON ss.script = s.id AND jt.name = $1 LEFT JOIN pool p ON ss.pool = p.id AND p.uuid = $2 WHERE scripttype = $3 ORDER BY sequence LIMIT 1 OFFSET $4");

	char * seq;
	asprintf(&seq, "%u", sequence);

	const char * param[] = { job_type, st_script_type_to_string(type, false), pool->uuid, seq };
	PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	char * script = NULL;
	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (PQntuples(result) > 0)
		st_database_postgresql_get_string_dup(result, 0, 0, &script);

	PQclear(result);

	free(seq);

	return script;
}


static int st_database_postgresql_sync_plugin_job(struct st_database_connection * connect, const char * job) {
	if (connect == NULL || job == NULL)
		return -1;

	struct st_database_postgresql_connection_private * self = connect->data;
	const char * query = "select_jobtype_by_name";
	st_database_postgresql_prepare(self, query, "SELECT name FROM jobtype WHERE name = $1 LIMIT 1");

	const char * param[] = { job };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	bool found = false;
	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		found = true;

	PQclear(result);

	if (found)
		return 0;

	query = "insert_jobtype";
	st_database_postgresql_prepare(self, query, "INSERT INTO jobtype(name) VALUES ($1)");

	result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_database_postgresql_get_error(result, query);

	PQclear(result);

	return status == PGRES_FATAL_ERROR;
}


static void st_database_postgresql_prepare(struct st_database_postgresql_connection_private * self, const char * statement_name, const char * query) {
	if (!st_value_hashtable_has_key2(self->cached_query, statement_name)) {
		PGresult * prepare = PQprepare(self->connect, statement_name, query, 0, NULL);
		ExecStatusType status = PQresultStatus(prepare);
		if (status == PGRES_FATAL_ERROR) {
			st_database_postgresql_get_error(prepare, statement_name);
			st_log_write2(status == PGRES_COMMAND_OK ? st_log_level_debug : st_log_level_error, st_log_type_plugin_db, gettext("PSQL: new query prepared (%s) => {%s}, status: %s"), statement_name, query, PQresStatus(status));
		} else
			st_value_hashtable_put2(self->cached_query, statement_name, st_value_new_string(query), true);
		PQclear(prepare);
	}
}

