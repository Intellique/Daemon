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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// dgettext
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

#include <libstoriqone/archive.h>
#include <libstoriqone/backup.h>
#include <libstoriqone/changer.h>
#include <libstoriqone/checksum.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/host.h>
#include <libstoriqone/job.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/script.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/time.h>
#include <libstoriqone/value.h>

#include "common.h"

struct so_database_postgresql_connection_private {
	PGconn * connect;
	struct so_value * cached_query;
};

static int so_database_postgresql_close(struct so_database_connection * connect);
static int so_database_postgresql_free(struct so_database_connection * connect);
static bool so_database_postgresql_is_connected(struct so_database_connection * connect);

static int so_database_postgresql_cancel_checkpoint(struct so_database_connection * connect, const char * checkpoint);
static int so_database_postgresql_cancel_transaction(struct so_database_connection * connect);
static int so_database_postgresql_create_checkpoint(struct so_database_connection * connect, const char * checkpoint);
static int so_database_postgresql_finish_transaction(struct so_database_connection * connect);
static int so_database_postgresql_start_transaction(struct so_database_connection * connect);

static int so_database_postgresql_add_host(struct so_database_connection * connect, const char * uuid, const char * name, const char * domaine, const char * description);
static bool so_database_postgresql_find_host(struct so_database_connection * connect, const char * uuid, const char * hostname);
static char * so_database_postgresql_get_host(struct so_database_connection * connect);
static int so_database_postgresql_get_host_by_name(struct so_database_connection * connect, struct so_host * host, const char * name);
static int so_database_postgresql_update_host(struct so_database_connection * connect, const char * uuid);

static int so_database_postgresql_delete_changer(struct so_database_connection * connect, struct so_changer * changer);
static int so_database_postgresql_delete_drive(struct so_database_connection * connect, struct so_drive * drive);
static struct so_value * so_database_postgresql_get_changers(struct so_database_connection * connect);
static struct so_value * so_database_postgresql_get_checksums_from_pool(struct so_database_connection * connect, struct so_pool * pool);
static struct so_value * so_database_postgresql_get_drives_by_changer(struct so_database_connection * connect, const char * changer_id);
static struct so_value * so_database_postgresql_get_free_medias(struct so_database_connection * connect, struct so_media_format * media_format, bool online);
static struct so_media * so_database_postgresql_get_media(struct so_database_connection * connect, const char * medium_serial_number, const char * label, struct so_job * job);
static struct so_value * so_database_postgresql_get_medias_of_pool(struct so_database_connection * connect, struct so_pool * pool);
static struct so_media_format * so_database_postgresql_get_media_format(struct so_database_connection * connect, unsigned int density_code, enum so_media_format_mode mode);
static struct so_pool * so_database_postgresql_get_pool(struct so_database_connection * connect, const char * uuid, struct so_job * job);
static struct so_value * so_database_postgresql_get_pool_by_pool_mirror(struct so_database_connection * connect, struct so_pool * pool);
static struct so_value * so_database_postgresql_get_selected_files_by_job(struct so_database_connection * connect, struct so_job * job);
static struct so_value * so_database_postgresql_get_slot_by_drive(struct so_database_connection * connect, const char * drive_id);
static struct so_value * so_database_postgresql_get_standalone_drives(struct so_database_connection * connect);
static struct so_value * so_database_postgresql_get_vtls(struct so_database_connection * connect, bool new_vtl);
static int so_database_postgresql_sync_changer(struct so_database_connection * connect, struct so_changer * changer, enum so_database_sync_method method);
static int so_database_postgresql_sync_drive(struct so_database_connection * connect, struct so_drive * drive, bool sync_media, enum so_database_sync_method method);
static int so_database_postgresql_sync_media(struct so_database_connection * connect, struct so_media * media, enum so_database_sync_method method);
static int so_database_postgresql_sync_pool(struct so_database_connection * connect, struct so_pool * pool, enum so_database_sync_method method);
static int so_database_postgresql_sync_slots(struct so_database_connection * connect, struct so_slot * slot, enum so_database_sync_method method);
static struct so_value * so_database_postgresql_update_vtl(struct so_database_connection * connect, struct so_changer * vtl);

static int so_database_postgresql_add_job_record(struct so_database_connection * connect, struct so_job * job, enum so_log_level level, enum so_job_record_notif notif, const char * message);
static int so_database_postgresql_add_report(struct so_database_connection * connect, struct so_job * job, struct so_archive * archive, struct so_media * media, const char * data);
static char * so_database_postgresql_get_restore_path(struct so_database_connection * connect, struct so_job * job);
static int so_database_postgresql_start_job(struct so_database_connection * connect, struct so_job * job);
static int so_database_postgresql_stop_job(struct so_database_connection * connect, struct so_job * job);
static int so_database_postgresql_sync_job(struct so_database_connection * connect, struct so_job * job);
static int so_database_postgresql_sync_jobs(struct so_database_connection * connect, struct so_value * jobs);

static int so_database_postgresql_get_nb_scripts(struct so_database_connection * connect, const char * job_type, enum so_script_type type, struct so_pool * pool);
static char * so_database_postgresql_get_script(struct so_database_connection * connect, const char * job_type, unsigned int sequence, enum so_script_type type, struct so_pool * pool);

static bool so_database_postgresql_find_plugin_checksum(struct so_database_connection * connect, const char * checksum);
static int so_database_postgresql_sync_plugin_checksum(struct so_database_connection * connect, struct so_checksum_driver * driver);
static int so_database_postgresql_sync_plugin_job(struct so_database_connection * connect, const char * job);

static int so_database_postgresql_check_archive_file(struct so_database_connection * connect, struct so_archive * archive, struct so_archive_file * file);
static int so_database_postgresql_check_archive_volume(struct so_database_connection * connect, struct so_archive_volume * volume);
static struct so_archive * so_database_postgresql_get_archive_by_id(struct so_database_connection * connect, const char * archive_id);
static struct so_archive * so_database_postgresql_get_archive_by_job(struct so_database_connection * connect, struct so_job * job);
static struct so_value * so_database_postgresql_get_archive_by_media(struct so_database_connection * connect, struct so_media * media);
static int so_database_postgresql_link_archives(struct so_database_connection * connect, struct so_archive * source, struct so_archive * copy);
static int so_database_postgresql_mark_archive_as_purged(struct so_database_connection * connect, struct so_media * media, struct so_job * job);
static unsigned int so_database_postgresql_get_nb_volumes_of_file(struct so_database_connection * connect, struct so_archive * archive, struct so_archive_file * file);
static int so_database_postgresql_sync_archive(struct so_database_connection * connect, struct so_archive * archive);
static int so_database_postgresql_sync_archive_file(struct so_database_connection * connect, struct so_archive_file * file, char ** file_id);
static int so_database_postgresql_sync_archive_volume(struct so_database_connection * connect, char * archive_id, struct so_archive_volume * volume);

static int so_database_postgresql_backup_add(struct so_database_connection * connect, struct so_backup * backup);
static struct so_backup * so_database_postgresql_get_backup(struct so_database_connection * connect, struct so_job * job);
static int so_database_postgresql_mark_backup_volume_checked(struct so_database_connection * connect, struct so_backup_volume * volume);

static int so_database_postgresql_checksumresult_add(struct so_database_connection * connect, const char * checksum, const char * digest, char ** digest_id);

static char * so_database_postgresql_find_selected_path(struct so_database_connection * connect, const char * selected_path);

static void so_database_postgresql_prepare(struct so_database_postgresql_connection_private * self, const char * statement_name, const char * query);

static struct so_database_connection_ops so_database_postgresql_connection_ops = {
	.close        = so_database_postgresql_close,
	.free         = so_database_postgresql_free,
	.is_connected = so_database_postgresql_is_connected,

	.cancel_transaction = so_database_postgresql_cancel_transaction,
	.finish_transaction = so_database_postgresql_finish_transaction,
	.start_transaction  = so_database_postgresql_start_transaction,

	.add_host         = so_database_postgresql_add_host,
	.find_host        = so_database_postgresql_find_host,
	.get_host_by_name = so_database_postgresql_get_host_by_name,
	.update_host      = so_database_postgresql_update_host,

	.delete_changer            = so_database_postgresql_delete_changer,
	.delete_drive              = so_database_postgresql_delete_drive,
	.get_changers              = so_database_postgresql_get_changers,
	.get_checksums_from_pool   = so_database_postgresql_get_checksums_from_pool,
	.get_free_medias           = so_database_postgresql_get_free_medias,
	.get_media                 = so_database_postgresql_get_media,
	.get_media_format          = so_database_postgresql_get_media_format,
	.get_medias_of_pool        = so_database_postgresql_get_medias_of_pool,
	.get_pool                  = so_database_postgresql_get_pool,
	.get_pool_by_pool_mirror   = so_database_postgresql_get_pool_by_pool_mirror,
	.get_selected_files_by_job = so_database_postgresql_get_selected_files_by_job,
	.get_standalone_drives     = so_database_postgresql_get_standalone_drives,
	.get_vtls                  = so_database_postgresql_get_vtls,
	.sync_changer              = so_database_postgresql_sync_changer,
	.sync_drive                = so_database_postgresql_sync_drive,
	.sync_media                = so_database_postgresql_sync_media,
	.update_vtl                = so_database_postgresql_update_vtl,

	.add_job_record   = so_database_postgresql_add_job_record,
	.add_report       = so_database_postgresql_add_report,
	.get_restore_path = so_database_postgresql_get_restore_path,
	.start_job        = so_database_postgresql_start_job,
	.stop_job         = so_database_postgresql_stop_job,
	.sync_job         = so_database_postgresql_sync_job,
	.sync_jobs        = so_database_postgresql_sync_jobs,

	.get_nb_scripts = so_database_postgresql_get_nb_scripts,
	.get_script     = so_database_postgresql_get_script,

	.find_plugin_checksum = so_database_postgresql_find_plugin_checksum,
	.sync_plugin_checksum = so_database_postgresql_sync_plugin_checksum,
	.sync_plugin_job      = so_database_postgresql_sync_plugin_job,

	.check_archive_file     = so_database_postgresql_check_archive_file,
	.check_archive_volume   = so_database_postgresql_check_archive_volume,
	.get_archive_by_job     = so_database_postgresql_get_archive_by_job,
	.get_archive_by_media   = so_database_postgresql_get_archive_by_media,
	.get_nb_volumes_of_file = so_database_postgresql_get_nb_volumes_of_file,
	.link_archives          = so_database_postgresql_link_archives,
	.mark_archive_as_purged = so_database_postgresql_mark_archive_as_purged,
	.sync_archive           = so_database_postgresql_sync_archive,

	.backup_add                 = so_database_postgresql_backup_add,
	.get_backup                 = so_database_postgresql_get_backup,
	.mark_backup_volume_checked = so_database_postgresql_mark_backup_volume_checked,
};


struct so_database_connection * so_database_postgresql_connect_init(PGconn * pg_connect) {
	PQsetErrorVerbosity(pg_connect, PQERRORS_VERBOSE);

	struct so_database_postgresql_connection_private * self = malloc(sizeof(struct so_database_postgresql_connection_private));
	self->connect = pg_connect;
	self->cached_query = so_value_new_hashtable2();

	struct so_database_connection * connect = malloc(sizeof(struct so_database_connection));
	connect->data = self;
	connect->ops = &so_database_postgresql_connection_ops;

	return connect;
}


static int so_database_postgresql_close(struct so_database_connection * connect) {
	struct so_database_postgresql_connection_private * self = connect->data;

	if (self->connect != NULL)
		PQfinish(self->connect);
	self->connect = NULL;

	if (self->cached_query != NULL)
		so_value_free(self->cached_query);
	self->cached_query = NULL;

	return 0;
}

static int so_database_postgresql_free(struct so_database_connection * connect) {
	so_database_postgresql_close(connect);
	free(connect->data);

	connect->data = NULL;
	connect->driver = NULL;
	connect->config = NULL;
	free(connect);

	return 0;
}

static bool so_database_postgresql_is_connected(struct so_database_connection * connect) {
	struct so_database_postgresql_connection_private * self = connect->data;

	if (self->connect == NULL)
		return false;

	return PQstatus(self->connect) != CONNECTION_OK;
}


static int so_database_postgresql_cancel_checkpoint(struct so_database_connection * connect, const char * checkpoint) {
	if (connect == NULL || checkpoint == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;
	char * query = NULL;
	asprintf(&query, "ROLLBACK TO %s", checkpoint);

	PGresult * result = PQexec(self->connect, query);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, NULL);

	if (status != PGRES_COMMAND_OK)
		so_log_write2(so_log_level_error, so_log_type_plugin_db, dgettext("libstoriqone-database-postgresql", "PSQL: error while rolling back a savepoint => %s"), PQerrorMessage(self->connect));

	PQclear(result);
	free(query);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

static int so_database_postgresql_cancel_transaction(struct so_database_connection * connect) {
	if (!connect)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;

	PGTransactionStatusType status = PQtransactionStatus(self->connect);
	switch (status) {
		case PQTRANS_INERROR:
		case PQTRANS_INTRANS: {
			PGresult * result = PQexec(self->connect, "ROLLBACK");
			ExecStatusType roll_back_status = PQresultStatus(result);
			if (roll_back_status != PGRES_COMMAND_OK)
				so_database_postgresql_get_error(result, NULL);
			PQclear(result);
			return roll_back_status != PGRES_COMMAND_OK;
		}
		break;

		default:
			return 0;
	}
}

static int so_database_postgresql_create_checkpoint(struct so_database_connection * connect, const char * checkpoint) {
	if (connect == NULL || checkpoint == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType transStatus = PQtransactionStatus(self->connect);

	if (transStatus == PQTRANS_INERROR) {
		so_log_write2(so_log_level_error, so_log_type_plugin_db, dgettext("libstoriqone-database-postgresql", "PSQL: Can't create checkpoint because there is an error into current transaction"));
		return -1;
	} else if (transStatus == PQTRANS_IDLE) {
		so_log_write2(so_log_level_error, so_log_type_plugin_db, dgettext("libstoriqone-database-postgresql", "PSQL: Can't create checkpoint because there is no active transaction"));
		return -1;
	}

	char * query = NULL;
	asprintf(&query, "SAVEPOINT %s", checkpoint);

	PGresult * result = PQexec(self->connect, query);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, NULL);

	if (status != PGRES_COMMAND_OK)
		so_log_write2(so_log_level_error, so_log_type_plugin_db, dgettext("libstoriqone-database-postgresql", "PSQL: error while creating a savepoint => %s"), PQerrorMessage(self->connect));

	PQclear(result);
	free(query);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

static int so_database_postgresql_finish_transaction(struct so_database_connection * connect) {
	if (connect == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;

	PGTransactionStatusType status = PQtransactionStatus(self->connect);
	switch (status) {
		case PQTRANS_INERROR: {
			so_log_write2(so_log_level_error, so_log_type_plugin_db, dgettext("libstoriqone-database-postgresql", "PSQL: Rolling back transaction because current transaction is in error"));

			PGresult * result = PQexec(self->connect, "ROLL BACK");
			PQclear(result);
			return 1;
		}
		break;

		case PQTRANS_INTRANS: {
			PGresult * result = PQexec(self->connect, "COMMIT");
			ExecStatusType commit_status = PQresultStatus(result);
			if (commit_status != PGRES_COMMAND_OK)
				so_database_postgresql_get_error(result, NULL);
			PQclear(result);
			return commit_status != PGRES_COMMAND_OK;
		}
		break;

		default:
			return 0;
	}
}

static int so_database_postgresql_start_transaction(struct so_database_connection * connect) {
	if (connect == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType status = PQtransactionStatus(self->connect);
	switch (status) {
		case PQTRANS_IDLE: {
			PGresult * result = PQexec(self->connect, "BEGIN");
			ExecStatusType commit_status = PQresultStatus(result);
			if (commit_status != PGRES_COMMAND_OK)
				so_database_postgresql_get_error(result, NULL);
			PQclear(result);
			return commit_status != PGRES_COMMAND_OK;
		}
		break;

		default:
			return 1;
	}
}


static int so_database_postgresql_add_host(struct so_database_connection * connect, const char * uuid, const char * name, const char * domaine, const char * description) {
	if (connect == NULL || uuid == NULL || name == NULL)
		return 1;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "insert_host";
	so_database_postgresql_prepare(self, query, "INSERT INTO host(uuid, name, domaine, description) VALUES ($1, $2, $3, $4)");

	const char * param[] = { uuid, name, domaine, description };
	PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	PQclear(result);

	return status == PGRES_FATAL_ERROR;
}

static bool so_database_postgresql_find_host(struct so_database_connection * connect, const char * uuid, const char * hostname) {
	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_host_by_name_or_uuid";
	so_database_postgresql_prepare(self, query, "SELECT id FROM host WHERE uuid::TEXT = $1 OR name = $2 OR name || '.' || domaine = $2 LIMIT 1");

	const char * param[] = { uuid, hostname };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	bool found = false;
	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (PQntuples(result) == 1)
		found = true;

	PQclear(result);

	return found;
}

static char * so_database_postgresql_get_host(struct so_database_connection * connect) {
	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_host_by_name";
	so_database_postgresql_prepare(self, query, "SELECT id FROM host WHERE name = $1 OR name || '.' || domaine = $1 LIMIT 1");

	struct utsname name;
	uname(&name);

	const char * param[] = { name.nodename };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	char * hostid = NULL;

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
		so_database_postgresql_get_string_dup(result, 0, 0, &hostid);
	else
		so_log_write2(so_log_level_error, so_log_type_plugin_db, dgettext("libstoriqone-database-postgresql", "PSQL: Host not found into database (%s)"), name.nodename);

	PQclear(result);

	return hostid;
}

static int so_database_postgresql_get_host_by_name(struct so_database_connection * connect, struct so_host * host, const char * name) {
	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_host_by_name_by_name";
	so_database_postgresql_prepare(self, query, "SELECT CASE WHEN domaine IS NULL THEN name ELSE name || '.' || domaine END, uuid FROM host WHERE name = $1 OR name || '.' || domaine = $1 LIMIT 1");

	const char * param[] = { name };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		free(host->hostname);
		free(host->uuid);
		host->hostname = host->uuid = NULL;

		so_database_postgresql_get_string_dup(result, 0, 0, &host->hostname);
		so_database_postgresql_get_string_dup(result, 0, 1, &host->uuid);
	}

	PQclear(result);

	return status != PGRES_TUPLES_OK;
}

static int so_database_postgresql_update_host(struct so_database_connection * connect, const char * uuid) {
	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "update_host_by_uuid";
	so_database_postgresql_prepare(self, query, "UPDATE host SET updated = NOW() WHERE uuid = $1");

	const char * param[] = { uuid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	PQclear(result);

	return status != PGRES_COMMAND_OK;
}


static int so_database_postgresql_delete_changer(struct so_database_connection * connect, struct so_changer * changer) {
	if (connect == NULL || changer == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = so_value_hashtable_get(changer->db_data, key, false, false);
	so_value_free(key);

	char * changer_id = NULL;
	so_value_unpack(db, "{ss}", "id", &changer_id);

	const char * query = "delete_changer_by_id";
	so_database_postgresql_prepare(self, query, "DELETE FROM changer WHERE id = $1");

	const char * param[] = { changer_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	PQclear(result);
	free(changer_id);

	return status != PGRES_COMMAND_OK;
}

static int so_database_postgresql_delete_drive(struct so_database_connection * connect, struct so_drive * drive) {
	if (connect == NULL || drive == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = so_value_hashtable_get(drive->db_data, key, false, false);
	so_value_free(key);

	char * drive_id = NULL;
	so_value_unpack(db, "{ss}", "id", &drive_id);

	const char * query = "delete_drive_by_id";
	so_database_postgresql_prepare(self, query, "DELETE FROM drive WHERE id = $1");

	const char * param[] = { drive_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	PQclear(result);
	free(drive_id);

	return status != PGRES_COMMAND_OK;
}

static struct so_value * so_database_postgresql_get_changers(struct so_database_connection * connect) {
	struct so_value * changers = so_value_new_linked_list();

	char * host_id = so_database_postgresql_get_host(connect);
	if (host_id == NULL)
		return changers;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_real_changer_by_host";
	so_database_postgresql_prepare(self, query, "SELECT DISTINCT c.id, c.model, c.vendor, c.firmwarerev, c.serialnumber, c.wwn, c.barcode, c.status, c.isonline, c.action, c.enable FROM changer c LEFT JOIN drive d ON c.id = d.changer AND c.serialnumber != d.serialnumber WHERE c.host = $1 AND c.serialnumber NOT IN (SELECT uuid::TEXT FROM vtl WHERE host = $1)");

	const char * param[] = { host_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		int i;
		for (i = 0; i < nb_result; i++) {
			char * changer_id;
			so_database_postgresql_get_string_dup(result, i, 0, &changer_id);

			bool barcode = false, isonline = false, enabled = false;
			so_database_postgresql_get_bool(result, i, 6, &barcode);
			so_database_postgresql_get_bool(result, i, 8, &isonline);
			so_database_postgresql_get_bool(result, i, 10, &enabled);
			enum so_changer_status changer_status = so_database_postgresql_string_to_status(PQgetvalue(result, i, 7));
			enum so_changer_action changer_action = so_database_postgresql_string_to_action(PQgetvalue(result, i, 9));

			struct so_value * changer = so_value_pack("{sssssssssssbsosssbsssb}",
					"model", PQgetvalue(result, i, 1),
					"vendor", PQgetvalue(result, i, 2),
					"firmwarerev", PQgetvalue(result, i, 3),
					"serial number", PQgetvalue(result, i, 4),
					"wwn", PQgetvalue(result, i, 5),
					"barcode", barcode,
					"drives", so_database_postgresql_get_drives_by_changer(connect, changer_id),
					"status", so_changer_status_to_string(changer_status, false),
					"is online", isonline,
					"action", so_changer_action_to_string(changer_action, false),
					"enable", enabled
			);

			so_value_list_push(changers, changer, true);

			free(changer_id);
		}
	}

	PQclear(result);
	free(host_id);

	return changers;
}

static struct so_value * so_database_postgresql_get_checksums_from_pool(struct so_database_connection * connect, struct so_pool * pool) {
	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_checksums_by_pools";
	so_database_postgresql_prepare(self, query, "SELECT c.name FROM checksum c INNER JOIN pooltochecksum pc ON c.id = pc.checksum INNER JOIN pool p ON pc.pool = p.id AND p.uuid::TEXT = $1");

	const char * param[] = { pool->uuid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	struct so_value * checksums = so_value_new_linked_list();

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		int i;
		for (i = 0; i < nb_result; i++)
			so_value_list_push(checksums, so_value_new_string(PQgetvalue(result, i, 0)), true);
	}

	PQclear(result);

	return checksums;
}

static struct so_value * so_database_postgresql_get_drives_by_changer(struct so_database_connection * connect, const char * changer_id) {
	struct so_value * drives = so_value_new_linked_list();

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_drives_by_changer";
	so_database_postgresql_prepare(self, query, "SELECT id, model, vendor, firmwarerev, serialnumber, enable FROM drive WHERE changer = $1");

	const char * param[] = { changer_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		int i;
		for (i = 0; i < nb_result; i++) {
			char * drive_id = PQgetvalue(result, i, 0);
			bool enabled = false;
			so_database_postgresql_get_bool(result, i, 5, &enabled);

			struct so_value * drive = so_value_pack("{sssssssssbso}",
					"model", PQgetvalue(result, i, 1),
					"vendor", PQgetvalue(result, i, 2),
					"firmware revision", PQgetvalue(result, i, 3),
					"serial number", PQgetvalue(result, i, 4),
					"enable", enabled,
					"slot", so_database_postgresql_get_slot_by_drive(connect, drive_id)
			);

			so_value_list_push(drives, drive, true);
		}
	}

	PQclear(result);

	return drives;
}

static struct so_value * so_database_postgresql_get_free_medias(struct so_database_connection * connect, struct so_media_format * media_format, bool online) {
	if (connect == NULL || media_format == NULL)
		return NULL;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query;

	if (online) {
		query = "select_online_medias";
		so_database_postgresql_prepare(self, query, "SELECT m.id, mf.id, m.uuid, m.label, m.mediumserialnumber, m.name, m.status, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, operationcount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, m.blocksize, freeblock, totalblock, append, m.type, writelock, nbfiles, densitycode, mode, p.uuid FROM media m INNER JOIN mediaformat mf ON m.mediaformat = mf.id LEFT JOIN pool p ON m.pool = p.id WHERE m.id IN (SELECT media FROM changerslot) AND mf.densitycode = $1 AND mf.mode = $2 ORDER BY m.label");

	} else {
		query = "select_offline_medias";
		so_database_postgresql_prepare(self, query, "SELECT m.id, mf.id, m.uuid, m.label, m.mediumserialnumber, m.name, m.status, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, operationcount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, m.blocksize, freeblock, totalblock, append, m.type, writelock, nbfiles, densitycode, mode, p.uuid FROM media m INNER JOIN mediaformat mf ON m.mediaformat = mf.id LEFT JOIN pool p ON m.pool = p.id WHERE m.id NOT IN (SELECT media FROM changerslot) AND mf.densitycode = $1 AND mf.mode = $2 ORDER BY m.label");
	}

	char * density_code;
	asprintf(&density_code, "%d", media_format->density_code);

	const char * param[] = { density_code, so_media_format_mode_to_string(media_format->mode, false) };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	struct so_value * medias = so_value_new_null();

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		medias = so_value_new_array(nb_result);

		int i;
		for (i = 0; i < nb_result; i++) {
			struct so_media * media = malloc(sizeof(struct so_media));
			bzero(media, sizeof(struct so_media));

			media->db_data = so_value_new_hashtable(so_value_custom_compute_hash);
			struct so_value * key = so_value_new_custom(connect->config, NULL);
			struct so_value * db = so_value_new_hashtable2();
			so_value_hashtable_put(media->db_data, key, true, db, true);

			so_value_hashtable_put2(db, "id", so_value_new_string(PQgetvalue(result, i, 0)), true);
			so_value_hashtable_put2(db, "media format id", so_value_new_string(PQgetvalue(result, i, 1)), true);

			so_database_postgresql_get_string(result, i, 2, media->uuid, 37);
			so_database_postgresql_get_string_dup(result, i, 3, &media->label);
			so_database_postgresql_get_string_dup(result, i, 4, &media->medium_serial_number);
			so_database_postgresql_get_string_dup(result, i, 5, &media->name);

			media->status = so_media_string_to_status(PQgetvalue(result, i, 6), false);

			so_database_postgresql_get_time(result, i, 7, &media->first_used);
			so_database_postgresql_get_time(result, i, 8, &media->use_before);
			if (!PQgetisnull(result, i, 9))
				so_database_postgresql_get_time(result, i, 9, &media->last_read);
			if (!PQgetisnull(result, i, 10))
				so_database_postgresql_get_time(result, i, 10, &media->last_write);

			so_database_postgresql_get_long(result, i, 11, &media->load_count);
			so_database_postgresql_get_long(result, i, 12, &media->read_count);
			so_database_postgresql_get_long(result, i, 13, &media->write_count);
			so_database_postgresql_get_long(result, i, 14, &media->operation_count);

			so_database_postgresql_get_long(result, i, 15, &media->nb_total_read);
			so_database_postgresql_get_long(result, i, 16, &media->nb_total_write);

			so_database_postgresql_get_uint(result, i, 17, &media->nb_read_errors);
			so_database_postgresql_get_uint(result, i, 18, &media->nb_write_errors);

			so_database_postgresql_get_size(result, i, 19, &media->block_size);
			so_database_postgresql_get_size(result, i, 20, &media->free_block);
			so_database_postgresql_get_size(result, i, 21, &media->total_block);

			so_database_postgresql_get_bool(result, i, 22, &media->append);
			media->type = so_media_string_to_type(PQgetvalue(result, i, 23), false);
			so_database_postgresql_get_bool(result, i, 24, &media->write_lock);
			so_database_postgresql_get_uint(result, i, 25, &media->nb_volumes);

			unsigned char density_code;
			so_database_postgresql_get_uchar(result, i, 26, &density_code);
			enum so_media_format_mode mode = so_media_string_to_format_mode(PQgetvalue(result, i, 27), false);
			media->format = so_database_postgresql_get_media_format(connect, density_code, mode);

			if (!PQgetisnull(result, i, 28))
				media->pool = so_database_postgresql_get_pool(connect, PQgetvalue(result, i, 28), NULL);

			so_value_list_push(medias, so_value_new_custom(media, so_media_free2), true);
		}
	}

	PQclear(result);

	return medias;
}

static struct so_media * so_database_postgresql_get_media(struct so_database_connection * connect, const char * medium_serial_number, const char * label, struct so_job * job) {
	if (connect == NULL || (medium_serial_number == NULL && label == NULL && job == NULL))
		return NULL;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query;
	PGresult * result;

	char * job_id = NULL;

	if (medium_serial_number != NULL) {
		query = "select_media_by_medium_serial_number";
		so_database_postgresql_prepare(self, query, "SELECT m.id, mf.id, m.uuid, label, mediumserialnumber, m.name, m.status, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, operationcount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, m.blocksize, freeblock, totalblock, append, m.type, writelock, nbfiles, densitycode, mode, p.uuid FROM media m LEFT JOIN mediaformat mf ON m.mediaformat = mf.id LEFT JOIN pool p ON m.pool = p.id WHERE mediumserialnumber = $1 LIMIT 1");

		const char * param[] = { medium_serial_number };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	} else if (label != NULL) {
		query = "select_media_by_label";
		so_database_postgresql_prepare(self, query, "SELECT m.id, mf.id, m.uuid, label, mediumserialnumber, m.name, m.status, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, operationcount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, m.blocksize, freeblock, totalblock, append, m.type, writelock, nbfiles, densitycode, mode, p.uuid FROM media m LEFT JOIN mediaformat mf ON m.mediaformat = mf.id LEFT JOIN pool p ON m.pool = p.id WHERE label = $1 LIMIT 1");

		const char * param[] = { label };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	} else {
		query = "select_media_by_job";
		so_database_postgresql_prepare(self, query, "SELECT m.id, mf.id, m.uuid, label, mediumserialnumber, m.name, m.status, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, operationcount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, m.blocksize, freeblock, totalblock, append, m.type, writelock, nbfiles, densitycode, mode, p.uuid FROM job j LEFT JOIN media m ON j.media = m.id LEFT JOIN mediaformat mf ON m.mediaformat = mf.id LEFT JOIN pool p ON m.pool = p.id WHERE j.id = $1 LIMIT 1");

		struct so_value * key = so_value_new_custom(connect->config, NULL);
		struct so_value * job_data = so_value_hashtable_get(job->db_data, key, false, false);
		so_value_unpack(job_data, "{ss}", "id", &job_id);
		so_value_free(key);

		const char * param[] = { job_id };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	}

	struct so_media * media = NULL;

	ExecStatusType status = PQresultStatus(result);
	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		media = malloc(sizeof(struct so_media));
		bzero(media, sizeof(struct so_media));

		media->db_data = so_value_new_hashtable(so_value_custom_compute_hash);
		struct so_value * key = so_value_new_custom(connect->config, NULL);
		struct so_value * db = so_value_new_hashtable2();
		so_value_hashtable_put(media->db_data, key, true, db, true);

		so_value_hashtable_put2(db, "id", so_value_new_string(PQgetvalue(result, 0, 0)), true);
		so_value_hashtable_put2(db, "media format id", so_value_new_string(PQgetvalue(result, 0, 1)), true);

		so_database_postgresql_get_string(result, 0, 2, media->uuid, 37);
		so_database_postgresql_get_string_dup(result, 0, 3, &media->label);
		so_database_postgresql_get_string_dup(result, 0, 4, &media->medium_serial_number);
		so_database_postgresql_get_string_dup(result, 0, 5, &media->name);

		media->status = so_media_string_to_status(PQgetvalue(result, 0, 6), false);

		so_database_postgresql_get_time(result, 0, 7, &media->first_used);
		so_database_postgresql_get_time(result, 0, 8, &media->use_before);
		if (!PQgetisnull(result, 0, 9))
			so_database_postgresql_get_time(result, 0, 9, &media->last_read);
		if (!PQgetisnull(result, 0, 10))
			so_database_postgresql_get_time(result, 0, 10, &media->last_write);

		so_database_postgresql_get_long(result, 0, 11, &media->load_count);
		so_database_postgresql_get_long(result, 0, 12, &media->read_count);
		so_database_postgresql_get_long(result, 0, 13, &media->write_count);
		so_database_postgresql_get_long(result, 0, 14, &media->operation_count);

		so_database_postgresql_get_long(result, 0, 15, &media->nb_total_read);
		so_database_postgresql_get_long(result, 0, 16, &media->nb_total_write);

		so_database_postgresql_get_uint(result, 0, 17, &media->nb_read_errors);
		so_database_postgresql_get_uint(result, 0, 18, &media->nb_write_errors);

		so_database_postgresql_get_size(result, 0, 19, &media->block_size);
		so_database_postgresql_get_size(result, 0, 20, &media->free_block);
		so_database_postgresql_get_size(result, 0, 21, &media->total_block);

		so_database_postgresql_get_bool(result, 0, 22, &media->append);
		media->type = so_media_string_to_type(PQgetvalue(result, 0, 23), false);
		so_database_postgresql_get_bool(result, 0, 24, &media->write_lock);
		so_database_postgresql_get_uint(result, 0, 25, &media->nb_volumes);

		unsigned char density_code;
		so_database_postgresql_get_uchar(result, 0, 26, &density_code);
		enum so_media_format_mode mode = so_media_string_to_format_mode(PQgetvalue(result, 0, 27), false);
		media->format = so_database_postgresql_get_media_format(connect, density_code, mode);

		if (!PQgetisnull(result, 0, 28))
			media->pool = so_database_postgresql_get_pool(connect, PQgetvalue(result, 0, 28), NULL);
	}

	PQclear(result);
	free(job_id);
	return media;
}

static struct so_value * so_database_postgresql_get_medias_of_pool(struct so_database_connection * connect, struct so_pool * pool) {
	if (connect == NULL || pool == NULL)
		return NULL;

	struct so_database_postgresql_connection_private * self = connect->data;
	const char * query = "select_medias_of_pools";
	so_database_postgresql_prepare(self, query, "SELECT m.id, mf.id, m.uuid, label, mediumserialnumber, m.name, m.status, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, operationcount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, m.blocksize, freeblock, totalblock, append, m.type, writelock, nbfiles, densitycode, mode, p.uuid FROM media m LEFT JOIN mediaformat mf ON m.mediaformat = mf.id LEFT JOIN pool p ON m.pool = p.id WHERE p.uuid = $1 ORDER BY m.label");

	const char * param[] = { pool->uuid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	struct so_value * medias = so_value_new_null();

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		medias = so_value_new_array(nb_result);

		int i;
		for (i = 0; i < nb_result; i++) {
			struct so_media * media = malloc(sizeof(struct so_media));
			bzero(media, sizeof(struct so_media));

			media->db_data = so_value_new_hashtable(so_value_custom_compute_hash);
			struct so_value * key = so_value_new_custom(connect->config, NULL);
			struct so_value * db = so_value_new_hashtable2();
			so_value_hashtable_put(media->db_data, key, true, db, true);

			so_value_hashtable_put2(db, "id", so_value_new_string(PQgetvalue(result, i, 0)), true);
			so_value_hashtable_put2(db, "media format id", so_value_new_string(PQgetvalue(result, i, 1)), true);

			so_database_postgresql_get_string(result, i, 2, media->uuid, 37);
			so_database_postgresql_get_string_dup(result, i, 3, &media->label);
			so_database_postgresql_get_string_dup(result, i, 4, &media->medium_serial_number);
			so_database_postgresql_get_string_dup(result, i, 5, &media->name);

			media->status = so_media_string_to_status(PQgetvalue(result, i, 6), false);

			so_database_postgresql_get_time(result, i, 7, &media->first_used);
			so_database_postgresql_get_time(result, i, 8, &media->use_before);
			if (!PQgetisnull(result, i, 9))
				so_database_postgresql_get_time(result, i, 9, &media->last_read);
			if (!PQgetisnull(result, i, 10))
				so_database_postgresql_get_time(result, i, 10, &media->last_write);

			so_database_postgresql_get_long(result, i, 11, &media->load_count);
			so_database_postgresql_get_long(result, i, 12, &media->read_count);
			so_database_postgresql_get_long(result, i, 13, &media->write_count);
			so_database_postgresql_get_long(result, i, 14, &media->operation_count);

			so_database_postgresql_get_long(result, i, 15, &media->nb_total_read);
			so_database_postgresql_get_long(result, i, 16, &media->nb_total_write);

			so_database_postgresql_get_uint(result, i, 17, &media->nb_read_errors);
			so_database_postgresql_get_uint(result, i, 18, &media->nb_write_errors);

			so_database_postgresql_get_size(result, i, 19, &media->block_size);
			so_database_postgresql_get_size(result, i, 20, &media->free_block);
			so_database_postgresql_get_size(result, i, 21, &media->total_block);

			so_database_postgresql_get_bool(result, i, 22, &media->append);
			media->type = so_media_string_to_type(PQgetvalue(result, i, 23), false);
			so_database_postgresql_get_bool(result, i, 24, &media->write_lock);
			so_database_postgresql_get_uint(result, i, 25, &media->nb_volumes);

			unsigned char density_code;
			so_database_postgresql_get_uchar(result, i, 26, &density_code);
			enum so_media_format_mode mode = so_media_string_to_format_mode(PQgetvalue(result, i, 27), false);
			media->format = so_database_postgresql_get_media_format(connect, density_code, mode);

			if (!PQgetisnull(result, i, 28))
				media->pool = so_database_postgresql_get_pool(connect, PQgetvalue(result, i, 28), NULL);

			so_value_list_push(medias, so_value_new_custom(media, so_media_free2), true);
		}
	}

	PQclear(result);

	return medias;
}

static struct so_media_format * so_database_postgresql_get_media_format(struct so_database_connection * connect, unsigned int density_code, enum so_media_format_mode mode) {
	if (connect == NULL || density_code == 0 || mode == so_media_format_mode_unknown)
		return NULL;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_media_format_by_density_code";
	so_database_postgresql_prepare(self, query, "SELECT id, name, datatype, maxloadcount, maxreadcount, maxwritecount, maxopcount, EXTRACT('epoch' FROM lifespan), capacity, blocksize, densitycode, supportpartition, supportmam FROM mediaformat WHERE densitycode = $1 AND mode = $2 LIMIT 1");

	char * c_density_code = NULL;
	asprintf(&c_density_code, "%d", density_code);

	const char * param[] = { c_density_code, so_media_format_mode_to_string(mode, false) };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	free(c_density_code);

	struct so_media_format * format = NULL;

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		format = malloc(sizeof(struct so_media_format));
		bzero(format, sizeof(struct so_media_format));

		struct so_value * key = so_value_new_custom(connect->config, NULL);
		format->db_data = so_value_new_hashtable(so_value_custom_compute_hash);
		so_value_hashtable_put(format->db_data, key, true, so_value_pack("{ss}", "id", PQgetvalue(result, 0, 0)), true);

		so_database_postgresql_get_string(result, 0, 1, format->name, 64);
		format->density_code = density_code;
		format->type = so_media_string_to_format_data_type(PQgetvalue(result, 0, 2), false);
		format->mode = mode;

		so_database_postgresql_get_long(result, 0, 3, &format->max_load_count);
		so_database_postgresql_get_long(result, 0, 4, &format->max_read_count);
		so_database_postgresql_get_long(result, 0, 5, &format->max_write_count);
		so_database_postgresql_get_long(result, 0, 6, &format->max_operation_count);

		so_database_postgresql_get_long(result, 0, 7, &format->life_span);

		so_database_postgresql_get_ssize(result, 0, 8, &format->capacity);
		so_database_postgresql_get_ssize(result, 0, 9, &format->block_size);
		so_database_postgresql_get_uchar(result, 0, 10, &format->density_code);

		so_database_postgresql_get_bool(result, 0, 11, &format->support_partition);
		so_database_postgresql_get_bool(result, 0, 12, &format->support_mam);
	}

	PQclear(result);

	return format;
}

static struct so_pool * so_database_postgresql_get_pool(struct so_database_connection * connect, const char * uuid, struct so_job * job) {
	if (connect == NULL || (uuid == NULL && job == NULL))
		return NULL;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query;
	PGresult * result;

	if (uuid != NULL) {
		query = "select_pool_by_uuid";
		so_database_postgresql_prepare(self, query, "SELECT p.id, uuid, p.name, autocheck, growable, unbreakablelevel, rewritable, deleted, densitycode, mode FROM pool p LEFT JOIN mediaformat mf ON p.mediaformat = mf.id WHERE uuid = $1 LIMIT 1");

		const char * param[] = { uuid };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	} else {
		query = "select_pool_by_job";
		so_database_postgresql_prepare(self, query, "SELECT p.id, uuid, p.name, autocheck, growable, unbreakablelevel, rewritable, deleted, densitycode, mode FROM job j INNER JOIN pool p ON j.pool = p.id LEFT JOIN mediaformat mf ON p.mediaformat = mf.id WHERE j.id = $1 LIMIT 1");

		char * job_id = NULL;

		struct so_value * key = so_value_new_custom(connect->config, NULL);
		struct so_value * job_data = so_value_hashtable_get(job->db_data, key, false, false);
		so_value_unpack(job_data, "{ss}", "id", &job_id);
		so_value_free(key);

		const char * param[] = { job_id };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);

		free(job_id);
	}

	struct so_pool * pool = NULL;

	ExecStatusType status = PQresultStatus(result);
	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		pool = malloc(sizeof(struct so_pool));
		bzero(pool, sizeof(struct so_pool));

		struct so_value * key = so_value_new_custom(connect->config, NULL);
		struct so_value * db = so_value_new_hashtable2();
		pool->db_data = so_value_new_hashtable(so_value_custom_compute_hash);
		so_value_hashtable_put2(db, "id", so_value_new_string(PQgetvalue(result, 0, 0)), true);
		so_value_hashtable_put(pool->db_data, key, true, db, true);

		so_database_postgresql_get_string(result, 0, 1, pool->uuid, 37);
		so_database_postgresql_get_string_dup(result, 0, 2, &pool->name);
		pool->auto_check = so_pool_string_to_autocheck_mode(PQgetvalue(result, 0, 3), false);
		so_database_postgresql_get_bool(result, 0, 4, &pool->growable);
		pool->unbreakable_level = so_pool_string_to_unbreakable_level(PQgetvalue(result, 0, 5), false);
		so_database_postgresql_get_bool(result, 0, 6, &pool->rewritable);
		so_database_postgresql_get_bool(result, 0, 7, &pool->deleted);

		unsigned char density_code;
		so_database_postgresql_get_uchar(result, 0, 8, &density_code);
		enum so_media_format_mode mode = so_media_string_to_format_mode(PQgetvalue(result, 0, 9), false);
		pool->format = so_database_postgresql_get_media_format(connect, density_code, mode);
	}

	PQclear(result);

	return pool;
}

static struct so_value * so_database_postgresql_get_pool_by_pool_mirror(struct so_database_connection * connect, struct so_pool * pool) {
	if (connect == NULL || pool == NULL)
		return NULL;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_pool_by_pool_mirror";
	so_database_postgresql_prepare(self, query, "SELECT p.id, uuid, p.name, autocheck, growable, unbreakablelevel, rewritable, deleted, densitycode, mode FROM pool p INNER JOIN mediaformat mf ON p.mediaformat = mf.id WHERE uuid::TEXT != $1 AND poolmirror IN (SELECT poolmirror FROM pool WHERE uuid::TEXT = $1)");

	const char * param[] = { pool->uuid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	struct so_value * pools = so_value_new_linked_list();

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (nb_result > 0) {
		int i;
		for (i = 0; i < nb_result; i++) {
			struct so_pool * new_pool = malloc(sizeof(struct so_pool));
			bzero(new_pool, sizeof(struct so_pool));

			struct so_value * key = so_value_new_custom(connect->config, NULL);
			struct so_value * db = so_value_new_hashtable2();
			new_pool->db_data = so_value_new_hashtable(so_value_custom_compute_hash);
			so_value_hashtable_put2(db, "id", so_value_new_string(PQgetvalue(result, 0, 0)), true);
			so_value_hashtable_put(new_pool->db_data, key, true, db, true);

			so_database_postgresql_get_string(result, i, 1, new_pool->uuid, 37);
			so_database_postgresql_get_string_dup(result, i, 2, &new_pool->name);
			new_pool->auto_check = so_pool_string_to_autocheck_mode(PQgetvalue(result, i, 3), false);
			so_database_postgresql_get_bool(result, i, 4, &new_pool->growable);
			new_pool->unbreakable_level = so_pool_string_to_unbreakable_level(PQgetvalue(result, i, 5), false);
			so_database_postgresql_get_bool(result, i, 6, &new_pool->rewritable);
			so_database_postgresql_get_bool(result, i, 7, &new_pool->deleted);

			unsigned char density_code;
			so_database_postgresql_get_uchar(result, i, 8, &density_code);
			enum so_media_format_mode mode = so_media_string_to_format_mode(PQgetvalue(result, i, 9), false);
			new_pool->format = so_database_postgresql_get_media_format(connect, density_code, mode);

			so_value_list_push(pools, so_value_new_custom(new_pool, so_pool_free2), true);
		}
	}

	return pools;
}

static struct so_value * so_database_postgresql_get_selected_files_by_job(struct so_database_connection * connect, struct so_job * job) {
	if (connect == NULL || job == NULL)
		return NULL;

	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * selected_files = so_value_new_linked_list();
	const char * query = "select_selected_path_by_job";
	so_database_postgresql_prepare(self, query, "SELECT path FROM selectedfile WHERE id IN (SELECT selectedfile FROM jobtoselectedfile WHERE job = $1) ORDER BY path");

	char * job_id = NULL;
	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * job_data = so_value_hashtable_get(job->db_data, key, false, false);
	so_value_unpack(job_data, "{ss}", "id", &job_id);
	so_value_free(key);

	const char * param[] = { job_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK) {
		int i;
		for (i = 0; i < nb_result; i++)
			so_value_list_push(selected_files, so_value_new_string(PQgetvalue(result, i, 0)), true);
	}

	PQclear(result);
	free(job_id);

	return selected_files;
}

static struct so_value * so_database_postgresql_get_standalone_drives(struct so_database_connection * connect) {
	struct so_value * changers = so_value_new_linked_list();

	char * host_id = so_database_postgresql_get_host(connect);
	if (host_id == NULL)
		return changers;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_real_changer_by_host";
	so_database_postgresql_prepare(self, query, "SELECT c.device, c.model, c.vendor, c.firmwarerev, c.serialnumber, c.status, c.isonline, c.action, c.enable FROM changer c LEFT JOIN drive d ON c.id = d.changer AND c.serialnumber = d.serialnumber WHERE c.host = $1 AND c.serialnumber NOT IN (SELECT uuid::TEXT FROM vtl WHERE host = $1)");

	const char * param[] = { host_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		int i;
		for (i = 0; i < nb_result; i++) {
			bool enabled = false;
			so_database_postgresql_get_bool(result, i, 11, &enabled);

			struct so_value * changer = so_value_pack("{sssssssssssnsbsssbsssb}",
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

			so_value_list_push(changers, changer, true);
		}
	}

	PQclear(result);
	free(host_id);

	return changers;
}

static struct so_value * so_database_postgresql_get_slot_by_drive(struct so_database_connection * connect, const char * drive_id) {
	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_slot_by_drive";
	so_database_postgresql_prepare(self, query, "SELECT index, isieport, enable FROM changerslot WHERE drive = $1");

	const char * param[] = { drive_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	struct so_value * slot = so_value_new_null();
	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK) {
		int index = -1;
		bool enable = false;
		bool ie_port = false;
		so_database_postgresql_get_int(result, 0, 0, &index);
		so_database_postgresql_get_bool(result, 0, 1, &ie_port);
		so_database_postgresql_get_bool(result, 0, 2, &enable);

		slot = so_value_pack("{sisssb}",
			"index", index,
			"ie port", ie_port,
			"enable", enable
		);
	}

	PQclear(result);

	return slot;
}

static struct so_value * so_database_postgresql_get_vtls(struct so_database_connection * connect, bool new_vtl) {
	struct so_value * changers = so_value_new_linked_list();

	char * host_id = so_database_postgresql_get_host(connect);
	if (host_id == NULL)
		return changers;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_vtls";
	if (new_vtl) {
		query = "select_new_vtls";
		so_database_postgresql_prepare(self, query, "SELECT v.uuid, v.path, v.prefix, v.nbslots, v.nbdrives, v.deleted, mf.name, mf.densitycode, mf.mode, NULL FROM vtl v INNER JOIN mediaformat mf ON v.mediaformat = mf.id WHERE v.host = $1 AND NOT v.deleted AND v.uuid::TEXT NOT IN (SELECT serialnumber FROM changer)");
	} else {
		query = "select_vtls";
		so_database_postgresql_prepare(self, query, "SELECT v.uuid, v.path, v.prefix, v.nbslots, v.nbdrives, v.deleted, mf.name, mf.densitycode, mf.mode, c.id FROM vtl v INNER JOIN mediaformat mf ON v.mediaformat = mf.id LEFT JOIN changer c ON v.uuid::TEXT = c.serialnumber WHERE v.host = $1 AND NOT v.deleted");
	}

	const char * params[] = { host_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, params, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		int i;
		for (i = 0; i < nb_result; i++) {
			char * changer_id = NULL;
			if (!PQgetisnull(result, i, 9))
				so_database_postgresql_get_string_dup(result, i, 9, &changer_id);

			unsigned int nb_slots, nb_drives;
			bool deleted;

			so_database_postgresql_get_uint(result, i, 3, &nb_slots);
			so_database_postgresql_get_uint(result, i, 4, &nb_drives);
			so_database_postgresql_get_bool(result, i, 5, &deleted);

			unsigned int density_code;
			so_database_postgresql_get_uint(result, i, 7, &density_code);
			enum so_media_format_mode mode = so_media_string_to_format_mode(PQgetvalue(result, i, 8), false);
			struct so_media_format * format = so_database_postgresql_get_media_format(connect, density_code, mode);

			struct so_value * changer = so_value_pack("{s[]sssssssisisbso}",
				"drives",
				"uuid", PQgetvalue(result, i, 0),
				"path", PQgetvalue(result, i, 1),
				"prefix", PQgetvalue(result, i, 2),
				"nb slots", (long long) nb_slots,
				"nb drives", (long long) nb_drives,
				"deleted", deleted,
				"format", so_media_format_convert(format)
			);

			so_value_list_push(changers, changer, true);

			so_media_format_free(format);
			free(changer_id);
		}
	}

	return changers;
}

static int so_database_postgresql_sync_changer(struct so_database_connection * connect, struct so_changer * changer, enum so_database_sync_method method) {
	if (connect == NULL || changer == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType transStatus = PQtransactionStatus(self->connect);
	if (transStatus == PQTRANS_INERROR)
		return 1;

	char * hostid = so_database_postgresql_get_host(connect);
	if (hostid == NULL)
		return 2;

	if (transStatus == PQTRANS_IDLE)
		so_database_postgresql_start_transaction(connect);

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = NULL;
	if (changer->db_data == NULL) {
		changer->db_data = so_value_new_hashtable(so_value_custom_compute_hash);

		db = so_value_new_hashtable2();
		so_value_hashtable_put(changer->db_data, key, true, db, true);
	} else {
		db = so_value_hashtable_get(changer->db_data, key, false, false);
		so_value_free(key);
	}

	char * changer_id = NULL;
	so_value_unpack(db, "{ss}", "id", &changer_id);

	bool enabled = true;
	if (changer_id != NULL) {
		const char * query = "select_changer_by_id";
		so_database_postgresql_prepare(self, query, "SELECT action, enable FROM changer WHERE id = $1 FOR UPDATE");

		const char * param[] = { changer_id };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);
		int nb_result = PQntuples(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && nb_result == 1 && method != so_database_sync_id_only) {
			enum so_changer_action new_action = so_database_postgresql_string_to_action(PQgetvalue(result, 0, 0));
			if (changer->status == so_changer_status_idle) {
				if (changer->is_online && new_action == so_changer_action_put_offline)
					changer->next_action = new_action;
				else if (!changer->is_online && new_action == so_changer_action_put_online)
					changer->next_action = new_action;
				else
					changer->next_action = so_changer_action_none;
			}

			so_database_postgresql_get_bool(result, 0, 1, &enabled);
		} else if (status == PGRES_TUPLES_OK && nb_result == 0)
			enabled = false;

		PQclear(result);

		if (status == PGRES_FATAL_ERROR || nb_result == 0) {
			free(hostid);
			free(changer_id);

			if (transStatus == PQTRANS_IDLE)
				so_database_postgresql_cancel_transaction(connect);
			return 3;
		}
	} else {
		const char * query;
		PGresult * result;

		if (changer->wwn != NULL) {
			query = "select_changer_by_model_vendor_serialnumber_wwn";
			so_database_postgresql_prepare(self, query, "SELECT id, enable FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 AND wwn = $4 FOR UPDATE");

			const char * param[] = { changer->model, changer->vendor, changer->serial_number, changer->wwn };
			result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
		} else {
			query = "select_changer_by_model_vendor_serialnumber";
			so_database_postgresql_prepare(self, query, "SELECT id, enable FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 AND wwn IS NULL FOR UPDATE");

			const char * param[] = { changer->model, changer->vendor, changer->serial_number };
			result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
		}
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			so_database_postgresql_get_string_dup(result, 0, 0, &changer_id);
			if (method != so_database_sync_id_only)
				so_database_postgresql_get_bool(result, 0, 1, &changer->enable);

			so_value_hashtable_put2(db, "id", so_value_new_string(changer_id), true);
		}

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			free(hostid);
			free(changer_id);

			if (transStatus == PQTRANS_IDLE)
				so_database_postgresql_cancel_transaction(connect);
			return 3;
		}
	}

	if (method != so_database_sync_id_only) {
		if (changer_id != NULL) {
			const char * query = "update_changer";
			so_database_postgresql_prepare(self, query, "UPDATE changer SET status = $1, firmwarerev = $2, isonline = $3, action = $4 WHERE id = $5");

			const char * params[] = {
				so_database_postgresql_changer_status_to_string(changer->status), changer->revision, so_database_postgresql_bool_to_string(changer->is_online),
				so_database_postgresql_changer_action_to_string(changer->next_action), changer_id
			};
			PGresult * result = PQexecPrepared(self->connect, query, 5, params, NULL, NULL, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				so_database_postgresql_get_error(result, query);

			PQclear(result);

			if (status == PGRES_FATAL_ERROR) {
				free(hostid);

				if (transStatus == PQTRANS_IDLE)
					so_database_postgresql_cancel_transaction(connect);
				return -2;
			}
		} else if (method == so_database_sync_init) {
			const char * query = "insert_changer";
			so_database_postgresql_prepare(self, query, "INSERT INTO changer(status, barcode, model, vendor, firmwarerev, serialnumber, wwn, host) VALUES ($1, $2, $3, $4, $5, $6, $7, $8) RETURNING id");

			const char * param[] = {
				so_database_postgresql_changer_status_to_string(changer->status), changer->barcode ? "true" : "false",
				changer->model, changer->vendor, changer->revision, changer->serial_number, changer->wwn, hostid,
			};

			PGresult * result = PQexecPrepared(self->connect, query, 8, param, NULL, NULL, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				so_database_postgresql_get_error(result, query);
			else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
				so_database_postgresql_get_string_dup(result, 0, 0, &changer_id);
				so_value_hashtable_put2(db, "id", so_value_new_string(changer_id), true);
			}

			PQclear(result);

			if (status == PGRES_FATAL_ERROR) {
				free(hostid);
				if (transStatus == PQTRANS_IDLE)
					so_database_postgresql_cancel_transaction(connect);
				return -1;
			}
		} else {
			if (transStatus == PQTRANS_IDLE)
				so_database_postgresql_cancel_transaction(connect);
			return -1;
		}
	}

	free(hostid);

	int failed = 0;
	unsigned int i;

	if (method == so_database_sync_id_only || method == so_database_sync_init) {
		so_database_postgresql_create_checkpoint(connect, "after_changers");

		for (i = 0; failed == 0 && i < changer->nb_drives; i++)
			if (changer->drives[i].enable)
				failed = so_database_postgresql_sync_drive(connect, changer->drives + i, false, method);

		if (failed != 0) {
			so_database_postgresql_cancel_checkpoint(connect, "after_changers");
			failed = 0;
			so_log_write2(so_log_level_warning, so_log_type_plugin_db, dgettext("libstoriqone-database-postgresql", "PSQL: error while synchronizing drive with database"));
		}
	}

	if (method == so_database_sync_init) {
		so_database_postgresql_create_checkpoint(connect, "after_drives");

		const char * query = "delete_from_changerslot_by_changer";
		so_database_postgresql_prepare(self, query, "DELETE FROM changerslot WHERE changer = $1");

		const char * param[] = { changer_id };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		PQclear(result);

		if (failed != 0) {
			so_database_postgresql_cancel_checkpoint(connect, "after_drives");
			failed = 0;
			so_log_write2(so_log_level_warning, so_log_type_plugin_db, dgettext("libstoriqone-database-postgresql", "PSQL: error while synchronizing changer slots with database"));
		}
	}

	free(changer_id);

	so_database_postgresql_create_checkpoint(connect, "after_slots");

	for (i = 0; failed == 0 && i < changer->nb_slots; i++)
		failed = so_database_postgresql_sync_slots(connect, changer->slots + i, method);

	if (failed != 0) {
		so_database_postgresql_cancel_checkpoint(connect, "after_slots");
		so_log_write2(so_log_level_warning, so_log_type_plugin_db, dgettext("libstoriqone-database-postgresql", "PSQL: error while synchronizing changer slots with database"));
	}

	if (transStatus == PQTRANS_IDLE)
		so_database_postgresql_finish_transaction(connect);

	return 0;
}

static int so_database_postgresql_sync_drive(struct so_database_connection * connect, struct so_drive * drive, bool sync_media, enum so_database_sync_method method) {
	if (connect == NULL || drive == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType transStatus = PQtransactionStatus(self->connect);
	if (transStatus == PQTRANS_INERROR)
		return 2;
	else if (transStatus == PQTRANS_IDLE)
		so_database_postgresql_start_transaction(connect);

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = NULL;
	if (drive->db_data == NULL) {
		drive->db_data = so_value_new_hashtable(so_value_custom_compute_hash);

		db = so_value_new_hashtable2();
		so_value_hashtable_put(drive->db_data, key, true, db, true);

		if (drive->changer != NULL) {
			struct so_value * changer_data = so_value_hashtable_get(drive->changer->db_data, key, false, false);
			struct so_value * changer_id = so_value_hashtable_get2(changer_data, "id", true, false);
			so_value_hashtable_put2(db, "changer id", changer_id, true);
		}
	} else {
		db = so_value_hashtable_get(drive->db_data, key, false, false);

		if (drive->changer != NULL && !so_value_hashtable_has_key2(db, "changer_id")) {
			struct so_value * changer_data = so_value_hashtable_get(drive->changer->db_data, key, false, false);
			struct so_value * changer_id = so_value_hashtable_get2(changer_data, "id", true, false);
			so_value_hashtable_put2(db, "changer id", changer_id, true);
		}

		so_value_free(key);
	}

	char * drive_id = NULL;
	so_value_unpack(db, "{ss}", "id", &drive_id);

	char * driveformat_id = NULL;

	if (drive_id != NULL) {
		const char * query = "select_drive_by_id";
		so_database_postgresql_prepare(self, query, "SELECT driveformat, enable FROM drive WHERE id = $1 FOR UPDATE");

		const char * param[] = { drive_id };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);
		int nb_result = PQntuples(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && nb_result == 1) {
			if (!PQgetisnull(result, 0, 0)) {
				so_database_postgresql_get_string_dup(result, 0, 0, &driveformat_id);
				so_value_hashtable_put2(db, "driveformat id", so_value_new_string(driveformat_id), true);
			}

			so_database_postgresql_get_bool(result, 0, 1, &drive->enable);
		} else if (status == PGRES_TUPLES_OK && nb_result == 1)
			drive->enable = false;

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			free(drive_id);
			if (transStatus == PQTRANS_IDLE)
				so_database_postgresql_cancel_transaction(connect);
			return 3;
		}
	} else {
		const char * query = "select_drive_by_model_vendor_serialnumber";
		so_database_postgresql_prepare(self, query, "SELECT id, to_char(operationduration, '0D'), lastclean, driveformat, enable FROM drive WHERE model = $1 AND vendor = $2 AND serialnumber = $3 FOR UPDATE");

		const char * param[] = { drive->model, drive->vendor, drive->serial_number };
		PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			so_database_postgresql_get_string_dup(result, 0, 0, &drive_id);
			so_value_hashtable_put2(db, "id", so_value_new_string(drive_id), true);

			if (method != so_database_sync_id_only) {
				double old_operation_duration = 0;
				so_database_postgresql_get_double(result, 0, 1, &old_operation_duration);
				drive->operation_duration += old_operation_duration;

				if (!PQgetisnull(result, 0, 2))
					so_database_postgresql_get_time(result, 0, 2, &drive->last_clean);
			}

			if (!PQgetisnull(result, 0, 3)) {
				so_database_postgresql_get_string_dup(result, 0, 3, &driveformat_id);
				so_value_hashtable_put2(db, "driveformat id", so_value_new_string(driveformat_id), true);
			}

			so_database_postgresql_get_bool(result, 0, 4, &drive->enable);
		}

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			if (transStatus == PQTRANS_IDLE)
				so_database_postgresql_cancel_transaction(connect);
			return 3;
		}
	}

	if (method != so_database_sync_id_only) {
		if (driveformat_id != NULL) {
			const char * query = "select_driveformat_by_id";
			so_database_postgresql_prepare(self, query, "SELECT densitycode FROM driveformat WHERE id = $1 LIMIT 1");

			const char * param[] = { driveformat_id };
			PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
			ExecStatusType status = PQresultStatus(result);

			unsigned char density_code = 0;

			if (status == PGRES_FATAL_ERROR)
				so_database_postgresql_get_error(result, query);
			else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
				so_database_postgresql_get_uchar(result, 0, 0, &density_code);

			PQclear(result);

			if (drive->density_code > density_code) {
				free(driveformat_id);
				driveformat_id = NULL;
			}
		}

		if (driveformat_id == NULL && drive->mode != so_media_format_mode_unknown) {
			const char * query = "select_driveformat_by_densitycode";
			so_database_postgresql_prepare(self, query, "SELECT id FROM driveformat WHERE densitycode = $1 AND mode = $2 LIMIT 1");

			char * densitycode = NULL;
			asprintf(&densitycode, "%u", drive->density_code);

			const char * param[] = { densitycode, so_media_format_mode_to_string(drive->mode, false), };
			PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				so_database_postgresql_get_error(result, query);
			else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
				so_database_postgresql_get_string_dup(result, 0, 0, &driveformat_id);
				so_value_hashtable_put2(db, "driveformat id", so_value_new_string(driveformat_id), true);
			}

			PQclear(result);
			free(densitycode);
		}

		if (drive_id != NULL) {
			const char * query = "update_drive";
			so_database_postgresql_prepare(self, query, "UPDATE drive SET status = $1, operationduration = $2, lastclean = $3, firmwarerev = $4, driveformat = $5 WHERE id = $6");

			char * op_duration = NULL, * last_clean = NULL;
			op_duration = so_database_postgresql_set_float(drive->operation_duration);

			if (drive->last_clean > 0) {
				last_clean = malloc(24);
				so_time_convert(&drive->last_clean, "%F %T", last_clean, 24);
			}

			const char * param[] = {
				so_database_postgresql_drive_status_to_string(drive->status), op_duration, last_clean, drive->revision, driveformat_id, drive_id,
			};
			PGresult * result = PQexecPrepared(self->connect, query, 6, param, NULL, NULL, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				so_database_postgresql_get_error(result, query);

			PQclear(result);
			free(last_clean);
			free(op_duration);

			if (status == PGRES_FATAL_ERROR) {
				free(drive_id);

				if (transStatus == PQTRANS_IDLE)
					so_database_postgresql_cancel_transaction(connect);
				return 3;
			}
		} else if (method == so_database_sync_init) {
			const char * query = "insert_drive";
			so_database_postgresql_prepare(self, query, "INSERT INTO drive (status, operationduration, lastclean, model, vendor, firmwarerev, serialnumber, changer, driveformat) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9) RETURNING id");

			char * op_duration = NULL, * last_clean = NULL;
			op_duration = so_database_postgresql_set_float(drive->operation_duration);

			db = so_value_hashtable_get(drive->db_data, key, false, false);

			char * changer_id = NULL;
			so_value_unpack(db, "{ss}", "changer id", &changer_id);

			if (drive->last_clean > 0) {
				last_clean = malloc(24);
				so_time_convert(&drive->last_clean, "%F %T", last_clean, 24);
			}

			const char * param[] = {
				so_database_postgresql_drive_status_to_string(drive->status), op_duration, last_clean,
				drive->model, drive->vendor, drive->revision, drive->serial_number, changer_id, driveformat_id
			};
			PGresult * result = PQexecPrepared(self->connect, query, 9, param, NULL, NULL, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				so_database_postgresql_get_error(result, query);
			else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
				so_database_postgresql_get_string_dup(result, 0, 0, &drive_id);
				so_value_hashtable_put2(db, "id", so_value_new_string(drive_id), true);
			}

			PQclear(result);
			free(changer_id);
			free(last_clean);
			free(op_duration);

			if (status == PGRES_FATAL_ERROR) {
				free(drive_id);

				if (transStatus == PQTRANS_IDLE)
					so_database_postgresql_cancel_transaction(connect);
				return 3;
			}
		}
	}

	free(driveformat_id);
	free(drive_id);

	if (sync_media && drive->slot != NULL && drive->slot->media != NULL)
		so_database_postgresql_sync_media(connect, drive->slot->media, method);

	if (transStatus == PQTRANS_IDLE)
		so_database_postgresql_finish_transaction(connect);

	return 0;
}

static int so_database_postgresql_sync_media(struct so_database_connection * connect, struct so_media * media, enum so_database_sync_method method) {
	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = NULL;
	if (media->db_data == NULL) {
		media->db_data = so_value_new_hashtable(so_value_custom_compute_hash);

		db = so_value_new_hashtable2();
		so_value_hashtable_put(media->db_data, key, true, db, true);
	} else {
		db = so_value_hashtable_get(media->db_data, key, false, false);
		so_value_free(key);
	}

	char * media_id = NULL, * mediaformat_id = NULL, * pool_id = NULL;
	so_value_unpack(db, "{ssss}", "id", &media_id, "media format id", &mediaformat_id);

	int failed = 0;

	if (method != so_database_sync_init && media_id == NULL) {
		const char * query = "select_media_by_medium_serial_number";
		so_database_postgresql_prepare(self, query, "SELECT id, label, name, mediaformat, pool FROM media WHERE mediumserialnumber = $1 LIMIT 1");

		const char * param[] = { media->medium_serial_number };

		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			free(media->label);
			free(media->name);
			media->label = media->name = NULL;

			so_database_postgresql_get_string_dup(result, 0, 0, &media_id);
			so_value_hashtable_put2(db, "id", so_value_new_string(media_id), true);

			so_database_postgresql_get_string_dup(result, 0, 1, &media->label);
			so_database_postgresql_get_string_dup(result, 0, 2, &media->name);

			so_database_postgresql_get_string_dup(result, 0, 2, &mediaformat_id);
			so_value_hashtable_put2(db, "media format id", so_value_new_string(mediaformat_id), true);

			if (!PQgetisnull(result, 0, 4)) {
				so_database_postgresql_get_string_dup(result, 0, 4, &pool_id);
				so_value_hashtable_put2(db, "pool id", so_value_new_string(pool_id), true);
				free(pool_id);
			}
		}

		PQclear(result);
	} else if (media != NULL) {
		const char * query = "select_media_for_sync";
		so_database_postgresql_prepare(self, query, "SELECT label, name FROM media WHERE id = $1 LIMIT 1");

		const char * param[] = { media_id };

		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			free(media->label);
			free(media->name);
			media->label = media->name = NULL;

			so_database_postgresql_get_string_dup(result, 0, 0, &media->label);
			so_database_postgresql_get_string_dup(result, 0, 1, &media->name);
		}

		PQclear(result);
	}

	if (media->pool != NULL) {
		so_value_unpack(db, "{ss}", "pool id", &pool_id);

		if (pool_id == NULL) {
			so_database_postgresql_sync_pool(connect, media->pool, so_database_sync_id_only);

			key = so_value_new_custom(connect->config, NULL);
			struct so_value * pool_db = so_value_hashtable_get(media->pool->db_data, key, false, false);
			so_value_unpack(pool_db, "{ss}", "id", &pool_id);
			so_value_hashtable_put2(db, "pool id", so_value_new_string(pool_id), true);
		}
	}

	if (media_id == NULL) {
		if (mediaformat_id == NULL) {
			if (media->format->db_data != NULL) {
				struct so_value * format_db = so_value_hashtable_get(media->format->db_data, key, false, false);
				so_value_unpack(format_db, "{ss}", "id", &mediaformat_id);
				so_value_hashtable_put2(db, "media format id", so_value_new_string(mediaformat_id), true);
			} else {
				const char * query = "select_media_format_by_density";
				so_database_postgresql_prepare(self, query, "SELECT id FROM mediaformat WHERE densitycode = $1 AND mode = $2");

				char * densitycode = NULL;
				asprintf(&densitycode, "%hhu", media->format->density_code);

				const char * param[] = { densitycode, so_media_format_mode_to_string(media->format->mode, false) };
				PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
				ExecStatusType status = PQresultStatus(result);

				if (status == PGRES_FATAL_ERROR)
					so_database_postgresql_get_error(result, query);
				else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
					so_database_postgresql_get_string_dup(result, 0, 0, &mediaformat_id);
					so_value_hashtable_put2(db, "media format id", so_value_new_string(mediaformat_id), true);
				}

				free(densitycode);
				PQclear(result);
			}
		}

		const char * query = "insert_into_media";
		so_database_postgresql_prepare(self, query, "INSERT INTO media(uuid, label, mediumserialnumber, name, status, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, type, nbfiles, blocksize, freeblock, totalblock, mediaformat, pool) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20, $21, $22, $23) RETURNING id");

		char buffer_first_used[32];
		char buffer_use_before[32];
		char buffer_last_read[32] = "";
		char buffer_last_write[32] = "";
		so_time_convert(&media->first_used, "%F %T", buffer_first_used, 32);
		so_time_convert(&media->use_before, "%F %T", buffer_use_before, 32);
		if (media->last_read > 0)
			so_time_convert(&media->last_read, "%F %T", buffer_last_read, 32);
		if (media->last_write > 0)
			so_time_convert(&media->last_write, "%F %T", buffer_last_write, 32);

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
			*media->uuid ? media->uuid : NULL, media->label, media->medium_serial_number, media->name, so_media_status_to_string(media->status, false),
			buffer_first_used, buffer_use_before, media->last_read > 0 ? buffer_last_read : NULL, media->last_write > 0 ? buffer_last_write : NULL,
			load, read, write, totalblockread, totalblockwrite, totalreaderror, totalwriteerror, so_media_type_to_string(media->type, false), nbfiles,
			blocksize, freeblock, totalblock, mediaformat_id, pool_id
		};
		PGresult * result = PQexecPrepared(self->connect, query, 23, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			so_value_hashtable_put2(db, "id", so_value_new_string(PQgetvalue(result, 0, 0)), true);

		PQclear(result);
		free(load);
		free(read);
		free(write);
		free(nbfiles);
		free(blocksize);
		free(freeblock);
		free(totalblock);
		free(mediaformat_id);
		free(pool_id);
		free(totalblockread);
		free(totalblockwrite);
		free(totalreaderror);
		free(totalwriteerror);

		return status == PGRES_FATAL_ERROR;
	} else if (method != so_database_sync_id_only) {
		const char * query = "update_media";
		so_database_postgresql_prepare(self, query, "UPDATE media SET uuid = $1, name = $2, status = $3, lastread = $4, lastwrite = $5, loadcount = $6, readcount = $7, writecount = $8, nbtotalblockread = $9, nbtotalblockwrite = $10, nbreaderror = $11, nbwriteerror = $12, nbfiles = $13, blocksize = $14, freeblock = $15, totalblock = $16, pool = $17, type = $18 WHERE id = $19");

		char buffer_last_read[32] = "";
		char buffer_last_write[32] = "";
		if (media->last_read > 0)
			so_time_convert(&media->last_read, "%F %T", buffer_last_read, 32);
		if (media->last_write > 0)
			so_time_convert(&media->last_write, "%F %T", buffer_last_write, 32);

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
			*media->uuid ? media->uuid : NULL, media->name, so_media_status_to_string(media->status, false),
			media->last_read > 0 ? buffer_last_read : NULL, media->last_write > 0 ? buffer_last_write : NULL,
			load, read, write, totalblockread, totalblockwrite, totalreaderror, totalwriteerror, nbfiles,
			blocksize, freeblock, totalblock, pool_id, so_media_type_to_string(media->type, false), media_id
		};
		PGresult * result = PQexecPrepared(self->connect, query, 19, param2, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);

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

static int so_database_postgresql_sync_pool(struct so_database_connection * connect, struct so_pool * pool, enum so_database_sync_method method) {
	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = NULL;
	if (pool->db_data == NULL) {
		pool->db_data = so_value_new_hashtable(so_value_custom_compute_hash);

		db = so_value_new_hashtable2();
		so_value_hashtable_put(pool->db_data, key, true, db, true);
	} else {
		db = so_value_hashtable_get(pool->db_data, key, false, false);
		so_value_free(key);
	}

	if (method == so_database_sync_id_only) {
		const char * query = "select_pool_id_by_uuid";
		so_database_postgresql_prepare(self, query, "SELECT id FROM pool WHERE uuid = $1 LIMIT 1");

		const char * param[] = { pool->uuid };

		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			char * pool_id = PQgetvalue(result, 0, 0);
			so_value_hashtable_put2(db, "id", so_value_new_string(pool_id), true);
		}

		PQclear(result);

		return status != PGRES_TUPLES_OK;
	}

	return 0;
}

static int so_database_postgresql_sync_slots(struct so_database_connection * connect, struct so_slot * slot, enum so_database_sync_method method) {
	int failed = 0;

	// media in drive are updated directly by sync_drive
	if (slot->drive == NULL && slot->media != NULL)
		failed = so_database_postgresql_sync_media(connect, slot->media, method);

	if (failed != 0)
		return failed;

	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = NULL;

	if (slot->db_data != NULL)
		db = so_value_hashtable_get(slot->db_data, key, false, false);

	if (db == NULL || db->type == so_value_null) {
		slot->db_data = so_value_new_hashtable(so_value_custom_compute_hash);

		db = so_value_new_hashtable2();
		so_value_hashtable_put(slot->db_data, key, false, db, true);

		struct so_value * changer_data = so_value_hashtable_get(slot->changer->db_data, key, false, false);
		struct so_value * changer_id = so_value_hashtable_get2(changer_data, "id", true, false);
		so_value_hashtable_put2(db, "changer id", changer_id, true);

		if (slot->drive != NULL) {
			struct so_value * drive_data = so_value_hashtable_get(slot->drive->db_data, key, false, false);
			struct so_value * drive_id = so_value_hashtable_get2(drive_data, "id", true, false);
			so_value_hashtable_put2(db, "drive id", drive_id, true);
		}
	} else {
		if (!so_value_hashtable_has_key2(db, "changer id")) {
			struct so_value * changer_data = so_value_hashtable_get(slot->changer->db_data, key, false, false);
			struct so_value * changer_id = so_value_hashtable_get2(changer_data, "id", true, false);
			so_value_hashtable_put2(db, "changer id", changer_id, true);
		}

		if (slot->drive != NULL && !so_value_hashtable_has_key2(db, "drive id")) {
			struct so_value * drive_data = so_value_hashtable_get(slot->drive->db_data, key, false, false);
			struct so_value * drive_id = so_value_hashtable_get2(drive_data, "id", true, false);
			so_value_hashtable_put2(db, "drive id", drive_id, true);
		}
	}

	char * changer_id = NULL, * drive_id = NULL, * media_id = NULL;
	so_value_unpack(db, "{ssss}", "changer id", &changer_id, "drive id", &drive_id);

	if (slot->media != NULL) {
		struct so_value * media_data = so_value_hashtable_get(slot->media->db_data, key, false, false);
		so_value_unpack(media_data, "{ss}", "id", &media_id);
	}

	so_value_free(key);

	if (method != so_database_sync_init) {
		const char * query = "select_changerslot_by_index";
		so_database_postgresql_prepare(self, query, "SELECT enable FROM changerslot WHERE changer = $1 AND index = $2 LIMIT 1");

		char * sindex;
		asprintf(&sindex, "%u", slot->index);

		const char * param[] = { changer_id, sindex };

		PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			so_database_postgresql_get_bool(result, 0, 0, &slot->enable);

		PQclear(result);
		free(sindex);
	}

	if (method == so_database_sync_init) {
		const char * query = "insert_into_changerslot";
		so_database_postgresql_prepare(self, query, "INSERT INTO changerslot(changer, index, drive, media, isieport) VALUES ($1, $2, $3, $4, $5)");

		char * sindex;
		asprintf(&sindex, "%u", slot->index);

		const char * param[] = { changer_id, sindex, drive_id, media_id, so_database_postgresql_bool_to_string(slot->is_ie_port) };

		PGresult * result = PQexecPrepared(self->connect, query, 5, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);

		PQclear(result);
		free(sindex);
	} else if (method != so_database_sync_id_only) {
		const char * query = "update_changerslot";
		so_database_postgresql_prepare(self, query, "UPDATE changerslot SET media = $1, enable = $2 WHERE changer = $3 AND index = $4");

		char * sindex;
		asprintf(&sindex, "%u", slot->index);

		const char * param[] = { media_id, slot->enable ? "true" : "false", changer_id, sindex };

		PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);

		PQclear(result);
		free(sindex);
	}

	free(changer_id);
	free(drive_id);
	free(media_id);

	return failed;
}

static struct so_value * so_database_postgresql_update_vtl(struct so_database_connection * connect, struct so_changer * vtl) {
	if (connect == NULL || vtl == NULL)
		return NULL;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_vtl_by_uuid";
	so_database_postgresql_prepare(self, query, "SELECT nbslots, nbdrives, deleted FROM vtl WHERE uuid = $1 LIMIT 1");

	const char * param[] = { vtl->serial_number };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	struct so_value * vtl_updated = NULL;
	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		unsigned int nb_slots = 0, nb_drives = 0;
		bool deleted = false;

		so_database_postgresql_get_uint(result, 0, 0, &nb_slots);
		so_database_postgresql_get_uint(result, 0, 1, &nb_drives);
		so_database_postgresql_get_bool(result, 0, 2, &deleted);

		vtl_updated = so_value_pack("{sisisb}",
			"nb slots", (long) nb_slots,
			"nb drives", (long) nb_drives,
			"deleted", deleted
		);
	}

	PQclear(result);

	return vtl_updated;
}


static int so_database_postgresql_add_job_record(struct so_database_connection * connect, struct so_job * job, enum so_log_level level, enum so_job_record_notif notif, const char * message) {
	if (connect == NULL || job == NULL || message == NULL)
		return 1;

	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = so_value_hashtable_get(job->db_data, key, false, false);
	so_value_free(key);

	char * jobrun_id = NULL;
	so_value_unpack(db, "{ss}", "jobrun id", &jobrun_id);

	const char * query = "insert_new_jobrecord";
	so_database_postgresql_prepare(self, query, "INSERT INTO jobrecord(jobrun, status, level, message, notif) VALUES ($1, $2, $3, $4, $5)");

	const char * param[] = { jobrun_id, so_job_status_to_string(job->status, false), so_database_postgresql_log_level_to_string(level), message, so_job_report_notif_to_string(notif, false) };
	PGresult * result = PQexecPrepared(self->connect, query, 5, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	PQclear(result);
	free(jobrun_id);

	return status != PGRES_TUPLES_OK;
}

static int so_database_postgresql_add_report(struct so_database_connection * connect, struct so_job * job, struct so_archive * archive, struct so_media * media, const char * data) {
	if (connect == NULL || job == NULL || (archive == NULL && media == NULL) || data == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db_job = so_value_hashtable_get(job->db_data, key, false, false);
	struct so_value * db_archive = NULL, * db_media = NULL;
	if (archive != NULL)
		db_archive = so_value_hashtable_get(archive->db_data, key, false, false);
	if (media != NULL)
		db_media = so_value_hashtable_get(media->db_data, key, false, false);
	so_value_free(key);

	char * jobrun_id = NULL, * archive_id = NULL, * media_id = NULL;
	so_value_unpack(db_job, "{ss}", "jobrun id", &jobrun_id);
	if (db_archive != NULL)
		so_value_unpack(db_archive, "{ss}", "id", &archive_id);
	if (db_media != NULL)
		so_value_unpack(db_media, "{ss}", "id", &media_id);

	const char * query = "insert_new_report";
	so_database_postgresql_prepare(self, query, "INSERT INTO report(jobrun, archive, media, data) VALUES ($1, $2, $3, $4)");

	const char * param[] = { jobrun_id, archive_id, media_id, data };
	PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	PQclear(result);
	free(jobrun_id);
	free(archive_id);
	free(media_id);

	return status != PGRES_COMMAND_OK;
}

static char * so_database_postgresql_get_restore_path(struct so_database_connection * connect, struct so_job * job) {
	if (connect == NULL || job == NULL)
		return NULL;

	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = so_value_hashtable_get(job->db_data, key, false, false);
	so_value_free(key);

	char * job_id = NULL;
	so_value_unpack(db, "{ss}", "id", &job_id);

	const char * query = "select_restore_path";
	so_database_postgresql_prepare(self, query, "SELECT path FROM restoreto WHERE job = $1 LIMIT 1");

	const char * param[] = { job_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	char * path = NULL;
	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		so_database_postgresql_get_string_dup(result, 0, 0, &path);

	PQclear(result);
	free(job_id);

	return path;
}

static int so_database_postgresql_start_job(struct so_database_connection * connect, struct so_job * job) {
	if (connect == NULL || job == NULL)
		return 1;

	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = so_value_hashtable_get(job->db_data, key, false, false);
	so_value_free(key);

	char * job_id = NULL;
	so_value_unpack(db, "{ss}", "id", &job_id);

	const char * query = "insert_new_jobrun";
	so_database_postgresql_prepare(self, query, "INSERT INTO jobrun(job, numrun) VALUES ($1, $2) RETURNING id");

	char * numrun = NULL;
	asprintf(&numrun, "%ld", job->num_runs);

	const char * param[] = { job_id, numrun };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		char * jobrun_id = PQgetvalue(result, 0, 0);
		so_value_hashtable_put2(db, "jobrun id", so_value_new_string(jobrun_id), true);
	}

	PQclear(result);
	free(job_id);
	free(numrun);

	return status != PGRES_TUPLES_OK;
}

static int so_database_postgresql_stop_job(struct so_database_connection * connect, struct so_job * job) {
	if (connect == NULL || job == NULL)
		return 1;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = so_value_hashtable_get(job->db_data, key, false, false);
	so_value_free(key);

	char * job_id = NULL, * jobrun_id = NULL;
	so_value_unpack(db, "{ssss}", "id", &job_id, "jobrun id", &jobrun_id);

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "finish_job";
	so_database_postgresql_prepare(self, query, "UPDATE job SET status = $1, update = NOW() WHERE id = $2");

	const char * param_job[] = { so_job_status_to_string(job->status, false), job_id };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param_job, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	PQclear(result);
	free(job_id);

	query = "finish_jobrun";
	so_database_postgresql_prepare(self, query, "UPDATE jobrun SET endtime = NOW(), status = $1, done = $2, exitcode = $3, stoppedbyuser = $4 WHERE id = $5");

	char * done = so_database_postgresql_set_float(job->done);
	char * exitcode = NULL;
	asprintf(&exitcode, "%d", job->exit_code);

	const char * param_jobrun[] = { so_job_status_to_string(job->status, false), done, exitcode, so_database_postgresql_bool_to_string(job->stopped_by_user), jobrun_id };
	result = PQexecPrepared(self->connect, query, 5, param_jobrun, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	PQclear(result);
	free(exitcode);
	free(done);
	free(jobrun_id);

	return status != PGRES_COMMAND_OK;
}

static int so_database_postgresql_sync_job(struct so_database_connection * connect, struct so_job * job) {
	if (connect == NULL || job == NULL)
		return 1;

	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	char * job_id = NULL;
	char * jobrun_id = NULL;

	if (job->db_data == NULL) {
		job->db_data = so_value_new_hashtable(so_value_custom_compute_hash);
		struct so_value * db = so_value_pack("{ss}", "id", job->key);
		so_value_hashtable_put(job->db_data, key, false, db, true);
		job_id = strdup(job->key);
	} else {
		struct so_value * db = so_value_hashtable_get(job->db_data, key, false, false);
		so_value_unpack(db, "{ssss}", "id", &job_id, "jobrun id", &jobrun_id);
	}

	so_value_free(key);

	const char * query = "check_if_user_stop_job";
	so_database_postgresql_prepare(self, query, "SELECT status FROM job WHERE id = $1 LIMIT 1");

	const char * param[] = { job_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		const enum so_job_status new_status = so_job_string_to_status(PQgetvalue(result, 0, 0), false);

		if ((job->status == so_job_status_running || job->status == so_job_status_pause) && new_status == so_job_status_stopped) {
			job->status = new_status;
			job->stopped_by_user = true;
		}
	}
	PQclear(result);

	// update job
	query = "update_job";
	so_database_postgresql_prepare(self, query, "UPDATE job SET status = $1, repetition = $2, update = NOW() WHERE id = $3");

	char * repetition = NULL;
	asprintf(&repetition, "%ld", job->repetition);

	const char * param2[] = { so_job_status_to_string(job->status, false), repetition, job_id };
	result = PQexecPrepared(self->connect, query, 3, param2, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	PQclear(result);
	free(job_id);
	free(repetition);

	if (jobrun_id == NULL)
		return status != PGRES_COMMAND_OK;

	// update jobrun
	query = "update_jobrun";
	so_database_postgresql_prepare(self, query, "UPDATE jobrun SET status = $1, done = $2 WHERE id = $3");

	char * done = so_database_postgresql_set_float(job->done);

	const char * param3[] = { so_job_status_to_string(job->status, false), done, jobrun_id };
	result = PQexecPrepared(self->connect, query, 3, param3, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	PQclear(result);
	free(done);
	free(jobrun_id);

	return status != PGRES_COMMAND_OK;
}

static int so_database_postgresql_sync_jobs(struct so_database_connection * connect, struct so_value * jobs) {
	if (connect == NULL || jobs == NULL)
		return 1;

	char * host_id = so_database_postgresql_get_host(connect);
	if (host_id == NULL)
		return 2;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_scheduled_jobs";
	so_database_postgresql_prepare(self, query, "SELECT j.id, j.name, jt.name, nextstart, interval, repetition, u.login, metadata, options, COALESCE(MAX(jr.numrun) + 1, 1) FROM job j INNER JOIN jobtype jt ON j.type = jt.id LEFT JOIN jobrun jr ON j.id = jr.job LEFT JOIN users u ON j.login = u.id WHERE j.status = 'scheduled' AND host = $1 GROUP BY j.id, j.name, jt.name, nextstart, interval, repetition, u.login, metadata::TEXT, options::TEXT");

	const char * param[] = { host_id };

	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		int i;
		for (i = 0; i < nb_result; i++) {
			char * job_id;
			so_database_postgresql_get_string_dup(result, i, 0, &job_id);

			struct so_job * job = NULL;
			bool has_job = so_value_hashtable_has_key2(jobs, job_id);
			if (has_job) {
				struct so_value * vj = so_value_hashtable_get2(jobs, job_id, false, false);
				job = so_value_custom_get(vj);
			} else {
				job = malloc(sizeof(struct so_job));
				bzero(job, sizeof(struct so_job));

				so_database_postgresql_get_string_dup(result, i, 0, &job->key);
				so_database_postgresql_get_string_dup(result, i, 1, &job->name);
				so_database_postgresql_get_string_dup(result, i, 2, &job->type);
				so_database_postgresql_get_string_dup(result, i, 6, &job->user);
				job->meta = so_json_parse_string(PQgetvalue(result, i, 7));
				job->option = so_json_parse_string(PQgetvalue(result, i, 8));
			}

			so_database_postgresql_get_time(result, i, 3, &job->next_start);
			so_database_postgresql_get_long(result, i, 4, &job->interval);
			so_database_postgresql_get_long(result, i, 5, &job->repetition);
			job->status = so_job_status_scheduled;
			so_database_postgresql_get_long(result, i, 9, &job->num_runs);

			if (!has_job) {
				job->db_data = so_value_new_hashtable(so_value_custom_compute_hash);
				struct so_value * key = so_value_new_custom(connect->config, NULL);
				struct so_value * db = so_value_pack("{ss}", "id", job_id);
				so_value_hashtable_put(job->db_data, key, true, db, true);

				struct so_value * vj = so_value_new_custom(job, so_job_free2);
				so_value_hashtable_put2(jobs, job_id, vj, true);
			}

			free(job_id);
		}
	}

	PQclear(result);
	free(host_id);

	return status == PGRES_FATAL_ERROR;
}


static int so_database_postgresql_get_nb_scripts(struct so_database_connection * connect, const char * job_type, enum so_script_type type, struct so_pool * pool) {
	if (connect == NULL || job_type == NULL || pool == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_nb_scripts_with_sequence_and_pool";
	so_database_postgresql_prepare(self, query, "SELECT COUNT(DISTINCT script) FROM scripts ss LEFT JOIN jobtype jt ON ss.jobtype = jt.id AND jt.name = $1 LEFT JOIN pool p ON ss.pool = p.id AND p.uuid = $2 WHERE scripttype = $3");

	const char * param[] = { job_type, pool->uuid, so_script_type_to_string(type, false) };
	PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	int nb_result = -1;

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		so_database_postgresql_get_int(result, 0, 0, &nb_result);

	PQclear(result);

	return nb_result;
}

static char * so_database_postgresql_get_script(struct so_database_connection * connect, const char * job_type, unsigned int sequence, enum so_script_type type, struct so_pool * pool) {
	if (connect == NULL || job_type == NULL || pool == NULL)
		return NULL;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_script_with_sequence_and_pool";
	so_database_postgresql_prepare(self, query, "SELECT s.path FROM scripts ss LEFT JOIN jobtype jt ON ss.jobtype = jt.id LEFT JOIN script s ON ss.script = s.id AND jt.name = $1 LEFT JOIN pool p ON ss.pool = p.id AND p.uuid = $2 WHERE scripttype = $3 ORDER BY sequence LIMIT 1 OFFSET $4");

	char * seq;
	asprintf(&seq, "%u", sequence);

	const char * param[] = { job_type, so_script_type_to_string(type, false), pool->uuid, seq };
	PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	char * script = NULL;
	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (PQntuples(result) > 0)
		so_database_postgresql_get_string_dup(result, 0, 0, &script);

	PQclear(result);

	free(seq);

	return script;
}


static bool so_database_postgresql_find_plugin_checksum(struct so_database_connection * connect, const char * checksum) {
	if (connect == NULL || checksum == NULL)
		return false;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "select_checksum_by_name";
	so_database_postgresql_prepare(self, query, "SELECT name FROM checksum WHERE name = $1 LIMIT 1");

	const char * param[] = { checksum };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	bool found = false;
	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		found = true;

	PQclear(result);

	return found;
}

static int so_database_postgresql_sync_plugin_checksum(struct so_database_connection * connect, struct so_checksum_driver * driver) {
	if (connect == NULL || driver == NULL)
		return -1;

	if (so_database_postgresql_find_plugin_checksum(connect, driver->name))
		return 0;

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "insert_checksum";
	so_database_postgresql_prepare(self, query, "INSERT INTO checksum(name, deflt) VALUES ($1, $2)");

	const char * param[] = { driver->name, so_database_postgresql_bool_to_string(driver->default_checksum) };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	PQclear(result);

	return status == PGRES_FATAL_ERROR;
}

static int so_database_postgresql_sync_plugin_job(struct so_database_connection * connect, const char * job) {
	if (connect == NULL || job == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;
	const char * query = "select_jobtype_by_name";
	so_database_postgresql_prepare(self, query, "SELECT name FROM jobtype WHERE name = $1 LIMIT 1");

	const char * param[] = { job };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	bool found = false;
	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		found = true;

	PQclear(result);

	if (found)
		return 0;

	query = "insert_jobtype";
	so_database_postgresql_prepare(self, query, "INSERT INTO jobtype(name) VALUES ($1)");

	result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	PQclear(result);

	return status == PGRES_FATAL_ERROR;
}


static int so_database_postgresql_check_archive_file(struct so_database_connection * connect, struct so_archive * archive, struct so_archive_file * file) {
	if (connect == NULL || archive == NULL || file == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;
	struct so_value * key = so_value_new_custom(connect->config, NULL);

	char * archive_id = NULL, * file_id = NULL;
	struct so_value * db = so_value_hashtable_get(archive->db_data, key, false, false);
	so_value_unpack(db, "{ss}", "id", &archive_id);

	db = so_value_hashtable_get(file->db_data, key, false, false);
	so_value_unpack(db, "{ss}", "id", &file_id);

	const char * query = "update_check_archive_file";
	so_database_postgresql_prepare(self, query, "UPDATE archivefiletoarchivevolume SET checktime = $1, checksumok = $2 WHERE archivefile = $3 AND archivevolume IN (SELECT id FROM archivevolume WHERE archive = $4)");

	char checktime[32];
	so_time_convert(&file->check_time, "%F %T", checktime, 32);

	const char * param[] = { checktime, so_database_postgresql_bool_to_string(file->check_ok), file_id, archive_id };
	PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	PQclear(result);

	return status != PGRES_COMMAND_OK;
}

static int so_database_postgresql_check_archive_volume(struct so_database_connection * connect, struct so_archive_volume * volume) {
	if (connect == NULL || volume == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;
	struct so_value * key = so_value_new_custom(connect->config, NULL);

	char * volume_id = NULL;
	struct so_value * db = so_value_hashtable_get(volume->db_data, key, false, false);
	so_value_unpack(db, "{ss}", "id", &volume_id);

	const char * query = "update_check_archive_volume";
	so_database_postgresql_prepare(self, query, "UPDATE archivevolume SET checktime = $1, checksumok = $2 WHERE id = $3");

	char checktime[32];
	so_time_convert(&volume->check_time, "%F %T", checktime, 32);

	const char * param[] = { checktime, so_database_postgresql_bool_to_string(volume->check_ok), volume_id };
	PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	PQclear(result);

	return status != PGRES_COMMAND_OK;
}

static struct so_archive * so_database_postgresql_get_archive_by_id(struct so_database_connection * connect, const char * archive_id) {
	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_archive * archive = NULL;
	struct so_value * key = so_value_new_custom(connect->config, NULL);

	const char * query = "select_archive_by_id";
	so_database_postgresql_prepare(self, query, "SELECT a.uuid, a.name, uc.login, uo.login, a.deleted FROM archive a INNER JOIN users uc ON a.creator = uc.id INNER JOIN users uo ON a.owner = uo.id WHERE a.id = $1 LIMIT 1");

	const char * param[] = { archive_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		archive = so_archive_new();

		so_database_postgresql_get_string(result, 0, 0, archive->uuid, 37);
		so_database_postgresql_get_string_dup(result, 0, 1, &archive->name);
		so_database_postgresql_get_string_dup(result, 0, 2, &archive->creator);
		so_database_postgresql_get_string_dup(result, 0, 3, &archive->owner);
		so_database_postgresql_get_bool(result, 0, 4, &archive->deleted);

		archive->db_data = so_value_new_hashtable(so_value_custom_compute_hash);
		struct so_value * db = so_value_new_hashtable2();
		so_value_hashtable_put(archive->db_data, key, false, db, true);
		so_value_hashtable_put2(db, "id", so_value_new_string(archive_id), true);
	}

	PQclear(result);

	if (archive == NULL)
		return NULL;

	query = "select_archivevolume_by_archive";
	so_database_postgresql_prepare(self, query, "SELECT av.id, av.sequence, av.size, av.starttime, av.endtime, av.checksumok, av.checktime, m.mediumserialnumber, av.mediaposition FROM archivevolume av INNER JOIN media m ON av.media = m.id WHERE archive = $1 ORDER BY sequence");

	const char * param2[] = { archive_id };
	result = PQexecPrepared(self->connect, query, 1, param2, NULL, NULL, 0);
	status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		archive->nb_volumes = nb_result;
		archive->volumes = calloc(nb_result, sizeof(struct so_archive_volume));

		unsigned int i;
		for (i = 0; i < archive->nb_volumes; i++) {
			struct so_archive_volume * vol = archive->volumes + i;

			char * archive_volume_id = PQgetvalue(result, i, 0);
			so_database_postgresql_get_uint(result, i, 1, &vol->sequence);
			so_database_postgresql_get_ssize(result, i, 2, &vol->size);
			so_database_postgresql_get_time(result, i, 3, &vol->start_time);
			so_database_postgresql_get_time(result, i, 4, &vol->end_time);
			so_database_postgresql_get_bool(result, i, 5, &vol->check_ok);
			so_database_postgresql_get_time(result, i, 6, &vol->check_time);

			char * media_id = PQgetvalue(result, i, 7);
			vol->media = so_database_postgresql_get_media(connect, media_id, NULL, NULL);
			so_database_postgresql_get_uint(result, i, 8, &vol->media_position);

			vol->db_data = so_value_new_hashtable(so_value_custom_compute_hash);
			struct so_value * db = so_value_new_hashtable2();
			so_value_hashtable_put(vol->db_data, key, false, db, true);
			so_value_hashtable_put2(db, "id", so_value_new_string(archive_volume_id), true);
			so_value_hashtable_put2(db, "archive id", so_value_new_string(archive_id), true);

			vol->digests = so_value_new_hashtable2();
			vol->archive = archive;
			archive->size += vol->size;


			const char * query3 = "select_checksum_from_archivevolume";
			so_database_postgresql_prepare(self, query3, "SELECT c.name, cr.result FROM archivevolumetochecksumresult av2cr INNER JOIN checksumresult cr ON av2cr.archivevolume = $1 AND av2cr.checksumresult = cr.id LEFT JOIN checksum c ON cr.checksum = c.id");

			const char * param3[] = { archive_volume_id };
			PGresult * result3 = PQexecPrepared(self->connect, query3, 1, param3, NULL, NULL, 0);
			ExecStatusType status3 = PQresultStatus(result);
			int nb_result3 = PQntuples(result3);

			if (status == PGRES_FATAL_ERROR)
				so_database_postgresql_get_error(result3, query3);
			else if (status3 == PGRES_TUPLES_OK && nb_result3 > 0) {
				int j;
				for (j = 0; j < nb_result3; j++)
					so_value_hashtable_put2(vol->digests, PQgetvalue(result3, j, 0), so_value_new_string(PQgetvalue(result3, j, 1)), true);
			}

			PQclear(result3);

			const char * query4 = "select_files_from_archivevolume";
			so_database_postgresql_prepare(self, query4, "SELECT af.id, afv.blocknumber, afv.archivetime, afv.checktime, afv.checksumok, af.name, af.type, af.mimetype, af.ownerid, af.owner, af.groupid, af.groups, af.perm, af.ctime, af.mtime, af.size, sf.path FROM archivefiletoarchivevolume afv INNER JOIN archivefile af ON afv.archivevolume = $1 AND afv.archivefile = af.id INNER JOIN selectedfile sf ON af.parent = sf.id ORDER BY af.id");

			result3 = PQexecPrepared(self->connect, query4, 1, param3, NULL, NULL, 0);
			status3 = PQresultStatus(result3);
			nb_result3 = PQntuples(result3);

			if (status == PGRES_FATAL_ERROR)
				so_database_postgresql_get_error(result3, query3);
			else if (status3 == PGRES_TUPLES_OK && nb_result3 > 0) {
				vol->nb_files = nb_result3;
				vol->files = calloc(nb_result3, sizeof(struct so_archive_files));

				unsigned int j;
				for (j = 0; j < vol->nb_files; j++) {
					struct so_archive_files * ptr_file = vol->files + j;
					struct so_archive_file * file = ptr_file->file = malloc(sizeof(struct so_archive_file));
					bzero(ptr_file->file, sizeof(struct so_archive_file));

					char * file_id = PQgetvalue(result3, j, 0);
					so_database_postgresql_get_ssize(result3, j, 1, &ptr_file->position);
					so_database_postgresql_get_time(result3, j, 2, &ptr_file->archived_time);

					so_database_postgresql_get_string_dup(result3, j, 5, &file->path);
					file->type = so_archive_file_string_to_type(PQgetvalue(result3, j, 6), false);
					so_database_postgresql_get_string_dup(result3, j, 7, &file->mime_type);
					so_database_postgresql_get_uint(result3, j, 8, &file->ownerid);
					so_database_postgresql_get_string_dup(result3, j, 9, &file->owner);
					so_database_postgresql_get_uint(result3, j, 10, &file->groupid);
					so_database_postgresql_get_string_dup(result3, j, 11, &file->group);
					so_database_postgresql_get_uint(result3, j, 12, &file->perm);

					so_database_postgresql_get_time(result3, j, 13, &file->create_time);
					so_database_postgresql_get_time(result3, j, 14, &file->modify_time);

					so_database_postgresql_get_time(result3, j, 3, &file->check_time);
					so_database_postgresql_get_bool(result3, j, 4, &file->check_ok);

					so_database_postgresql_get_ssize(result3, j, 15, &file->size);
					so_database_postgresql_get_string_dup(result3, j, 16, &file->selected_path);
					file->digests = so_value_new_hashtable2();

					file->db_data = so_value_new_hashtable(so_value_custom_compute_hash);
					struct so_value * db = so_value_new_hashtable2();
					so_value_hashtable_put(file->db_data, key, false, db, true);
					so_value_hashtable_put2(db, "id", so_value_new_string(file_id), true);

					const char * query4 = "select_checksum_from_archivefile";
					so_database_postgresql_prepare(self, query4, "SELECT c.name, cr.result FROM archivefiletochecksumresult af2cr INNER JOIN checksumresult cr ON af2cr.archivefile = $1 AND af2cr.checksumresult = cr.id LEFT JOIN checksum c ON cr.checksum = c.id");

					const char * param4[] = { file_id };
					PGresult * result4 = PQexecPrepared(self->connect, query4, 1, param4, NULL, NULL, 0);
					ExecStatusType status4 = PQresultStatus(result);
					int nb_result4 = PQntuples(result4);

					if (status4 == PGRES_FATAL_ERROR)
						so_database_postgresql_get_error(result4, query4);
					else if (status4 == PGRES_TUPLES_OK && nb_result4 > 0) {
						int k;
						for (k = 0; k < nb_result4; k++)
							so_value_hashtable_put2(file->digests, PQgetvalue(result4, k, 0), so_value_new_string(PQgetvalue(result4, k, 1)), true);
					}

					PQclear(result4);
				}
			}

			PQclear(result3);
		}
	}

	PQclear(result);

	return archive;
}

static struct so_archive * so_database_postgresql_get_archive_by_job(struct so_database_connection * connect, struct so_job * job) {
	if (connect == NULL || job == NULL)
		return NULL;

	struct so_database_postgresql_connection_private * self = connect->data;

	char * job_id = NULL;
	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = so_value_hashtable_get(job->db_data, key, false, false);
	so_value_unpack(db, "{ss}", "id", &job_id);

	struct so_archive * archive = NULL;

	const char * query = "select_archive_by_job";
	so_database_postgresql_prepare(self, query, "SELECT archive FROM job j WHERE j.id = $1 LIMIT 1");

	const char * param[] = { job_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		archive = so_database_postgresql_get_archive_by_id(connect, PQgetvalue(result, 0, 0));

	PQclear(result);
	free(job_id);

	return archive;
}

static struct so_value * so_database_postgresql_get_archive_by_media(struct so_database_connection * connect, struct so_media * media) {
	if (connect == NULL || media == NULL)
		return NULL;

	struct so_database_postgresql_connection_private * self = connect->data;

	char * media_id = NULL;
	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = so_value_hashtable_get(media->db_data, key, false, false);
	so_value_unpack(db, "{ss}", "id", &media_id);

	const char * query = "select_archive_by_media";
	so_database_postgresql_prepare(self, query, "SELECT DISTINCT archive FROM archivevolume WHERE media = $1");

	const char * param[] = { media_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	struct so_value * archives = so_value_new_array(nb_result);
	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		int i;
		for (i = 0; i < nb_result; i++) {
			struct so_archive * archive = so_database_postgresql_get_archive_by_id(connect, PQgetvalue(result, i, 0));
			so_value_list_push(archives, so_value_new_custom(archive, so_archive_free2), true);
		}
	}

	PQclear(result);
	free(media_id);

	return archives;
}

static unsigned int so_database_postgresql_get_nb_volumes_of_file(struct so_database_connection * connect, struct so_archive * archive, struct so_archive_file * file) {
	if (connect == NULL || archive == NULL || file == NULL)
		return 0;

	struct so_database_postgresql_connection_private * self = connect->data;
	struct so_value * key = so_value_new_custom(connect->config, NULL);

	char * archive_id = NULL, * file_id = NULL;
	struct so_value * db = so_value_hashtable_get(archive->db_data, key, false, false);
	so_value_unpack(db, "{ss}", "id", &archive_id);

	db = so_value_hashtable_get(file->db_data, key, false, false);
	so_value_unpack(db, "{ss}", "id", &file_id);

	const char * query = "select_nb_volumes_of_file";
	so_database_postgresql_prepare(self, query, "SELECT COUNT(*) FROM archivefiletoarchivevolume afv INNER JOIN archivevolume av ON afv.archivefile = $2 AND afv.archivevolume = av.id AND av.archive = $1");

	const char * param[] = { archive_id, file_id };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	unsigned int nb_volumes = 0;
	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) > 0)
		so_database_postgresql_get_uint(result, 0, 0, &nb_volumes);


	PQclear(result);

	return nb_volumes;
}

static int so_database_postgresql_link_archives(struct so_database_connection * connect, struct so_archive * source, struct so_archive * copy) {
	if (connect == NULL || source == NULL || copy == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;

	char * src_archive_id = NULL, * copy_archive_id = NULL, * copy_poolmirror_id = NULL, * archivemirror_id = NULL, * last_update = NULL;
	struct so_value * key = so_value_new_custom(connect->config, NULL);

	struct so_value * db = so_value_hashtable_get(source->db_data, key, false, false);
	so_value_unpack(db, "{ss}", "id", &src_archive_id);

	db = so_value_hashtable_get(copy->db_data, key, false, false);
	so_value_unpack(db, "{ss}", "id", &copy_archive_id);

	so_value_free(key);


	const char * query = "select_poolmirror_by_archive";
	so_database_postgresql_prepare(self, query, "SELECT p.poolmirror FROM archivevolume av INNER JOIN media m ON av.archive = $1 AND av.sequence = 0 AND av.media = m.id INNER JOIN pool p ON m.pool = p.id LIMIT 1");

	const char * param_1[] = { copy_archive_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param_1, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		so_database_postgresql_get_string_dup(result, 0, 0, &copy_poolmirror_id);

	PQclear(result);

	if (status == PGRES_FATAL_ERROR) {
		free(src_archive_id);
		free(copy_archive_id);
		return 1;
	}

	if (copy_poolmirror_id != NULL) {
		query = "select_archivemirror_by_archive_and_poolmirror";
		so_database_postgresql_prepare(self, query, "SELECT am.id, aam.lastupdate FROM archivetoarchivemirror aam INNER JOIN archivemirror am ON aam.archive = $1 AND aam.archivemirror = am.id AND am.poolmirror = $2 LIMIT 1");

		const char * param_2[] = { src_archive_id, copy_poolmirror_id };
		result = PQexecPrepared(self->connect, query, 2, param_2, NULL, NULL, 0);
		status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			so_database_postgresql_get_string_dup(result, 0, 0, &archivemirror_id);
			so_database_postgresql_get_string_dup(result, 0, 1, &last_update);
		}

		if (status == PGRES_FATAL_ERROR) {
			free(src_archive_id);
			free(copy_archive_id);
			free(copy_poolmirror_id);
			return 2;
		}

		PQclear(result);

		if (archivemirror_id != NULL) {
			query = "insert_archive_mirror";
			so_database_postgresql_prepare(self, query, "INSERT INTO archivetoarchivemirror(archive, archivemirror, lastupdate) VALUES ($1, $2, $3)");

			const char * param_3[] = { copy_archive_id, archivemirror_id, last_update };
			result = PQexecPrepared(self->connect, query, 3, param_3, NULL, NULL, 0);
			status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				so_database_postgresql_get_error(result, query);

			PQclear(result);

			free(src_archive_id);
			free(copy_archive_id);
			free(archivemirror_id);
			free(last_update);

			return status != PGRES_COMMAND_OK;
		}
	}

	query = "insert_archive_mirror2";
	so_database_postgresql_prepare(self, query, "INSERT INTO archivemirror(id, poolmirror) VALUES (DEFAULT, NULL) RETURNING id");

	result = PQexecPrepared(self->connect, query, 0, NULL, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		so_database_postgresql_get_string_dup(result, 0, 0, &archivemirror_id);

	PQclear(result);

	if (archivemirror_id != NULL) {
		query = "insert_archive_mirror3";
		so_database_postgresql_prepare(self, query, "INSERT INTO archivetoarchivemirror(archive, archivemirror) VALUES ($1, $3), ($2, $3)");

		const char * param_3[] = { src_archive_id, copy_poolmirror_id, archivemirror_id };

		result = PQexecPrepared(self->connect, query, 3, param_3, NULL, NULL, 0);
		status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);

		PQclear(result);

		free(archivemirror_id);
	}

	free(src_archive_id);
	free(copy_archive_id);

	return status != PGRES_COMMAND_OK;
}

static int so_database_postgresql_mark_archive_as_purged(struct so_database_connection * connect, struct so_media * media, struct so_job * job) {
	if (connect == NULL || media == NULL || job == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;

	char * jobrun_id = NULL, * media_id = NULL;
	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = so_value_hashtable_get(media->db_data, key, false, false);
	so_value_unpack(db, "{ss}", "id", &media_id);

	db = so_value_hashtable_get(job->db_data, key, false, false);
	so_value_unpack(db, "{ss}", "jobrun id", &jobrun_id);


	const char * query = "mark_archive_as_purged";
	so_database_postgresql_prepare(self, query, "UPDATE archivevolume SET purged = $2 WHERE media = $1 AND purged IS NULL");

	const char * param[] = { media_id, jobrun_id };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	PQclear(result);
	free(jobrun_id);
	free(media_id);

	return status != PGRES_COMMAND_OK;
}

static int so_database_postgresql_sync_archive(struct so_database_connection * connect, struct so_archive * archive) {
	if (connect == NULL || archive == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = NULL;

	char * archive_id = NULL;
	if (archive->db_data != NULL) {
		db = so_value_hashtable_get(archive->db_data, key, false, false);
		so_value_unpack(db, "{ss}", "id", &archive_id);
	} else {
		archive->db_data = so_value_new_hashtable(so_value_custom_compute_hash);

		db = so_value_new_hashtable2();
		so_value_hashtable_put(archive->db_data, key, true, db, true);
	}

	if (archive_id == NULL) {
		const char * query = "insert_archive";
		so_database_postgresql_prepare(self, query, "WITH u AS (SELECT $1::UUID, $2::TEXT, id, id FROM users WHERE login = $3 LIMIT 1) INSERT INTO archive(uuid, name, owner, creator) SELECT * FROM u RETURNING id");

		const char * param[] = { archive->uuid, archive->name, archive->creator };
		PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK) {
			so_database_postgresql_get_string_dup(result, 0, 0, &archive_id);
			so_value_hashtable_put2(db, "id", so_value_new_string(archive_id), true);
		}

		PQclear(result);
	}

	if (archive_id == NULL) {
		free(archive_id);
		return 1;
	}

	int failed = 0;
	unsigned int i;
	for (i = 0; failed == 0 && i < archive->nb_volumes; i++)
		failed = so_database_postgresql_sync_archive_volume(connect, archive_id, archive->volumes + i);

	free(archive_id);

	return failed;
}

static int so_database_postgresql_sync_archive_file(struct so_database_connection * connect, struct so_archive_file * file, char ** file_id) {
	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = NULL;

	if (file->db_data != NULL) {
		db = so_value_hashtable_get(file->db_data, key, false, false);
		so_value_unpack(db, "{ss}", "id", file_id);
		so_value_free(key);
		return 0;
	} else {
		file->db_data = so_value_new_hashtable(so_value_custom_compute_hash);

		db = so_value_new_hashtable2();
		so_value_hashtable_put(file->db_data, key, true, db, true);
	}

	struct so_database_postgresql_connection_private * self = connect->data;

	const char * query = "insert_archive_file";
	so_database_postgresql_prepare(self, query, "INSERT INTO archivefile(name, type, mimetype, ownerid, owner, groupid, groups, perm, ctime, mtime, size, parent) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12) RETURNING id");

	char * perm = NULL, * ownerid = NULL, * groupid = NULL, create_time[32] = "", modify_time[32] = "", * size = NULL;
	char * selected_path_id = so_database_postgresql_find_selected_path(connect, file->selected_path);

	asprintf(&ownerid, "%d", file->ownerid);
	asprintf(&groupid, "%d", file->groupid);
	asprintf(&perm, "%d", file->perm);
	so_time_convert(&file->create_time, "%F %T", create_time, 32);
	so_time_convert(&file->modify_time, "%F %T", modify_time, 32);
	asprintf(&size, "%zd", file->size);

	const char * param[] = { file->path, so_archive_file_type_to_string(file->type, false), file->mime_type, ownerid, file->owner, groupid, file->group, perm, create_time, modify_time, size, selected_path_id };
	PGresult * result = PQexecPrepared(self->connect, query, 12, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK) {
		so_database_postgresql_get_string_dup(result, 0, 0, file_id);
		so_value_hashtable_put2(db, "id", so_value_new_string(*file_id), true);
	}

	PQclear(result);
	free(selected_path_id);
	free(size);
	free(groupid);
	free(ownerid);
	free(perm);

	if (status != PGRES_TUPLES_OK)
		return 1;

	const char * queryB = "insert_archivefile_to_checksum";
	so_database_postgresql_prepare(self, queryB, "INSERT INTO archivefiletochecksumresult VALUES ($1, $2)");

	struct so_value_iterator * iter = so_value_hashtable_get_iterator(file->digests);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * key = so_value_iterator_get_key(iter, false, false);
		struct so_value * value = so_value_iterator_get_value(iter, false);

		const char * checksum = so_value_string_get(key);
		const char * digest = so_value_string_get(value);

		char * digest_id = NULL;
		if (so_database_postgresql_checksumresult_add(connect, checksum, digest, &digest_id) == 0) {
			const char * param[] = { *file_id, digest_id };
			PGresult * result = PQexecPrepared(self->connect, queryB, 2, param, NULL, NULL, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				so_database_postgresql_get_error(result, queryB);

			PQclear(result);
		}

		free(digest_id);
	}
	so_value_iterator_free(iter);

	return status != PGRES_TUPLES_OK;
}

static int so_database_postgresql_sync_archive_volume(struct so_database_connection * connect, char * archive_id, struct so_archive_volume * volume) {
	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = NULL;

	char * volume_id = NULL;
	if (volume->db_data != NULL) {
		db = so_value_hashtable_get(volume->db_data, key, false, false);
		so_value_unpack(db, "{ss}", "id", &volume_id);
	} else {
		volume->db_data = so_value_new_hashtable(so_value_custom_compute_hash);

		db = so_value_new_hashtable2();
		so_value_hashtable_put(volume->db_data, key, true, db, true);
	}

	if (volume_id == NULL) {
		char * sequence = NULL;
		asprintf(&sequence, "%u", volume->sequence);

		const char * query = "select_archive_volume_by_archive";
		so_database_postgresql_prepare(self, query, "SELECT id FROM archivevolume WHERE archive = $1 AND sequence = $2");

		const char * param[] = { archive_id, sequence };
		PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			so_database_postgresql_get_string_dup(result, 0, 0, &volume_id);
			so_value_hashtable_put2(db, "id", so_value_new_string(volume_id), true);
			so_value_hashtable_put2(db, "archive id", so_value_new_string(archive_id), true);
		}

		PQclear(result);
		free(sequence);
	}

	if (volume_id == NULL) {
		so_database_postgresql_sync_media(connect, volume->media, so_database_sync_id_only);
		struct so_value * db_media = so_value_hashtable_get(volume->media->db_data, key, false, false);
		struct so_value * db_job = so_value_hashtable_get(volume->job->db_data, key, false, false);

		char * sequence = NULL, * size = NULL, starttime[32] = "", endtime[32] = "", * media_id = NULL, * media_position = NULL, * job_run = NULL;
		asprintf(&sequence, "%u", volume->sequence);
		asprintf(&size, "%zd", volume->size);
		so_time_convert(&volume->start_time, "%F %T", starttime, 32);
		so_time_convert(&volume->end_time, "%F %T", endtime, 32);
		so_value_unpack(db_media, "{ss}", "id", &media_id);
		asprintf(&media_position, "%u", volume->media_position);
		so_value_unpack(db_job, "{ss}", "jobrun id", &job_run);

		const char * query = "insert_archive_volume";
		so_database_postgresql_prepare(self, query, "INSERT INTO archivevolume(sequence, size, starttime, endtime, archive, media, mediaposition, jobrun) VALUES ($1, $2, $3, $4, $5, $6, $7, $8) RETURNING id");

		const char * param[] = { sequence, size, starttime, endtime, archive_id, media_id, media_position, job_run };
		PGresult * result = PQexecPrepared(self->connect, query, 8, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK) {
			so_database_postgresql_get_string_dup(result, 0, 0, &volume_id);
			so_value_hashtable_put2(db, "id", so_value_new_string(volume_id), true);
		}

		PQclear(result);
		free(job_run);
		free(media_position);
		free(media_id);
		free(size);
		free(sequence);
	}

	if (volume_id != NULL) {
		if (volume->digests != NULL) {
			const char * query = "insert_archive_volume_to_checksum";
			so_database_postgresql_prepare(self, query, "INSERT INTO archivevolumetochecksumresult VALUES ($1, $2)");

			struct so_value_iterator * iter = so_value_hashtable_get_iterator(volume->digests);
			while (so_value_iterator_has_next(iter)) {
				struct so_value * key = so_value_iterator_get_key(iter, false, false);
				struct so_value * value = so_value_iterator_get_value(iter, false);

				const char * checksum = so_value_string_get(key);
				const char * digest = so_value_string_get(value);

				char * digest_id = NULL;
				if (so_database_postgresql_checksumresult_add(connect, checksum, digest, &digest_id) == 0) {
					const char * param[] = { volume_id, digest_id };
					PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
					ExecStatusType status = PQresultStatus(result);

					if (status == PGRES_FATAL_ERROR)
						so_database_postgresql_get_error(result, query);

					PQclear(result);
				}

				free(digest_id);
			}
			so_value_iterator_free(iter);
		}

		const char * queryA = "insert_archivefiletoarchivevolume";
		so_database_postgresql_prepare(self, queryA, "INSERT INTO archivefiletoarchivevolume(archivevolume, archivefile, blocknumber, archivetime) VALUES ($1, $2, $3, $4)");

		unsigned int i;
		for (i = 0; i < volume->nb_files; i++) {
			struct so_archive_files * ptr_file = volume->files + i;
			struct so_archive_file * file = ptr_file->file;

			char * file_id = NULL;
			so_database_postgresql_sync_archive_file(connect, file, &file_id);

			char * block_number = NULL, archive_time[32] = "";
			asprintf(&block_number, "%zd", ptr_file->position);
			so_time_convert(&ptr_file->archived_time, "%F %T", archive_time, 32);

			const char * paramA[] = { volume_id, file_id, block_number, archive_time };
			PGresult * resultA = PQexecPrepared(self->connect, queryA, 4, paramA, NULL, NULL, 0);
			ExecStatusType statusA = PQresultStatus(resultA);

			if (statusA == PGRES_FATAL_ERROR)
				so_database_postgresql_get_error(resultA, queryA);

			PQclear(resultA);
			free(block_number);
			free(file_id);
		}
	}

	free(volume_id);

	return 0;
}


static int so_database_postgresql_backup_add(struct so_database_connection * connect, struct so_backup * backup) {
	if (connect == NULL || backup == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;
	char * query = "select_count_archives";
	so_database_postgresql_prepare(self, query, "SELECT COUNT(*) FROM archive WHERE NOT deleted");

	PGresult * result = PQexecPrepared(self->connect, query, 0, NULL, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK)
		so_database_postgresql_get_long(result, 0, 0, &backup->nb_archives);
	else
		backup->nb_archives = 0;

	PQclear(result);

	if (status == PGRES_FATAL_ERROR)
		return -2;

	query = "select_count_medias";
	so_database_postgresql_prepare(self, query, "SELECT COUNT(*) FROM media");

	result = PQexecPrepared(self->connect, query, 0, NULL, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK)
		so_database_postgresql_get_long(result, 0, 0, &backup->nb_medias);
	else
		backup->nb_medias = 0;

	PQclear(result);

	if (status == PGRES_FATAL_ERROR)
		return -3;

	query = "insert_backup";
	so_database_postgresql_prepare(self, query, "INSERT INTO backup(nbmedia, nbarchive, jobrun) VALUES ($1, $2, $3) RETURNING id, timestamp");

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = so_value_hashtable_get(backup->job->db_data, key, false, false);
	so_value_free(key);

	char * jobrun_id = NULL;
	so_value_unpack(db, "{ss}", "jobrun id", &jobrun_id);

	char * backup_id = NULL, * nbmedia, * nbarchives;
	asprintf(&nbmedia, "%ld", backup->nb_medias);
	asprintf(&nbarchives, "%ld", backup->nb_archives);

	const char * param[] = { nbmedia, nbarchives, jobrun_id };
	result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK) {
		so_database_postgresql_get_string_dup(result, 0, 0, &backup_id);
		so_database_postgresql_get_time(result, 0, 1, &backup->timestamp);
	}

	PQclear(result);
	free(jobrun_id);
	free(nbmedia);
	free(nbarchives);

	if (status == PGRES_FATAL_ERROR) {
		free(backup_id);
		return -4;
	}

	query = "insert_backup_volume";
	so_database_postgresql_prepare(self, query, "INSERT INTO backupvolume(sequence, size, media, mediaposition, backup) VALUES ($1, $2, $3, $4, $5) RETURNING id");

	unsigned int i;
	for (i = 0; i < backup->nb_volumes; i++) {
		struct so_backup_volume * bv = backup->volumes + i;

		so_database_postgresql_sync_media(connect, bv->media, so_database_sync_id_only);

		struct so_value * key = so_value_new_custom(connect->config, NULL);
		struct so_value * media_db = so_value_hashtable_get(bv->media->db_data, key, false, false);
		so_value_free(key);

		char * seq, * size, * media_id, * media_position;
		asprintf(&seq, "%u", i);
		asprintf(&size, "%zd", bv->size);
		so_value_unpack(media_db, "{ss}", "id", &media_id);
		asprintf(&media_position, "%u", bv->position);

		const char * param[] = { seq, size, media_id, media_position, backup_id };
		result = PQexecPrepared(self->connect, query, 5, param, NULL, NULL, 0);
		status = PQresultStatus(result);

		char * backupvolume_id = NULL;

		if (status == PGRES_FATAL_ERROR)
			so_database_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			so_database_postgresql_get_string_dup(result, 0, 0, &backupvolume_id);

		PQclear(result);
		free(media_position);
		free(media_id);
		free(seq);
		free(size);

		if (backupvolume_id != NULL && so_value_hashtable_get_length(bv->digests) > 0) {
			struct so_value_iterator * iter = so_value_hashtable_get_iterator(bv->digests);
			while (so_value_iterator_has_next(iter)) {
				struct so_value * key = so_value_iterator_get_key(iter, false, false);
				struct so_value * value = so_value_iterator_get_value(iter, false);

				char * digest_id = NULL;
				int failed = so_database_postgresql_checksumresult_add(connect, so_value_string_get(key), so_value_string_get(value), &digest_id);
				if (failed != 0)
					continue;

				const char * query2 = "insert_backup_volume_checksum_result";
				so_database_postgresql_prepare(self, query2, "INSERT INTO backupvolumetochecksumresult VALUES ($1, $2)");

				const char * param2[] = { backupvolume_id, digest_id };
				result = PQexecPrepared(self->connect, query2, 2, param2, NULL, NULL, 0);
				status = PQresultStatus(result);

				if (status == PGRES_FATAL_ERROR)
					so_database_postgresql_get_error(result, query2);

				PQclear(result);
				free(digest_id);
			}
		}

		free(backupvolume_id);
	}

	free(backup_id);

	return status == PGRES_FATAL_ERROR ? -5 : 0;
}

static struct so_backup * so_database_postgresql_get_backup(struct so_database_connection * connect, struct so_job * job) {
	if (connect == NULL || job == NULL)
		return NULL;

	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = so_value_hashtable_get(job->db_data, key, false, false);

	char * job_id = NULL;
	so_value_unpack(db, "{ss}", "id", &job_id);

	const char * query = "select_backup_by_job";
	so_database_postgresql_prepare(self, query, "SELECT b.id, b.timestamp, b.nbmedia, b.nbarchive FROM backup b INNER JOIN job j ON j.id = $1 AND b.id = j.backup LIMIT 1");

	const char * param[] = { job_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	struct so_backup * backup = NULL;
	char * backup_id = NULL;

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK) {
		backup = malloc(sizeof(struct so_backup));
		bzero(backup, sizeof(struct so_backup));

		so_database_postgresql_get_string_dup(result, 0, 0, &backup_id);
		so_database_postgresql_get_time(result, 0, 1, &backup->timestamp);
		so_database_postgresql_get_long(result, 0, 2, &backup->nb_medias);
		so_database_postgresql_get_long(result, 0, 3, &backup->nb_archives);

		backup->db_data = so_value_new_hashtable(so_value_custom_compute_hash);
		struct so_value * db = so_value_new_hashtable2();
		so_value_hashtable_put(backup->db_data, key, false, db, true);
		so_value_hashtable_put2(db, "id", so_value_new_string(backup_id), true);
	}

	PQclear(result);
	free(job_id);

	if (status == PGRES_FATAL_ERROR) {
		so_value_free(key);
		return NULL;
	}

	query = "select_backupvolume_by_backup";
	so_database_postgresql_prepare(self, query, "SELECT bv.id, m.mediumserialnumber, bv.size, bv.mediaposition, bv.checktime, bv.checksumok FROM backupvolume bv INNER JOIN media m ON bv.media = m.id WHERE bv.backup = $1 ORDER BY bv.sequence");

	const char * param2[] = { backup_id };
	result = PQexecPrepared(self->connect, query, 1, param2, NULL, NULL, 0);
	status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		backup->volumes = calloc(nb_result, sizeof(struct so_backup_volume));
		backup->nb_volumes = nb_result;

		int i;
		for (i = 0; i < nb_result; i++) {
			struct so_backup_volume * vol = backup->volumes + i;

			char * backupvolume_id = PQgetvalue(result, i, 0);
			char * media_id = PQgetvalue(result, i, 1);
			vol->media = so_database_postgresql_get_media(connect, media_id, NULL, NULL);
			so_database_postgresql_get_size(result, i, 2, &vol->size);
			so_database_postgresql_get_uint(result, i, 3, &vol->position);
			so_database_postgresql_get_time(result, i, 4, &vol->checktime);
			so_database_postgresql_get_bool(result, i, 5, &vol->checksum_ok);
			vol->digests = so_value_new_hashtable2();

			vol->db_data = so_value_new_hashtable(so_value_custom_compute_hash);
			struct so_value * db = so_value_new_hashtable2();
			so_value_hashtable_put(vol->db_data, key, false, db, true);
			so_value_hashtable_put2(db, "id", so_value_new_string(backupvolume_id), true);
			so_value_hashtable_put2(db, "backup id", so_value_new_string(backup_id), true);

			const char * query3 = "select_checksum_from_backupvolume";
			so_database_postgresql_prepare(self, query3, "SELECT c.name, cr.result FROM backupvolumetochecksumresult bv2cr INNER JOIN checksumresult cr ON bv2cr.backupvolume = $1 AND bv2cr.checksumresult = cr.id LEFT JOIN checksum c ON cr.checksum = c.id");

			const char * param3[] = { backupvolume_id };
			PGresult * result3 = PQexecPrepared(self->connect, query3, 1, param3, NULL, NULL, 0);
			ExecStatusType status3 = PQresultStatus(result);
			int nb_result3 = PQntuples(result3);

			if (status == PGRES_FATAL_ERROR)
				so_database_postgresql_get_error(result3, query3);
			else if (status3 == PGRES_TUPLES_OK && nb_result3 > 0) {
				int j;
				for (j = 0; j < nb_result3; j++)
					so_value_hashtable_put2(vol->digests, PQgetvalue(result3, j, 0), so_value_new_string(PQgetvalue(result3, j, 1)), true);
			}

			PQclear(result3);
		}
	}

	PQclear(result);
	free(backup_id);
	so_value_free(key);

	return backup;
}

static int so_database_postgresql_mark_backup_volume_checked(struct so_database_connection * connect, struct so_backup_volume * volume) {
	if (connect == NULL || volume == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;

	struct so_value * key = so_value_new_custom(connect->config, NULL);
	struct so_value * db = so_value_hashtable_get(volume->db_data, key, false, false);

	char * backup_volume_id = NULL;
	so_value_unpack(db, "{ss}", "id", &backup_volume_id);

	char * query = "update_backup_volume";
	so_database_postgresql_prepare(self, query, "UPDATE backupvolume SET checktime = $1, checksumok = $2 WHERE id = $3");

	char checktime[24];
	so_time_convert(&volume->checktime, "%F %T", checktime, 24);

	const char * param[] = { checktime, so_database_postgresql_bool_to_string(volume->checksum_ok), backup_volume_id };
	PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);

	free(backup_volume_id);

	return status == PGRES_FATAL_ERROR ? 1 : 0;
}


static int so_database_postgresql_checksumresult_add(struct so_database_connection * connect, const char * checksum, const char * digest, char ** digest_id) {
	if (connect == NULL || checksum == NULL || digest == NULL || digest_id == NULL)
		return -1;

	struct so_database_postgresql_connection_private * self = connect->data;

	char * query = "select_checksum_by_name";
	so_database_postgresql_prepare(self, query, "SELECT id FROM checksum WHERE name = $1 LIMIT 1");

	const char * param1[] = { checksum };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param1, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	char * checksum_id;

	if (status == PGRES_FATAL_ERROR) {
		so_database_postgresql_get_error(result, query);
		PQclear(result);
		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		so_database_postgresql_get_string_dup(result, 0, 0, &checksum_id);
	} else {
		PQclear(result);
		return 2;
	}
	PQclear(result);

	query = "select_checksumresult";
	so_database_postgresql_prepare(self, query, "SELECT id FROM checksumresult WHERE checksum = $1 AND result = $2 LIMIT 1");

	const char * param2[] = { checksum_id, digest };
	result = PQexecPrepared(self->connect, query, 2, param2, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR) {
		so_database_postgresql_get_error(result, query);
		PQclear(result);
		free(checksum_id);
		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		so_database_postgresql_get_string_dup(result, 0, 0, digest_id);
		PQclear(result);
		free(checksum_id);
		return 0;
	}
	PQclear(result);

	query = "insert_checksumresult";
	so_database_postgresql_prepare(self, query, "INSERT INTO checksumresult(checksum, result) VALUES ($1, $2) RETURNING id");

	result = PQexecPrepared(self->connect, query, 2, param2, NULL, NULL, 0);
	status = PQresultStatus(result);

	free(checksum_id);

	if (status == PGRES_FATAL_ERROR) {
		so_database_postgresql_get_error(result, query);
		PQclear(result);
		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		so_database_postgresql_get_string_dup(result, 0, 0, digest_id);
		PQclear(result);
		return 0;
	} else {
		PQclear(result);
		return 2;
	}
}


static char * so_database_postgresql_find_selected_path(struct so_database_connection * connect, const char * selected_path) {
	struct so_database_postgresql_connection_private * self = connect->data;

	char * query = "select_selectedfile_by_name";
	so_database_postgresql_prepare(self, query, "SELECT id FROM selectedfile WHERE path = $1 LIMIT 1");

	const char * param[] = { selected_path };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	char * selected_path_id = NULL;

	if (status == PGRES_FATAL_ERROR)
		so_database_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		so_database_postgresql_get_string_dup(result, 0, 0, &selected_path_id);

	PQclear(result);

	return selected_path_id;
}


static void so_database_postgresql_prepare(struct so_database_postgresql_connection_private * self, const char * statement_name, const char * query) {
	if (!so_value_hashtable_has_key2(self->cached_query, statement_name)) {
		PGresult * prepare = PQprepare(self->connect, statement_name, query, 0, NULL);
		ExecStatusType status = PQresultStatus(prepare);
		if (status == PGRES_FATAL_ERROR) {
			so_database_postgresql_get_error(prepare, statement_name);
			so_log_write2(status == PGRES_COMMAND_OK ? so_log_level_debug : so_log_level_error, so_log_type_plugin_db, dgettext("libstoriqone-database-postgresql", "PSQL: new query prepared (%s) => {%s}, status: %s"), statement_name, query, PQresStatus(status));
		} else
			so_value_hashtable_put2(self->cached_query, statement_name, so_value_new_string(query), true);
		PQclear(prepare);
	}
}

