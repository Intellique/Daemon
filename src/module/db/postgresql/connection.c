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
*  Last modified: Tue, 03 Jun 2014 22:51:51 +0200                            *
\****************************************************************************/

#define _GNU_SOURCE
// PQclear, PQexec, PQexecPrepared, PQfinish, PQresultStatus
// PQsetErrorVerbosity, PQtransactionStatus
#include <postgresql/libpq-fe.h>
// va_end, va_start
#include <stdarg.h>
// bool
#include <stdbool.h>
// asprintf
#include <stdio.h>
// free, malloc, realloc
#include <stdlib.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>
// uname
#include <sys/utsname.h>
// time
#include <time.h>

#include <libstone/backup.h>
#include <libstone/checksum.h>
#include <libstone/host.h>
#include <libstone/job.h>
#include <libstone/library/archive.h>
#include <libstone/library/changer.h>
#include <libstone/library/drive.h>
#include <libstone/library/media.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/library/vtl.h>
#include <libstone/log.h>
#include <libstone/script.h>
#include <libstone/user.h>
#include <libstone/util/hashtable.h>
#include <libstone/util/json.h>
#include <libstone/util/string.h>
#include <libstone/util/time.h>
#include <libstone/util/util.h>

#include "common.h"

struct st_db_postgresql_connection_private {
	PGconn * connect;
	struct st_hashtable * cached_query;
};

struct st_db_postgresql_archive_data {
	long id;
};

struct st_db_postgresql_archive_file_data {
	long id;
};

struct st_db_postgresql_archive_volume_data {
	long id;
};

struct st_db_postgresql_changer_data {
	long id;
};

struct st_db_postgresql_drive_data {
	long id;
};

struct st_db_postgresql_job_data {
	long job_id;
	long jobrun_id;
};

struct st_db_postgresql_media_data {
	long id;
};

struct st_db_postgresql_pool_data {
	long id;
};

struct st_db_postgresql_selected_path_data {
	long id;
};

struct st_db_postgresql_slot_data {
	long id;
};

struct st_db_postgresql_user_data {
	long id;
};

static int st_db_postgresql_close(struct st_database_connection * connect);
static int st_db_postgresql_free(struct st_database_connection * connect);
static int st_db_postgresql_is_connection_closed(struct st_database_connection * connect);

static int st_db_postgresql_add_backup(struct st_database_connection * connect, struct st_backup * backup);
static int st_db_postgresql_cancel_checkpoint(struct st_database_connection * connect, const char * checkpoint);
static int st_db_postgresql_cancel_transaction(struct st_database_connection * connect);
static int st_db_postgresql_create_checkpoint(struct st_database_connection * connect, const char * checkpoint);
static int st_db_postgresql_finish_transaction(struct st_database_connection * connect);
static int st_db_postgresql_start_transaction(struct st_database_connection * connect);

static int st_db_postgresql_sync_plugin_checksum(struct st_database_connection * connect, const char * name);
static int st_db_postgresql_sync_plugin_job(struct st_database_connection * connect, const char * plugin);

static int st_db_postgresql_add_host(struct st_database_connection * connect, const char * uuid, const char * name, const char * domaine, const char * description);
static bool st_db_postgresql_find_host(struct st_database_connection * connect, const char * uuid, const char * hostname);
static char * st_db_postgresql_get_host(struct st_database_connection * connect);
static int st_db_postgresql_get_host_by_name(struct st_database_connection * connect, const char * name, struct st_host * host);
static int st_db_postgresql_update_host_timestamp(struct st_database_connection * connect);

static int st_db_postgresql_get_nb_scripts(struct st_database_connection * connect, const char * job_type, enum st_script_type type, struct st_pool * pool);
static char * st_db_postgresql_get_script(struct st_database_connection * connect, const char * job_type, unsigned int sequence, enum st_script_type type, struct st_pool * pool);
static int st_db_postgresql_sync_script(struct st_database_connection * connect, const char * script_path);

static bool st_db_postgresql_changer_is_enabled(struct st_database_connection * connect, struct st_changer * changer);
static bool st_db_postgresql_drive_is_enabled(struct st_database_connection * connect, struct st_drive * drive);
static int st_db_postgresql_is_changer_contain_drive(struct st_database_connection * connect, struct st_changer * changer, struct st_drive * drive);
static void st_db_postgresql_slots_are_enabled(struct st_database_connection * connect, struct st_changer * changer);
static int st_db_postgresql_sync_changer(struct st_database_connection * connect, struct st_changer * changer);
static int st_db_postgresql_sync_drive(struct st_database_connection * connect, struct st_drive * drive);

static ssize_t st_db_postgresql_get_available_size_of_offline_media_from_pool(struct st_database_connection * connect, struct st_pool * pool);
static char ** st_db_postgresql_get_checksums_by_pool(struct st_database_connection * connect, struct st_pool * pool, unsigned int * nb_checksums);
static struct st_media * st_db_postgresql_get_media(struct st_database_connection * connect, struct st_job * job, const char * uuid, const char * medium_serial_number, const char * label);
static int st_db_postgresql_get_media_format(struct st_database_connection * connect, struct st_media_format * media_format, unsigned char density_code, const char * name, enum st_media_format_mode mode);
static struct st_pool * st_db_postgresql_get_pool(struct st_database_connection * connect, struct st_archive * archive, struct st_job * job, const char * uuid);
static int st_db_postgresql_sync_media(struct st_database_connection * connect, struct st_media * media);
static int st_db_postgresql_sync_media_format(struct st_database_connection * connect, struct st_media_format * format);
static int st_db_postgresql_sync_slot(struct st_database_connection * connect, struct st_slot * slot);

static int st_db_postgresql_add_check_archive_job(struct st_database_connection * connect, struct st_job * job, struct st_archive * archive, time_t starttime, bool quick_mode);
static int st_db_postgresql_add_job_record(struct st_database_connection * connect, struct st_job * job, const char * message, enum st_job_record_notif notif);
static int st_db_postgresql_finish_job_run(struct st_database_connection * connect, struct st_job * job, time_t endtime, int exitcode);
static struct st_job_selected_path * st_db_postgresql_get_selected_paths(struct st_database_connection * connect, struct st_job * job, unsigned int * nb_paths);
static int st_db_postgresql_start_job_run(struct st_database_connection * connect, struct st_job * job, time_t starttime);
static int st_db_postgresql_sync_job(struct st_database_connection * connect, struct st_job *** jobs, unsigned int * nb_jobs);

static int st_db_postgresql_get_user(struct st_database_connection * connect, struct st_user * user, const char * login);
static int st_db_postgresql_sync_user(struct st_database_connection * connect, struct st_user * user);

static bool st_db_postgresql_add_report(struct st_database_connection * connect, struct st_job * job, struct st_archive * archive, const char * report);
static bool st_db_postgresql_check_checksums_of_archive_volume(struct st_database_connection * connect, struct st_archive_volume * volume);
static bool st_db_postgresql_check_checksums_of_file(struct st_database_connection * connect, struct st_archive_file * file);
static int st_db_postgresql_add_checksum_result(struct st_database_connection * connect, const char * checksum, char * result, char ** checksum_result_id);
static struct st_archive_file * st_db_postgresql_get_archive_file_for_restore_directory(struct st_database_connection * connect, struct st_job * job, unsigned int * nb_files);
static struct st_archive * st_db_postgresql_get_archive_by_job(struct st_database_connection * connect, struct st_job * job);
static int st_db_postgresql_get_archive_files_by_job_and_archive_volume(struct st_database_connection * connect, struct st_job * job, struct st_archive_volume * volume);
static struct st_archive * st_db_postgresql_get_archive_volumes_by_job(struct st_database_connection * connect, struct st_job * job);
static unsigned int st_db_postgresql_get_checksums_of_archive_volume(struct st_database_connection * connect, struct st_archive_volume * volume);
static unsigned int  st_db_postgresql_get_checksums_of_file(struct st_database_connection * connect, struct st_archive_file * file);
static unsigned int st_db_postgresql_get_nb_volume_of_file(struct st_database_connection * connect, struct st_job * job, struct st_archive_file * file);
static char * st_db_postgresql_get_restore_path_from_file(struct st_database_connection * connect, struct st_job * job, struct st_archive_file * file);
static char * st_db_postgresql_get_restore_path_of_job(struct st_database_connection * connect, struct st_job * job);
static ssize_t st_db_postgresql_get_restore_size_by_job(struct st_database_connection * connect, struct st_job * job);
static bool st_db_postgresql_has_restore_to_by_job(struct st_database_connection * connect, struct st_job * job);
static bool st_db_postgresql_has_selected_files_by_job(struct st_database_connection * connect, struct st_job * job);
static bool st_db_postgresql_mark_archive_file_as_checked(struct st_database_connection * connect, struct st_archive * archive, struct st_archive_file * file, bool ok);
static bool st_db_postgresql_mark_archive_volume_as_checked(struct st_database_connection * connect, struct st_archive_volume * volume, bool ok);
static int st_db_postgresql_sync_archive(struct st_database_connection * connect, struct st_archive * archive);
static int st_db_postgresql_sync_file(struct st_database_connection * connect, struct st_archive * archive, struct st_archive_file * file, char ** file_id);
static int st_db_postgresql_sync_volume(struct st_database_connection * connect, struct st_archive * archive, struct st_archive_volume * volume);

static struct st_vtl_config * st_db_postgresql_get_vtls(struct st_database_connection * connect, unsigned int * nb_vtls);
static int st_db_postgresql_delete_vtl(struct st_database_connection * connect, struct st_vtl_config * config);

static void st_db_postgresql_prepare(struct st_db_postgresql_connection_private * self, const char * statement_name, const char * query);

static struct st_database_connection_ops st_db_postgresql_connection_ops = {
	.close                = st_db_postgresql_close,
	.free                 = st_db_postgresql_free,
	.is_connection_closed = st_db_postgresql_is_connection_closed,

	.add_backup         = st_db_postgresql_add_backup,
	.cancel_transaction = st_db_postgresql_cancel_transaction,
	.finish_transaction = st_db_postgresql_finish_transaction,
	.start_transaction  = st_db_postgresql_start_transaction,

	.sync_plugin_checksum = st_db_postgresql_sync_plugin_checksum,
	.sync_plugin_job      = st_db_postgresql_sync_plugin_job,

	.add_host              = st_db_postgresql_add_host,
	.find_host             = st_db_postgresql_find_host,
	.get_host_by_name      = st_db_postgresql_get_host_by_name,
	.update_host_timestamp = st_db_postgresql_update_host_timestamp,

	.get_nb_scripts = st_db_postgresql_get_nb_scripts,
	.get_script     = st_db_postgresql_get_script,
	.sync_script    = st_db_postgresql_sync_script,

	.changer_is_enabled       = st_db_postgresql_changer_is_enabled,
	.drive_is_enabled         = st_db_postgresql_drive_is_enabled,
	.is_changer_contain_drive = st_db_postgresql_is_changer_contain_drive,
	.slots_are_enabled        = st_db_postgresql_slots_are_enabled,
	.sync_changer             = st_db_postgresql_sync_changer,
	.sync_drive               = st_db_postgresql_sync_drive,

	.get_available_size_of_offline_media_from_pool = st_db_postgresql_get_available_size_of_offline_media_from_pool,
	.get_checksums_by_pool                         = st_db_postgresql_get_checksums_by_pool,
	.get_media                                     = st_db_postgresql_get_media,
	.get_media_format                              = st_db_postgresql_get_media_format,
	.get_pool                                      = st_db_postgresql_get_pool,
	.sync_media                                    = st_db_postgresql_sync_media,
	.sync_media_format                             = st_db_postgresql_sync_media_format,

	.add_check_archive_job = st_db_postgresql_add_check_archive_job,
	.add_job_record        = st_db_postgresql_add_job_record,
	.finish_job_run        = st_db_postgresql_finish_job_run,
	.get_selected_paths    = st_db_postgresql_get_selected_paths,
	.start_job_run         = st_db_postgresql_start_job_run,
	.sync_job              = st_db_postgresql_sync_job,

	.get_user  = st_db_postgresql_get_user,
	.sync_user = st_db_postgresql_sync_user,

	.add_report                                  = st_db_postgresql_add_report,
	.check_checksums_of_archive_volume           = st_db_postgresql_check_checksums_of_archive_volume,
	.check_checksums_of_file                     = st_db_postgresql_check_checksums_of_file,
	.get_archive_by_job                          = st_db_postgresql_get_archive_by_job,
	.get_archive_file_for_restore_directory      = st_db_postgresql_get_archive_file_for_restore_directory,
	.get_archive_files_by_job_and_archive_volume = st_db_postgresql_get_archive_files_by_job_and_archive_volume,
	.get_archive_volumes_by_job                  = st_db_postgresql_get_archive_volumes_by_job,
	.get_checksums_of_archive_volume             = st_db_postgresql_get_checksums_of_archive_volume,
	.get_checksums_of_file                       = st_db_postgresql_get_checksums_of_file,
	.get_nb_volume_of_file                       = st_db_postgresql_get_nb_volume_of_file,
	.get_restore_path_from_file                  = st_db_postgresql_get_restore_path_from_file,
	.get_restore_path_of_job                     = st_db_postgresql_get_restore_path_of_job,
	.get_restore_size_by_job                     = st_db_postgresql_get_restore_size_by_job,
	.has_restore_to_by_job                       = st_db_postgresql_has_restore_to_by_job,
	.mark_archive_file_as_checked                = st_db_postgresql_mark_archive_file_as_checked,
	.mark_archive_volume_as_checked              = st_db_postgresql_mark_archive_volume_as_checked,
	.sync_archive                                = st_db_postgresql_sync_archive,

	.get_vtls   = st_db_postgresql_get_vtls,
	.delete_vtl = st_db_postgresql_delete_vtl,
};


static int st_db_postgresql_close(struct st_database_connection * connect) {
	struct st_db_postgresql_connection_private * self = connect->data;

	if (self->connect != NULL)
		PQfinish(self->connect);
	self->connect = NULL;

	if (self->cached_query != NULL)
		st_hashtable_free(self->cached_query);
	self->cached_query = NULL;

	return 0;
}

static int st_db_postgresql_free(struct st_database_connection * connect) {
	struct st_db_postgresql_connection_private * self = connect->data;

	st_db_postgresql_close(connect);
	free(self);

	connect->data = NULL;
	connect->driver = NULL;
	connect->config = NULL;
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

static int st_db_postgresql_is_connection_closed(struct st_database_connection * connect) {
	struct st_db_postgresql_connection_private * self = connect->data;
	return self->connect == NULL;
}


static int st_db_postgresql_add_backup(struct st_database_connection * connect, struct st_backup * backup) {
	if (connect == NULL || backup == NULL)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;
	char * query = "select_count_archives";
	st_db_postgresql_prepare(self, query, "SELECT COUNT(*) FROM archive WHERE NOT deleted");

	PGresult * result = PQexecPrepared(self->connect, query, 0, NULL, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK)
		st_db_postgresql_get_long(result, 0, 0, &backup->nb_archives);
	else
		backup->nb_archives = 0;

	PQclear(result);

	if (status == PGRES_FATAL_ERROR)
		return -2;

	query = "select_count_medias";
	st_db_postgresql_prepare(self, query, "SELECT COUNT(*) FROM media");

	result = PQexecPrepared(self->connect, query, 0, NULL, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK)
		st_db_postgresql_get_long(result, 0, 0, &backup->nb_medias);
	else
		backup->nb_medias = 0;

	PQclear(result);

	if (status == PGRES_FATAL_ERROR)
		return -3;

	query = "insert_backup";
	st_db_postgresql_prepare(self, query, "INSERT INTO backup(nbmedia, nbarchive) VALUES ($1, $2) RETURNING id, timestamp");

	char * backup_id = NULL, * nbmedia, * nbarchives;
	asprintf(&nbmedia, "%ld", backup->nb_medias);
	asprintf(&nbarchives, "%ld", backup->nb_archives);

	const char * param[] = { nbmedia, nbarchives };
	result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK) {
		st_db_postgresql_get_string_dup(result, 0, 0, &backup_id);
		st_db_postgresql_get_time(result, 0, 1, &backup->timestamp);
	}

	PQclear(result);

	if (status == PGRES_FATAL_ERROR) {
		free(backup_id);
		return -4;
	}

	query = "insert_backup_volume";
	st_db_postgresql_prepare(self, query, "INSERT INTO backupvolume(sequence, backup, media, mediaposition, jobrun) VALUES ($1, $2, $3, $4, $5)");

	unsigned int i;
	for (i = 0; i < backup->nb_volumes; i++) {
		struct st_backup_volume * bv = backup->volumes + i;
		struct st_db_postgresql_media_data * media_data = bv->media->db_data;
		struct st_db_postgresql_job_data * job_data = bv->job->db_data;

		char * seq, * media_id, * media_position, * jobrun_id;
		asprintf(&seq, "%u", i);
		asprintf(&media_id, "%ld", media_data->id);
		asprintf(&media_position, "%u", bv->position);
		asprintf(&jobrun_id, "%ld", job_data->jobrun_id);

		const char * param[] = { seq, backup_id, media_id, media_position, jobrun_id };
		result = PQexecPrepared(self->connect, query, 5, param, NULL, NULL, 0);
		status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);

		free(media_position);
		free(media_id);
		free(jobrun_id);
		free(seq);
	}

	free(backup_id);

	return status == PGRES_FATAL_ERROR ? -5 : 0;
}

static int st_db_postgresql_cancel_checkpoint(struct st_database_connection * connect, const char * checkpoint) {
	if (connect == NULL || checkpoint == NULL)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;
	char * query = NULL;
	asprintf(&query, "ROLLBACK TO %s", checkpoint);

	PGresult * result = PQexec(self->connect, query);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, NULL);

	if (status != PGRES_COMMAND_OK)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: error while rollbacking a savepoint => %s", PQerrorMessage(self->connect));

	PQclear(result);
	free(query);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

static int st_db_postgresql_cancel_transaction(struct st_database_connection * connect) {
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
				st_db_postgresql_get_error(result, NULL);
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

	struct st_db_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType transStatus = PQtransactionStatus(self->connect);

	if (transStatus == PQTRANS_INERROR) {
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: Can't create checkpoint because there is an error into current transaction");
		return -1;
	} else if (transStatus == PQTRANS_IDLE) {
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: Can't create checkpoint because there is no active transaction");
		return -1;
	}

	char * query = NULL;
	asprintf(&query, "SAVEPOINT %s", checkpoint);

	PGresult * result = PQexec(self->connect, query);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, NULL);

	if (status != PGRES_COMMAND_OK)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: error while creating a savepoint => %s", PQerrorMessage(self->connect));

	PQclear(result);
	free(query);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

static int st_db_postgresql_finish_transaction(struct st_database_connection * connect) {
	if (connect == NULL)
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
				st_db_postgresql_get_error(result, NULL);
			PQclear(result);
			return commit_status != PGRES_COMMAND_OK;
		}
		break;

		default:
			return 0;
	}
}

static int st_db_postgresql_start_transaction(struct st_database_connection * connect) {
	if (connect == NULL)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType status = PQtransactionStatus(self->connect);
	switch (status) {
		case PQTRANS_IDLE: {
			PGresult * result = PQexec(self->connect, "BEGIN");
			ExecStatusType commit_status = PQresultStatus(result);
			if (commit_status != PGRES_COMMAND_OK)
				st_db_postgresql_get_error(result, NULL);
			PQclear(result);
			return commit_status != PGRES_COMMAND_OK;
		}
		break;

		default:
			return 1;
	}
}


static int st_db_postgresql_sync_plugin_checksum(struct st_database_connection * connect, const char * name) {
	if (connect == NULL || name == NULL)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query = "select_checksum_by_name";
	st_db_postgresql_prepare(self, query, "SELECT deflt FROM checksum WHERE name = $1 LIMIT 1");

	const char * param[] = { name };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	bool found = false;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		found = true;

	PQclear(result);

	if (found)
		return 0;

	struct st_checksum_driver * driver = st_checksum_get_driver(name);
	const char * param2[] = { name, driver->default_checksum ? "TRUE" : "FALSE" };

	query = "insert_checksum";
	st_db_postgresql_prepare(self, query, "INSERT INTO checksum(name, deflt) VALUES ($1, $2)");

	result = PQexecPrepared(self->connect, query, 2, param2, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	PQclear(result);

	return status == PGRES_FATAL_ERROR;
}

static int st_db_postgresql_sync_plugin_job(struct st_database_connection * connect, const char * plugin) {
	if (connect == NULL || plugin == NULL)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query = "select_jobtype_by_name";
	st_db_postgresql_prepare(self, query, "SELECT name FROM jobtype WHERE name = $1 LIMIT 1");

	const char * param[] = { plugin };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	int found = 0;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		found = 1;

	PQclear(result);

	if (found)
		return 0;

	query = "insert_jobtype";
	st_db_postgresql_prepare(self, query, "INSERT INTO jobtype(name) VALUES ($1)");

	result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	PQclear(result);

	return status == PGRES_FATAL_ERROR;
}


static int st_db_postgresql_add_host(struct st_database_connection * connect, const char * uuid, const char * name, const char * domaine, const char * description) {
	if (connect == NULL || uuid == NULL || name == NULL)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;

	const char * query = "insert_host";
	st_db_postgresql_prepare(self, query, "INSERT INTO host(uuid, name, domaine, description) VALUES ($1, $2, $3, $4)");

	const char * param[] = { uuid, name, domaine, description };
	PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	PQclear(result);

	return status == PGRES_FATAL_ERROR;
}

static bool st_db_postgresql_find_host(struct st_database_connection * connect, const char * uuid, const char * hostname) {
	struct st_db_postgresql_connection_private * self = connect->data;

	const char * query = "select_host_by_name_or_uuid";
	st_db_postgresql_prepare(self, query, "SELECT id FROM host WHERE uuid::TEXT = $1 OR name = $2 OR name || '.' || domaine = $2 LIMIT 1");

	const char * param[] = { uuid, hostname };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	bool found = false;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (PQntuples(result) == 1)
		found = true;

	PQclear(result);

	return found;
}

static char * st_db_postgresql_get_host(struct st_database_connection * connect) {
	struct st_db_postgresql_connection_private * self = connect->data;

	const char * query = "select_host_by_name";
	st_db_postgresql_prepare(self, query, "SELECT id FROM host WHERE name = $1 OR name || '.' || domaine = $1 LIMIT 1");

	struct utsname name;
	uname(&name);

	const char * param[] = { name.nodename };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	char * hostid = NULL;

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_string_dup(result, 0, 0, &hostid);
	else
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: Host not found into database (%s)", name.nodename);

	PQclear(result);

	return hostid;
}

static int st_db_postgresql_get_host_by_name(struct st_database_connection * connect, const char * name, struct st_host * host) {
	struct st_db_postgresql_connection_private * self = connect->data;

	const char * query = "select_host_by_name_by_name";
	st_db_postgresql_prepare(self, query, "SELECT CASE WHEN domaine IS NULL THEN name ELSE name || '.' || domaine END, uuid FROM host WHERE name = $1 OR name || '.' || domaine = $1 LIMIT 1");

	const char * param[] = { name };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	bool failed = true;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		if (host->hostname != NULL)
			free(host->hostname);
		if (host->uuid != NULL)
			free(host->uuid);
		host->hostname = host->uuid = NULL;

		st_db_postgresql_get_string_dup(result, 0, 0, &host->hostname);
		st_db_postgresql_get_string_dup(result, 0, 1, &host->uuid);

		failed = false;
	}

	PQclear(result);

	return failed;
}

static int st_db_postgresql_update_host_timestamp(struct st_database_connection * connect) {
	char * hostid = st_db_postgresql_get_host(connect);
	if (hostid == NULL)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;

	const char * query = "update_host_timestamp";
	st_db_postgresql_prepare(self, query, "UPDATE host SET updated = NOW() WHERE id = $1");

	const char * param[] = { hostid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	PQclear(result);
	free(hostid);

	return status == PGRES_FATAL_ERROR;
}


static int st_db_postgresql_get_nb_scripts(struct st_database_connection * connect, const char * job_type, enum st_script_type type, struct st_pool * pool) {
	if (connect == NULL || pool == NULL)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_pool_data * pool_data = pool->db_data;

	const char * query = "select_nb_scripts_with_sequence_and_pool";
	st_db_postgresql_prepare(self, query, "SELECT COUNT(*) FROM scripts ss LEFT JOIN jobtype jt ON ss.jobType = jt.id WHERE jt.name = $1 AND scriptType = $2 AND pool = $3");

	char * pool_id;
	asprintf(&pool_id, "%ld", pool_data->id);

	const char * param[] = { job_type, st_db_postgresql_script_type_to_string(type), pool_id };
	PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	int nb_scripts = -1;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else
		st_db_postgresql_get_int(result, 0, 0, &nb_scripts);

	PQclear(result);
	free(pool_id);

	return nb_scripts;
}

static char * st_db_postgresql_get_script(struct st_database_connection * connect, const char * job_type, unsigned int sequence, enum st_script_type type, struct st_pool * pool) {
	if (connect == NULL || job_type == NULL || pool == NULL)
		return NULL;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_pool_data * pool_data = pool->db_data;

	const char * query = "select_script_with_sequence_and_pool";
	st_db_postgresql_prepare(self, query, "SELECT s.path FROM scripts ss LEFT JOIN jobtype jt ON ss.jobType = jt.id LEFT JOIN script s ON ss.script = s.id WHERE jt.name = $1 AND scripttype = $2 AND pool = $3 ORDER BY sequence LIMIT 1 OFFSET $4");

	char * seq, * pool_id;
	asprintf(&seq, "%u", sequence);
	asprintf(&pool_id, "%ld", pool_data->id);

	const char * param[] = { job_type, st_db_postgresql_script_type_to_string(type), pool_id, seq };
	PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	char * script = NULL;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (PQntuples(result) > 0)
		st_db_postgresql_get_string_dup(result, 0, 0, &script);

	PQclear(result);

	free(seq);
	free(pool_id);

	return script;
}

static int st_db_postgresql_sync_script(struct st_database_connection * connect, const char * script_path) {
	if (connect == NULL || script_path == NULL)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;

	const char * query = "select_id_from_script";
	st_db_postgresql_prepare(self, query, "SELECT id FROM script WHERE path = $1 LIMIT 1");

	const char * param[] = { script_path };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	bool found = false;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else
		found = PQntuples(result) > 0;

	PQclear(result);

	if (found)
		return 0;

	query = "insert_script";
	st_db_postgresql_prepare(self, query, "INSERT INTO script(path) VALUES ($1)");

	result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	status = PQresultStatus(result);
	PQclear(result);

	return status != PGRES_FATAL_ERROR ? 0 : -2;
}


static bool st_db_postgresql_changer_is_enabled(struct st_database_connection * connect, struct st_changer * changer) {
	if (connect == NULL || changer == NULL)
		return false;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_changer_data * changer_data = changer->db_data;
	if (changer_data == NULL) {
		changer_data = changer->db_data = malloc(sizeof(struct st_db_postgresql_changer_data));
		changer_data->id = -1;
	}

	const char * query = "select_enabled_from_changer";
	st_db_postgresql_prepare(self, query, "SELECT id, enable FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 AND wwn = $4 LIMIT 1");

	const char * param[] = { changer->model, changer->vendor, changer->serial_number, changer->wwn };
	PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	bool enabled = false;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &changer_data->id);
		st_db_postgresql_get_bool(result, 0, 1, &enabled);
	} else
		enabled = true;

	PQclear(result);
	return changer->enabled = enabled;
}

static bool st_db_postgresql_drive_is_enabled(struct st_database_connection * connect, struct st_drive * drive) {
	if (connect == NULL || drive == NULL)
		return false;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_drive_data * drive_data = drive->db_data;
	if (drive_data == NULL) {
		drive_data = drive->db_data = malloc(sizeof(struct st_db_postgresql_drive_data));
		drive_data->id = -1;
	}

	const char * query = "select_enabled_from_drive";
	st_db_postgresql_prepare(self, query, "SELECT id, enable FROM drive WHERE model = $1 AND vendor = $2 AND serialnumber = $3 LIMIT 1");

	const char * param[] = { drive->model, drive->vendor, drive->serial_number };
	PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	bool enabled = false;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &drive_data->id);
		st_db_postgresql_get_bool(result, 0, 1, &enabled);
	} else
		enabled = true;

	PQclear(result);
	return drive->enabled = enabled;
}

static int st_db_postgresql_is_changer_contain_drive(struct st_database_connection * connect, struct st_changer * changer, struct st_drive * drive) {
	if (connect == NULL || changer == NULL || drive == NULL)
		return 0;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query = "select_changer_of_drive_by_model_vendor_serialnumber";
	st_db_postgresql_prepare(self, query, "SELECT changer FROM drive WHERE model = $1 AND vendor = $2 AND serialnumber = $3 LIMIT 1");

	char * changerid = NULL;

	const char * param[] = { drive->model, drive->vendor, drive->serial_number };
	PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
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

	query = "select_changer_by_model_vendor_serialnumber_wwn_id";
	st_db_postgresql_prepare(self, query, "SELECT id FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 AND wwn = $4 AND id = $5 LIMIT 1");

	const char * params2[] = { changer->model, changer->vendor, changer->serial_number, changer->wwn, changerid };
	result = PQexecPrepared(self->connect, query, 5, params2, NULL, NULL, 0);
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

static void st_db_postgresql_slots_are_enabled(struct st_database_connection * connect, struct st_changer * changer) {
	if (connect == NULL || changer == NULL)
		return;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_changer_data * changer_data = changer->db_data;

	const char * query = "select_enable_from_changerslot";
	st_db_postgresql_prepare(self, query, "SELECT id, enable FROM changerslot WHERE changer = $1 AND index = $2 LIMIT 1");

	char * changerid;
	asprintf(&changerid, "%ld", changer_data->id);

	unsigned int i;
	for (i = 0; i < changer->nb_slots; i++) {
		struct st_slot * sl = changer->slots + i;
		struct st_db_postgresql_slot_data * sl_data = sl->db_data;
		if (sl_data == NULL) {
			sl->db_data = sl_data = malloc(sizeof(struct st_db_postgresql_slot_data));
			sl_data->id = -1;
		}

		char * index;
		asprintf(&index, "%u", i);

		const char * param[] = { changerid, index };
		PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_long(result, 0, 0, &sl_data->id);
			st_db_postgresql_get_bool(result, 0, 1, &sl->enable);
		}

		PQclear(result);
		free(index);
	}

	free(changerid);
}

static int st_db_postgresql_sync_changer(struct st_database_connection * connect, struct st_changer * changer) {
	if (connect == NULL || changer == NULL)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType transStatus = PQtransactionStatus(self->connect);
	if (transStatus == PQTRANS_INERROR)
		return 2;

	char * hostid = st_db_postgresql_get_host(connect);
	if (hostid == NULL)
		return 1;

	if (transStatus == PQTRANS_IDLE)
		st_db_postgresql_start_transaction(connect);

	struct st_db_postgresql_changer_data * changer_data = changer->db_data;
	if (changer_data == NULL) {
		changer->db_data = changer_data = malloc(sizeof(struct st_db_postgresql_changer_data));
		changer_data->id = -1;
	}

	char * changerid = NULL;
	if (changer_data->id >= 0) {
		asprintf(&changerid, "%ld", changer_data->id);

		const char * query = "select_changer_by_id";
		st_db_postgresql_prepare(self, query, "SELECT enable FROM changer WHERE id = $1 FOR UPDATE NOWAIT");

		const char * param[] = { changerid };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
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
		const char * query;
		PGresult * result;

		if (changer->wwn != NULL) {
			query = "select_changer_by_model_vendor_serialnumber_wwn";
			st_db_postgresql_prepare(self, query, "SELECT id, enable FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 AND wwn = $4 FOR UPDATE NOWAIT");

			const char * param[] = { changer->model, changer->vendor, changer->serial_number, changer->wwn };
			result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
		} else {
			query = "select_changer_by_model_vendor_serialnumber";
			st_db_postgresql_prepare(self, query, "SELECT id, enable FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 AND wwn IS NULL FOR UPDATE NOWAIT");

			const char * param[] = { changer->model, changer->vendor, changer->serial_number };
			result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
		}
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
		PGresult * result = PQexecPrepared(self->connect, query, 4, params, NULL, NULL, 0);
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
		st_db_postgresql_prepare(self, query, "INSERT INTO changer(device, status, barcode, model, vendor, firmwarerev, serialnumber, wwn, host) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9) RETURNING id");

		const char * param[] = {
			changer->device, st_changer_status_to_string(changer->status), changer->barcode ? "true" : "false",
			changer->model, changer->vendor, changer->revision, changer->serial_number, changer->wwn, hostid,
		};

		PGresult * result = PQexecPrepared(self->connect, query, 9, param, NULL, NULL, 0);
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

static int st_db_postgresql_sync_drive(struct st_database_connection * connect, struct st_drive * drive) {
	if (connect == NULL || drive == NULL)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;
	PGTransactionStatusType transStatus = PQtransactionStatus(self->connect);
	if (transStatus == PQTRANS_INERROR)
		return 2;
	else if (transStatus == PQTRANS_IDLE)
		st_db_postgresql_start_transaction(connect);

	struct st_db_postgresql_changer_data * changer_data = NULL;
	if (drive->changer != NULL)
		changer_data = drive->changer->db_data;

	struct st_db_postgresql_drive_data * drive_data = drive->db_data;
	if (drive_data == NULL) {
		drive->db_data = drive_data = malloc(sizeof(struct st_db_postgresql_drive_data));
		drive_data->id = -1;
	}

	char * changerid = NULL, * driveid = NULL, * driveformatid = NULL;
	if (changer_data != NULL)
		asprintf(&changerid, "%ld", changer_data->id);

	if (drive_data->id >= 0) {
		asprintf(&driveid, "%ld", drive_data->id);

		const char * query = "select_drive_by_id";
		st_db_postgresql_prepare(self, query, "SELECT driveformat, enable FROM drive WHERE id = $1 FOR UPDATE NOWAIT");

		const char * param[] = { driveid };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
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
			free(driveid);
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connect);
			return 3;
		}
	} else {
		const char * query = "select_drive_by_model_vendor_serialnumber";
		st_db_postgresql_prepare(self, query, "SELECT id, operationduration, lastclean, driveformat, enable FROM drive WHERE model = $1 AND vendor = $2 AND serialnumber = $3 FOR UPDATE NOWAIT");

		const char * param[] = { drive->model, drive->vendor, drive->serial_number };
		PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
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

	if (driveformatid != NULL) {
		const char * query = "select_driveformat_by_id";
		st_db_postgresql_prepare(self, query, "SELECT densitycode FROM driveformat WHERE id = $1 LIMIT 1");

		const char * param[] = { driveformatid };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		unsigned char density_code = 0;

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_uchar(result, 0, 0, &density_code);

		PQclear(result);

		if (drive->density_code > density_code) {
			free(driveformatid);
			driveformatid = NULL;
		}
	}

	if (driveformatid == NULL) {
		const char * query = "select_driveformat_by_densitycode";
		st_db_postgresql_prepare(self, query, "SELECT id FROM driveformat WHERE densitycode = $1 AND mode = $2 LIMIT 1");

		char * densitycode = NULL;
		asprintf(&densitycode, "%u", drive->density_code);

		const char * param[] = { densitycode, st_media_format_mode_to_string(drive->mode), };
		PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
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

		char * op_duration = NULL;
		asprintf(&op_duration, "%.3Lf", drive->operation_duration);

		char * last_clean = NULL;
		if (drive->last_clean > 0) {
			last_clean = malloc(24);
			st_util_time_convert(&drive->last_clean, "%F %T", last_clean, 24);
		}

		const char * param[] = {
			drive->device, drive->scsi_device, st_drive_status_to_string(drive->status),
			op_duration, last_clean, drive->revision, driveformatid, driveid,
		};
		PGresult * result = PQexecPrepared(self->connect, query, 8, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);

		PQclear(result);
		free(changerid);
		free(driveid);
		free(last_clean);
		free(driveformatid);
		free(op_duration);

		if (status == PGRES_FATAL_ERROR) {
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connect);
			return 3;
		}
	} else {
		const char * query = "insert_drive";
		st_db_postgresql_prepare(self, query, "INSERT INTO drive(device, scsidevice, status, operationduration, lastclean, model, vendor, firmwarerev, serialnumber, changer, driveformat) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11) RETURNING id");

		char * op_duration = NULL;
		asprintf(&op_duration, "%.3Lf", drive->operation_duration);

		char * last_clean = NULL;
		if (drive->last_clean > 0) {
			last_clean = malloc(24);
			st_util_time_convert(&drive->last_clean, "%F %T", last_clean, 24);
		}


		const char * param[] = {
			drive->device, drive->scsi_device, st_drive_status_to_string(drive->status), op_duration, last_clean,
			drive->model, drive->vendor, drive->revision, drive->serial_number, changerid, driveformatid,
		};
		PGresult * result = PQexecPrepared(self->connect, query, 11, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &drive_data->id);

		PQclear(result);
		free(changerid);
		free(last_clean);
		free(driveformatid);
		free(op_duration);

		if (status == PGRES_FATAL_ERROR) {
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connect);
			return 3;
		}
	}

	st_db_postgresql_create_checkpoint(connect, "sync_drive_before_slot");

	if (drive->slot != NULL && st_db_postgresql_sync_slot(connect, drive->slot))
		st_db_postgresql_cancel_checkpoint(connect, "sync_drive_before_slot");

	if (transStatus == PQTRANS_IDLE)
		st_db_postgresql_finish_transaction(connect);

	return 0;
}


static ssize_t st_db_postgresql_get_available_size_of_offline_media_from_pool(struct st_database_connection * connect, struct st_pool * pool) {
	if (connect == NULL || pool == NULL)
		return -1;

	struct st_db_postgresql_connection_private * self = connect->data;

	const char * query = "select_available_offline_size_by_pool";
	st_db_postgresql_prepare(self, query, "SELECT SUM(m.freeblock * m.blocksize::BIGINT) AS total FROM media m LEFT JOIN pool p ON m.pool = p.id WHERE m.location = 'offline' AND m.status = 'in use' AND p.uuid = $1");

	const char * param[] = { pool->uuid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	ssize_t total = 0;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1 && !PQgetisnull(result, 0, 0))
		st_db_postgresql_get_ssize(result, 0, 0, &total);

	PQclear(result);
	return total;
}

static char ** st_db_postgresql_get_checksums_by_pool(struct st_database_connection * connect, struct st_pool * pool, unsigned int * nb_checksums) {
	if (connect == NULL || pool == NULL)
		return NULL;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query = "select_checksums_by_pool";
	st_db_postgresql_prepare(self, query, "SELECT name FROM checksum WHERE id IN (SELECT checksum FROM pooltochecksum WHERE pool = $1) ORDER BY id");

	struct st_db_postgresql_pool_data * pool_data = pool->db_data;
	char * poolid;
	asprintf(&poolid, "%ld", pool_data->id);

	const char * param[] = { poolid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	if (nb_checksums != NULL)
		*nb_checksums = 0;

	char ** checksums = NULL;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else {
		unsigned int nb_tuples = PQntuples(result);
		if (nb_checksums != NULL)
			*nb_checksums = nb_tuples;
		checksums = calloc(nb_tuples + 1, sizeof(char *));

		unsigned int i;
		for (i = 0; i < nb_tuples; i++)
			st_db_postgresql_get_string_dup(result, i, 0, checksums + i);
		checksums[i] = NULL;
	}

	PQclear(result);
	free(poolid);

	return checksums;
}

static struct st_media * st_db_postgresql_get_media(struct st_database_connection * connect, struct st_job * job, const char * uuid, const char * medium_serial_number, const char * label) {
	if (connect == NULL || (job == NULL && uuid == NULL && medium_serial_number == NULL && label == NULL))
		return NULL;

	struct st_db_postgresql_connection_private * self = connect->data;

	const char * query;
	PGresult * result;

	if (job != NULL) {
		query = "select_media_by_job";
		st_db_postgresql_prepare(self, query, "SELECT m.id, m.uuid, label, mediumserialnumber, m.name, m.status, location, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, operationcount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, m.blocksize, freeblock, totalblock, m.type, nbfiles, densitycode, mode, p.uuid FROM media m LEFT JOIN mediaformat mf ON m.mediaformat = mf.id LEFT JOIN pool p ON m.pool = p.id LEFT JOIN job j ON j.media = m.id WHERE j.id = $1 LIMIT 1");

		struct st_db_postgresql_job_data * job_data = job->db_data;
		char * jobid;
		asprintf(&jobid, "%ld", job_data->job_id);

		const char * param[] = { jobid };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);

		free(jobid);
	} else if (uuid != NULL) {
		query = "select_media_by_uuid";
		st_db_postgresql_prepare(self, query, "SELECT m.id, m.uuid, label, mediumserialnumber, m.name, m.status, location, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, operationcount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, m.blocksize, freeblock, totalblock, m.type, nbfiles, densitycode, mode, p.uuid FROM media m LEFT JOIN mediaformat mf ON m.mediaformat = mf.id LEFT JOIN pool p ON m.pool = p.id WHERE m.uuid = $1 LIMIT 1");

		const char * param[] = { uuid };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	} else if (medium_serial_number != NULL) {
		query = "select_media_by_medium_serial_number";
		st_db_postgresql_prepare(self, query, "SELECT m.id, m.uuid, label, mediumserialnumber, m.name, m.status, location, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, operationcount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, m.blocksize, freeblock, totalblock, m.type, nbfiles, densitycode, mode, p.uuid FROM media m LEFT JOIN mediaformat mf ON m.mediaformat = mf.id LEFT JOIN pool p ON m.pool = p.id WHERE mediumserialnumber = $1 LIMIT 1");

		const char * param[] = { medium_serial_number };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	} else {
		query = "select_media_by_label";
		st_db_postgresql_prepare(self, query, "SELECT m.id, m.uuid, label, mediumserialnumber, m.name, m.status, location, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, operationcount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, m.blocksize, freeblock, totalblock, m.type, nbfiles, densitycode, mode, p.uuid FROM media m LEFT JOIN mediaformat mf ON m.mediaformat = mf.id LEFT JOIN pool p ON m.pool = p.id WHERE label = $1 LIMIT 1");

		const char * param[] = { label };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	}

	struct st_media * media = NULL;
	ExecStatusType status = PQresultStatus(result);
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		media = malloc(sizeof(struct st_media));
		bzero(media, sizeof(struct st_media));

		struct st_db_postgresql_media_data * media_data = malloc(sizeof(struct st_db_postgresql_media_data));
		media->db_data = media_data;

		st_db_postgresql_get_long(result, 0, 0, &media_data->id);
		st_db_postgresql_get_string(result, 0, 1, media->uuid, 37);
		st_db_postgresql_get_string_dup(result, 0, 2, &media->label);
		st_db_postgresql_get_string_dup(result, 0, 3, &media->medium_serial_number);
		st_db_postgresql_get_string_dup(result, 0, 4, &media->name);

		media->status = st_media_string_to_status(PQgetvalue(result, 0, 5));
		media->location = st_media_string_to_location(PQgetvalue(result, 0, 6));

		media->last_read = media->last_write = 0;
		st_db_postgresql_get_time(result, 0, 7, &media->first_used);
		st_db_postgresql_get_time(result, 0, 8, &media->use_before);
		if (!PQgetisnull(result, 0, 9))
			st_db_postgresql_get_time(result, 0, 9, &media->last_read);
		if (!PQgetisnull(result, 0, 10))
			st_db_postgresql_get_time(result, 0, 10, &media->last_write);

		st_db_postgresql_get_long(result, 0, 11, &media->nb_total_read);
		st_db_postgresql_get_long(result, 0, 12, &media->nb_total_write);

		st_db_postgresql_get_uint(result, 0, 13, &media->nb_read_errors);
		st_db_postgresql_get_uint(result, 0, 14, &media->nb_write_errors);

		st_db_postgresql_get_long(result, 0, 15, &media->load_count);
		st_db_postgresql_get_long(result, 0, 16, &media->read_count);
		st_db_postgresql_get_long(result, 0, 17, &media->write_count);
		st_db_postgresql_get_long(result, 0, 18, &media->operation_count);

		st_db_postgresql_get_ssize(result, 0, 19, &media->block_size);
		st_db_postgresql_get_ssize(result, 0, 20, &media->free_block);
		st_db_postgresql_get_ssize(result, 0, 21, &media->total_block);

		media->type = st_media_string_to_type(PQgetvalue(result, 0, 22));
		st_db_postgresql_get_uint(result, 0, 23, &media->nb_volumes);

		unsigned char density_code;
		st_db_postgresql_get_uchar(result, 0, 24, &density_code);
		enum st_media_format_mode mode = st_media_string_to_format_mode(PQgetvalue(result, 0, 25));
		media->format = st_media_format_get_by_density_code(density_code, mode);

		if (!PQgetisnull(result, 0, 26))
			media->pool = st_pool_get_by_uuid(PQgetvalue(result, 0, 26));
	}

	PQclear(result);
	return media;
}

static int st_db_postgresql_get_media_format(struct st_database_connection * connect, struct st_media_format * media_format, unsigned char density_code, const char * name, enum st_media_format_mode mode) {
	if (connect == NULL || media_format == NULL)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;

	const char * query;
	PGresult * result;

	if (name != NULL) {
		query = "select_media_format_by_name";
		st_db_postgresql_prepare(self, query, "SELECT name, datatype, maxloadcount, maxreadcount, maxwritecount, maxopcount, EXTRACT('epoch' FROM lifespan), capacity, blocksize, densitycode, supportpartition, supportmam FROM mediaformat WHERE name = $1 AND mode = $2 LIMIT 1");

		const char * param[] = { name, st_media_format_mode_to_string(mode) };
		result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	} else {
		query = "select_media_format_by_density_code";
		st_db_postgresql_prepare(self, query, "SELECT name, datatype, maxloadcount, maxreadcount, maxwritecount, maxopcount, EXTRACT('epoch' FROM lifespan), capacity, blocksize, densitycode, supportpartition, supportmam FROM mediaformat WHERE densitycode = $1 AND mode = $2 LIMIT 1");

		char * c_density_code = NULL;
		asprintf(&c_density_code, "%d", density_code);

		const char * param[] = { c_density_code, st_media_format_mode_to_string(mode) };
		result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);

		free(c_density_code);
	}

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

		st_db_postgresql_get_long(result, 0, 6, &media_format->life_span);

		st_db_postgresql_get_ssize(result, 0, 7, &media_format->capacity);
		st_db_postgresql_get_ssize(result, 0, 8, &media_format->block_size);
		st_db_postgresql_get_uchar(result, 0, 9, &media_format->density_code);

		st_db_postgresql_get_bool(result, 0, 10, &media_format->support_partition);
		st_db_postgresql_get_bool(result, 0, 11, &media_format->support_mam);

		PQclear(result);
		return 0;
	}

	PQclear(result);
	return 1;
}

static struct st_pool * st_db_postgresql_get_pool(struct st_database_connection * connect, struct st_archive * archive, struct st_job * job, const char * uuid) {
	if (connect == NULL || (archive == NULL && job == NULL && uuid == NULL))
		return NULL;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query;
	PGresult * result;

	if (archive != NULL) {
		query = "select_pool_by_archive";
		st_db_postgresql_prepare(self, query, "SELECT DISTINCT p.id, p.uuid, p.name, autocheck, growable, unbreakablelevel, rewritable, densitycode, mode, deleted FROM pool p LEFT JOIN mediaformat mf ON p.mediaformat = mf.id LEFT JOIN media m ON m.pool = p.id LEFT JOIN archivevolume av ON av.media = m.id WHERE av.archive = $1");

		struct st_db_postgresql_archive_data * archive_data = archive->db_data;
		char * archiveid;
		asprintf(&archiveid, "%ld", archive_data->id);

		const char * param[] = { archiveid };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);

		free(archiveid);
	} else if (job != NULL) {
		query = "select_pool_by_job";
		st_db_postgresql_prepare(self, query, "SELECT p.id, p.uuid, p.name, autocheck, growable, unbreakablelevel, rewritable, densitycode, mode, deleted FROM job j RIGHT JOIN pool p ON j.pool = p.id LEFT JOIN mediaformat mf ON p.mediaformat = mf.id WHERE j.id = $1 LIMIT 1");

		struct st_db_postgresql_job_data * job_data = job->db_data;
		char * jobid;
		asprintf(&jobid, "%ld", job_data->job_id);

		const char * param[] = { jobid };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);

		free(jobid);
	} else {
		query = "select_pool_by_uuid";
		st_db_postgresql_prepare(self, query, "SELECT p.id, p.uuid, p.name, autocheck, growable, unbreakablelevel, rewritable, densitycode, mode, deleted FROM pool p LEFT JOIN mediaformat mf ON p.mediaformat = mf.id WHERE uuid = $1 LIMIT 1");

		const char * param[] = { uuid };
		result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	}

	ExecStatusType status = PQresultStatus(result);

	struct st_pool * pool = NULL;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		pool = malloc(sizeof(struct st_pool));
		bzero(pool, sizeof(struct st_pool));

		struct st_db_postgresql_pool_data * pool_data = pool->db_data = malloc(sizeof(struct st_db_postgresql_pool_data));
		st_db_postgresql_get_long(result, 0, 0, &pool_data->id);

		st_db_postgresql_get_string(result, 0, 1, pool->uuid, 37);
		st_db_postgresql_get_string_dup(result, 0, 2, &pool->name);
		pool->auto_check = st_pool_autocheck_string_to_mode(PQgetvalue(result, 0, 3));
		st_db_postgresql_get_bool(result, 0, 4, &pool->growable);
		pool->unbreakable_level = st_pool_unbreakable_string_to_level(PQgetvalue(result, 0, 5));
		st_db_postgresql_get_bool(result, 0, 6, &pool->rewritable);
		st_db_postgresql_get_bool(result, 0, 9, &pool->deleted);

		unsigned char density_code;
		st_db_postgresql_get_uchar(result, 0, 7, &density_code);
		enum st_media_format_mode mode = st_media_string_to_format_mode(PQgetvalue(result, 0, 8));

		pool->format = st_media_format_get_by_density_code(density_code, mode);
	}

	PQclear(result);

	return pool;
}

static int st_db_postgresql_sync_media(struct st_database_connection * connect, struct st_media * media) {
	struct st_db_postgresql_connection_private * self = connect->data;

	struct st_db_postgresql_media_data * media_data = media->db_data;
	if (media_data == NULL) {
		media->db_data = media_data = malloc(sizeof(struct st_db_postgresql_media_data));
		media_data->id = -1;
	}

	bool found_id = false;
	char * mediaid = NULL, * mediaformatid = NULL, * poolid = NULL;
	if (media_data->id < 0 && media->medium_serial_number != NULL) {
		const char * query = "select_media_id_by_medium_serial_number";
		st_db_postgresql_prepare(self, query, "SELECT id, uuid, label FROM media WHERE mediumserialnumber = $1 FOR UPDATE NOWAIT");

		const char * param[] = { media->medium_serial_number };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_string_dup(result, 0, 0, &mediaid);
			st_db_postgresql_get_long(result, 0, 0, &media_data->id);

			st_db_postgresql_get_string(result, 0, 1, media->uuid, 37);

			free(media->label);
			media->label = NULL;
			st_db_postgresql_get_string_dup(result, 0, 2, &media->label);

			found_id = true;
		}

		PQclear(result);
	}

	if (media_data->id < 0 && media->uuid[0] != '\0') {
		const char * query = "select_media_id_by_uuid";
		st_db_postgresql_prepare(self, query, "SELECT id, mediumserialnumber, label FROM media WHERE uuid = $1 FOR UPDATE NOWAIT");

		const char * param[] = { media->uuid };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_string_dup(result, 0, 0, &mediaid);
			st_db_postgresql_get_long(result, 0, 0, &media_data->id);

			free(media->medium_serial_number);
			media->medium_serial_number = NULL;
			st_db_postgresql_get_string_dup(result, 0, 1, &media->medium_serial_number);

			free(media->label);
			media->label = NULL;
			st_db_postgresql_get_string_dup(result, 0, 2, &media->label);

			found_id = true;
		}

		PQclear(result);
	}

	if (media_data->id < 0 && media->label != NULL) {
		const char * query = "select_media_id_by_label_for_sync";
		st_db_postgresql_prepare(self, query, "SELECT id, mediumserialnumber, uuid FROM media WHERE label = $1 FOR UPDATE NOWAIT");

		const char * param[] = { media->label };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_string_dup(result, 0, 0, &mediaid);
			st_db_postgresql_get_long(result, 0, 0, &media_data->id);

			free(media->medium_serial_number);
			media->medium_serial_number = NULL;
			st_db_postgresql_get_string_dup(result, 0, 1, &media->medium_serial_number);

			st_db_postgresql_get_string(result, 0, 2, media->uuid, 37);

			found_id = true;
		}

		PQclear(result);
	}

	if (found_id) {
		const char * query = "select_media_by_id";
		st_db_postgresql_prepare(self, query, "SELECT name, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, type, blocksize, mediaformat, pool FROM media WHERE id = $1");

		const char * param[] = { mediaid };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			free(media->name);
			media->name = NULL;
			st_db_postgresql_get_string_dup(result, 0, 0, &media->name);

			st_db_postgresql_get_time(result, 0, 1, &media->first_used);
			st_db_postgresql_get_time(result, 0, 2, &media->use_before);

			if (!PQgetisnull(result, 0, 3))
				st_db_postgresql_get_time_max(result, 0, 3, &media->last_read);
			if (!PQgetisnull(result, 0, 4))
				st_db_postgresql_get_time_max(result, 0, 4, &media->last_write);

			st_db_postgresql_get_long_add(result, 0, 5, &media->load_count);
			st_db_postgresql_get_long_add(result, 0, 6, &media->read_count);
			st_db_postgresql_get_long_add(result, 0, 7, &media->write_count);

			st_db_postgresql_get_long_add(result, 0, 8, &media->nb_total_read);
			st_db_postgresql_get_long_add(result, 0, 9, &media->nb_total_write);

			st_db_postgresql_get_uint_add(result, 0, 10, &media->nb_read_errors);
			st_db_postgresql_get_uint_add(result, 0, 11, &media->nb_write_errors);

			media->type = st_media_string_to_type(PQgetvalue(result, 0, 12));
			st_db_postgresql_get_ssize(result, 0, 13, &media->block_size);

			st_db_postgresql_get_string_dup(result, 0, 14, &mediaformatid);

			if (!PQgetisnull(result, 0, 15))
				st_db_postgresql_get_string_dup(result, 0, 15, &poolid);
		}

		PQclear(result);
	}

	if (media_data->id < 0 && mediaformatid == NULL) {
		const char * query = "select_media_format_by_density";
		st_db_postgresql_prepare(self, query, "SELECT id FROM mediaformat WHERE densitycode = $1 AND mode = $2 FOR SHARE NOWAIT");

		char * densitycode = NULL;
		asprintf(&densitycode, "%hhu", media->format->density_code);

		const char * param[] = { densitycode, st_media_format_mode_to_string(media->format->mode) };
		PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_string_dup(result, 0, 0, &mediaformatid);

		free(densitycode);
		PQclear(result);
	}

	if (poolid == NULL && media->pool != NULL) {
		const char * query = "select_pool_id_by_uuid";
		st_db_postgresql_prepare(self, query, "SELECT id FROM pool WHERE uuid = $1 LIMIT 1");

		const char * param[] = { media->pool->uuid };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_string_dup(result, 0, 0, &poolid);

		PQclear(result);
	} else if (poolid != NULL && media->pool == NULL) {
		free(poolid);
		poolid = NULL;
	}

	if (media_data->id < 0) {
		const char * query = "insert_media";
		st_db_postgresql_prepare(self, query, "INSERT INTO media(uuid, label, mediumserialnumber, name, status, location, firstused, usebefore, lastread, lastwrite, loadcount, readcount, writecount, nbtotalblockread, nbtotalblockwrite, nbreaderror, nbwriteerror, type, nbfiles, blocksize, freeblock, totalblock, mediaformat, pool) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20, $21, $22, $23, $24) RETURNING id");

		char buffer_first_used[32];
		char buffer_use_before[32];
		char buffer_last_read[32] = "";
		char buffer_last_write[32] = "";
		st_util_time_convert(&media->first_used, "%F %T", buffer_first_used, 32);
		st_util_time_convert(&media->use_before, "%F %T", buffer_use_before, 32);
		if (media->last_read > 0)
			st_util_time_convert(&media->last_read, "%F %T", buffer_last_read, 32);
		if (media->last_write > 0)
			st_util_time_convert(&media->last_write, "%F %T", buffer_last_write, 32);

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
			*media->uuid ? media->uuid : NULL, media->label, media->medium_serial_number, media->name, st_media_status_to_string(media->status),
			st_media_location_to_string(media->location), buffer_first_used, buffer_use_before, media->last_read > 0 ? buffer_last_read : NULL,
			media->last_write > 0 ? buffer_last_write : NULL, load, read, write, totalblockread, totalblockwrite, totalreaderror, totalwriteerror,
			st_media_type_to_string(media->type), nbfiles, blocksize, freeblock, totalblock, mediaformatid, poolid
		};
		PGresult * result = PQexecPrepared(self->connect, query, 24, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &media_data->id);

		PQclear(result);
		free(load);
		free(read);
		free(write);
		free(nbfiles);
		free(blocksize);
		free(mediaformatid);
		free(poolid);
		free(totalblockread);
		free(totalblockwrite);
		free(totalreaderror);
		free(totalwriteerror);

		return status == PGRES_FATAL_ERROR;
	} else {
		if (mediaid == NULL)
			asprintf(&mediaid, "%ld", media_data->id);

		if (!media->locked && !media->lock->ops->try_lock(media->lock)) {
			const char * query = "select_media_before_update";
			st_db_postgresql_prepare(self, query, "SELECT m.name, m.label, p.uuid FROM media m LEFT JOIN pool p ON m.pool = p.id WHERE m.id = $1");
			const char * param1[] = { mediaid };

			PGresult * result = PQexecPrepared(self->connect, query, 1, param1, NULL, NULL, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result, query);
			else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
				free(media->name);
				free(media->label);
				media->name = media->label = NULL;

				st_db_postgresql_get_string_dup(result, 0, 0, &media->name);
				st_db_postgresql_get_string_dup(result, 0, 1, &media->label);

				char * uuid = NULL;
				st_db_postgresql_get_string_dup(result, 0, 2, &uuid);

				if (uuid != NULL)
					media->pool = st_pool_get_by_uuid(uuid);
				else if (media->pool != NULL) {
					media->pool = NULL;
					media->status = st_media_status_new;
				}

				free(uuid);
			}

			PQclear(result);

			media->lock->ops->unlock(media->lock);
		}

		const char * query = "update_media";
		st_db_postgresql_prepare(self, query, "UPDATE media SET uuid = $1, name = $2, status = $3, location = $4, lastread = $5, lastwrite = $6, loadcount = $7, readcount = $8, writecount = $9, nbtotalblockread = $10, nbtotalblockwrite = $11, nbreaderror = $12, nbwriteerror = $13, nbfiles = $14, blocksize = $15, freeblock = $16, totalblock = $17, pool = $18, locked = $19, type = $20 WHERE id = $21");

		char buffer_last_read[32] = "";
		char buffer_last_write[32] = "";
		if (media->last_read > 0)
			st_util_time_convert(&media->last_read, "%F %T", buffer_last_read, 32);
		if (media->last_write > 0)
			st_util_time_convert(&media->last_write, "%F %T", buffer_last_write, 32);

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
			*media->uuid ? media->uuid : NULL, media->name, st_media_status_to_string(media->status), st_media_location_to_string(media->location),
			media->last_read > 0 ? buffer_last_read : NULL, media->last_write > 0 ? buffer_last_write : NULL, load, read, write, 
			totalblockread, totalblockwrite, totalreaderror, totalwriteerror, nbfiles, blocksize, freeblock, totalblock,
			poolid, media->locked ? "true" : "false", st_media_type_to_string(media->type), mediaid
		};
		PGresult * result = PQexecPrepared(self->connect, query, 21, param2, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);

		PQclear(result);
		free(load);
		free(read);
		free(write);
		free(nbfiles);
		free(blocksize);
		free(freeblock);
		free(totalblock);
		free(mediaid);
		free(mediaformatid);
		free(poolid);
		free(totalblockread);
		free(totalblockwrite);
		free(totalreaderror);
		free(totalwriteerror);

		return status == PGRES_FATAL_ERROR;
	}

	return 0;
}

static int st_db_postgresql_sync_media_format(struct st_database_connection * connect, struct st_media_format * format) {
	if (connect == NULL || format == NULL)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query = "select_media_format_by_name";
	st_db_postgresql_prepare(self, query, "SELECT name, datatype, maxloadcount, maxreadcount, maxwritecount, maxopcount, EXTRACT('epoch' FROM lifespan), capacity, blocksize, densitycode, supportpartition, supportmam FROM mediaformat WHERE name = $1 AND mode = $2 LIMIT 1");

	const char * param[] = { format->name, st_media_format_mode_to_string(format->mode) };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_ssize(result, 0, 7, &format->capacity);

	PQclear(result);

	return status != PGRES_TUPLES_OK;
}

static int st_db_postgresql_sync_slot(struct st_database_connection * connect, struct st_slot * slot) {
	struct st_db_postgresql_connection_private * self = connect->data;

	if (slot->media != NULL && st_db_postgresql_sync_media(connect, slot->media))
		return 1;

	struct st_db_postgresql_changer_data * changer_data = slot->changer->db_data;

	struct st_db_postgresql_slot_data * slot_data = slot->db_data;
	if (slot_data == NULL) {
		slot->db_data = slot_data = malloc(sizeof(struct st_db_postgresql_slot_data));
		slot_data->id = -1;
	}

	struct st_db_postgresql_media_data * media_data = NULL;
	if (slot->media != NULL)
		media_data = slot->media->db_data;

	char * changer_id = NULL, * media_id = NULL, * slot_id = NULL;
	asprintf(&changer_id, "%ld", changer_data->id);
	if (slot_data->id >= 0)
		asprintf(&slot_id, "%ld", slot_data->id);

	if (slot_data->id >= 0) {
		const char * query = "select_slot_by_id";
		st_db_postgresql_prepare(self, query, "SELECT media FROM changerslot WHERE id = $1 FOR UPDATE NOWAIT");

		const char * param[] = { slot_id };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1 && !PQgetisnull(result, 0, 0))
			st_db_postgresql_get_string_dup(result, 0, 0, &media_id);

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			free(changer_id);
			free(slot_id);
			return 2;
		}
	} else {
		const char * query = "select_slot_by_index_changer";
		st_db_postgresql_prepare(self, query, "SELECT id, media FROM changerslot WHERE index = $1 AND changer = $2 FOR UPDATE NOWAIT");

		char * slot_index = NULL;
		asprintf(&slot_index, "%td", slot - slot->changer->slots);

		const char * param[] = { slot_index, changer_id };
		PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
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

		if (status == PGRES_FATAL_ERROR) {
			free(changer_id);
			return 2;
		}
	}

	long mediaid = -1;
	if (media_id) {
		sscanf(media_id, "%ld", &mediaid);

		if (media_data == NULL || mediaid != media_data->id) {
			const char * query = "update_media_offline";
			st_db_postgresql_prepare(self, query, "UPDATE media SET location = 'offline' WHERE id = $1");

			const char * param[] = { media_id };
			PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result, query);

			PQclear(result);

			free(media_id);
			media_id = NULL;
		}
	}

	if (media_id == NULL && media_data != NULL && media_data->id > -1)
		asprintf(&media_id, "%ld", media_data->id);

	if (slot_data->id >= 0) {
		const char * query = "update_slot";
		st_db_postgresql_prepare(self, query, "UPDATE changerslot SET media = $1 WHERE id = $2");

		const char * param[] = { media_id, slot_id };
		PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);

		PQclear(result);
		free(changer_id);
		free(slot_id);
		free(media_id);

		return status == PGRES_FATAL_ERROR;
	} else {
		struct st_db_postgresql_drive_data * drive_data = NULL;
		if (slot->drive != NULL)
			drive_data = slot->drive->db_data;

		char * drive_id = NULL;
		if (drive_data != NULL && drive_data->id >= 0)
			asprintf(&drive_id, "%ld", drive_data->id);

		const char * query = "insert_slot";
		st_db_postgresql_prepare(self, query, "INSERT INTO changerslot(index, changer, drive, media, type) VALUES ($1, $2, $3, $4, $5) RETURNING id");

		char * slot_index = NULL;
		asprintf(&slot_index, "%td", slot - slot->changer->slots);
		char * type;
		switch (slot->type) {
			case st_slot_type_drive:
				type = "drive";
				break;

			case st_slot_type_import_export:
				type = "import / export";
				break;

			default:
				type = "storage";
				break;
		}

		const char * param[] = { slot_index, changer_id, drive_id, media_id, type };
		PGresult * result = PQexecPrepared(self->connect, query, 5, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &slot_data->id);

		PQclear(result);
		free(changer_id);
		free(drive_id);
		free(media_id);
		free(slot_index);

		return status == PGRES_FATAL_ERROR;
	}
}


static int st_db_postgresql_add_check_archive_job(struct st_database_connection * connect, struct st_job * job, struct st_archive * archive, time_t starttime, bool quick_mode) {
	if (connect == NULL || job == NULL || archive == NULL)
		return 1;

	struct st_db_postgresql_job_data * job_data = job->db_data;
	if (job_data == NULL || job_data->job_id < 1)
		return 2;

	struct st_db_postgresql_archive_data * archive_data = archive->db_data;
	if (archive_data == NULL || archive_data->id < 1)
		return 2;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query = "select_job_type";
	st_db_postgresql_prepare(self, query, "SELECT id FROM jobtype WHERE name = 'check-archive' LIMIT 1");
	PGresult * result = PQexecPrepared(self->connect, query, 0, NULL, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	char * jobtypeid;

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_string_dup(result, 0, 0, &jobtypeid);

	PQclear(result);

	if (status == PGRES_FATAL_ERROR)
		return 3;

	query = "select_host_and_login_from_job";
	st_db_postgresql_prepare(self, query, "SELECT host, login FROM job WHERE id = $1");

	char * jobid;
	asprintf(&jobid, "%ld", job_data->job_id);

	const char * param1[] = { jobid };
	result = PQexecPrepared(self->connect, query, 1, param1, 0, 0, 0);
	status = PQresultStatus(result);

	char * hostid, * loginid;

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_string_dup(result, 0, 0, &hostid);
		st_db_postgresql_get_string_dup(result, 0, 1, &loginid);
	}

	PQclear(result);

	if (status == PGRES_FATAL_ERROR) {
		free(jobtypeid);
		return 3;
	}

	query = "insert_check_archive_job";
	st_db_postgresql_prepare(self, query, "INSERT INTO job(name, type, nextstart, archive, host, login, options) VALUES ($1, $2, $3, $4, $5, $6, hstore('quick_mode', $7))");

	char * name, * archiveid;
	asprintf(&name, "check %s", archive->name);
	asprintf(&archiveid, "%ld", archive_data->id);

	char sstarttime[20];
	st_util_time_convert(&starttime, "%F %T", sstarttime, 20);

	const char * param2[] = {
		name, jobtypeid, sstarttime, archiveid, hostid, loginid,
		quick_mode ? "true" : "false"
	};
	result = PQexecPrepared(self->connect, query, 7, param2, 0, 0, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	PQclear(result);
	free(name);
	free(jobtypeid);
	free(archiveid);
	free(hostid);
	free(loginid);

	return status == PGRES_FATAL_ERROR;
}

static int st_db_postgresql_add_job_record(struct st_database_connection * connect, struct st_job * job, const char * message, enum st_job_record_notif notif) {
	if (connect == NULL || job == NULL || message == NULL)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query = "insert_job_record";
	st_db_postgresql_prepare(self, query, "INSERT INTO jobrecord(jobrun, status, message, notif) VALUES ($1, $2, $3, $4)");

	struct st_db_postgresql_job_data * job_data = job->db_data;

	char * jobrunid, * numrun;
	asprintf(&jobrunid, "%ld", job_data->jobrun_id);
	asprintf(&numrun, "%ld", job->num_runs);

	const char * param[] = { jobrunid, st_job_status_to_string(job->sched_status), message, st_db_postgresql_job_record_notif_to_string(notif) };
	PGresult * result = PQexecPrepared(self->connect, query, 4, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	PQclear(result);
	free(numrun);
	free(jobrunid);

	return status != PGRES_COMMAND_OK;
}

static int st_db_postgresql_finish_job_run(struct st_database_connection * connect, struct st_job * job, time_t endtime, int exitcode) {
	if (connect == NULL || job == NULL)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query = "update_job_run";
	st_db_postgresql_prepare(self, query, "UPDATE jobrun SET endtime = $1, status = $2, done = $3, exitcode = $4, stoppedbyuser = $5 WHERE id = $6");

	struct st_db_postgresql_job_data * job_data = job->db_data;

	char * jobrunid, str_endtime[24], * done, * str_exitcode, * stoppedbyuser;
	asprintf(&jobrunid, "%ld", job_data->jobrun_id);
	st_util_time_convert(&endtime, "%F %T", str_endtime, 24);
	asprintf(&done, "%f", job->done);
	asprintf(&str_exitcode, "%d", exitcode);
	stoppedbyuser = job->stoped_by_user ? "TRUE" : "FALSE";

	const char * param[] = { str_endtime, st_job_status_to_string(job->sched_status), done, str_exitcode, stoppedbyuser, jobrunid };
	PGresult * result = PQexecPrepared(self->connect, query, 6, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	job_data->jobrun_id = -1;

	PQclear(result);
	free(jobrunid);
	free(done);
	free(str_exitcode);

	return status != PGRES_COMMAND_OK;
}

static struct st_job_selected_path * st_db_postgresql_get_selected_paths(struct st_database_connection * connect, struct st_job * job, unsigned int * nb_paths) {
	if (connect == NULL || job == NULL || nb_paths == NULL)
		return NULL;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query = "select_selected_paths_by_job";
	st_db_postgresql_prepare(self, query, "SELECT id, path FROM selectedfile WHERE id IN (SELECT selectedfile FROM jobtoselectedfile WHERE job = $1) ORDER BY id");

	struct st_db_postgresql_job_data * job_data = job->db_data;
	char * jobid;
	asprintf(&jobid, "%ld", job_data->job_id);

	const char * param[] = { jobid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	struct st_job_selected_path * paths = NULL;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else {
		*nb_paths = PQntuples(result);
		paths = calloc(*nb_paths, sizeof(struct st_job_selected_path));

		unsigned int i;
		for (i = 0; i < *nb_paths; i++) {
			struct st_job_selected_path * path = paths + i;
			st_db_postgresql_get_string_dup(result, i, 1, &path->path);

			struct st_db_postgresql_selected_path_data * data = path->db_data = malloc(sizeof(struct st_db_postgresql_selected_path_data));
			st_db_postgresql_get_long(result, i, 0, &data->id);
		}
	}

	PQclear(result);
	free(jobid);

	return paths;
}

static int st_db_postgresql_start_job_run(struct st_database_connection * connect, struct st_job * job, time_t starttime) {
	if (connect == NULL || job == NULL)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query = "insert_job_run";
	st_db_postgresql_prepare(self, query, "INSERT INTO jobrun(job, numrun, starttime) VALUES ($1, $2, $3) RETURNING id");

	struct st_db_postgresql_job_data * job_data = job->db_data;

	char * jobid, * numrun, sstarttime[24];
	asprintf(&jobid, "%ld", job_data->job_id);
	asprintf(&numrun, "%ld", job->num_runs);
	st_util_time_convert(&starttime, "%F %T", sstarttime, 24);

	const char * param[] = { jobid, numrun, sstarttime };
	PGresult * result = PQexecPrepared(self->connect, query, 3, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_long(result, 0, 0, &job_data->jobrun_id);

	PQclear(result);
	free(jobid);
	free(numrun);

	return status != PGRES_COMMAND_OK;
}

static int st_db_postgresql_sync_job(struct st_database_connection * connect, struct st_job *** jobs, unsigned int * nb_jobs) {
	if (connect == NULL || jobs == NULL || nb_jobs == NULL)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;

	long max_id = 0;
	unsigned int i;
	for (i = 0; i < *nb_jobs; i++) {
		const char * query = "select_job_by_id";
		st_db_postgresql_prepare(self, query, "SELECT status FROM job WHERE id = $1 FOR UPDATE LIMIT 1");

		struct st_job * job = (*jobs)[i];
		struct st_db_postgresql_job_data * job_data = job->db_data;
		char * job_id;
		asprintf(&job_id, "%ld", job_data->job_id);

		if (max_id < job_data->job_id)
			max_id = job_data->job_id;

		const char * param1[] = { job_id };
		PGresult * result = PQexecPrepared(self->connect, query, 1, param1, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);
		int nb_result = PQntuples(result);

		int found = 0;
		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && nb_result == 1) {
			job->db_status = st_job_string_to_status(PQgetvalue(result, 0, 0));
			found = 1;
		} else if (status == PGRES_TUPLES_OK && nb_result == 0)
			job->db_status = st_job_status_stopped;

		PQclear(result);

		if (!found || job->db_status == st_job_status_stopped) {
			free(job_id);
			continue;
		}

		query = "update_job";
		st_db_postgresql_prepare(self, query, "UPDATE job SET nextstart = $1, repetition = $2, status = $3, update = NOW() WHERE id = $4");

		char next_start[24];
		st_util_time_convert(&job->next_start, "%F %T", next_start, 24);

		char * repetition;
		asprintf(&repetition, "%ld", job->repetition);

		const char * param2[] = { next_start, repetition, st_job_status_to_string(job->sched_status), job_id };
		result = PQexecPrepared(self->connect, query, 4, param2, NULL, NULL, 0);
		status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);

		free(job_id);
		free(repetition);
		PQclear(result);

		if (job_data->jobrun_id > 0) {
			query = "update_jobrun";
			st_db_postgresql_prepare(self, query, "UPDATE jobrun SET status = $1, done = $2 WHERE id = $3");

			char * done, * id;
			asprintf(&done, "%f", job->done);
			asprintf(&id, "%ld", job_data->jobrun_id);

			const char * param3[] = { st_job_status_to_string(job->sched_status), done, id };
			result = PQexecPrepared(self->connect, query, 3, param3, NULL, NULL, 0);

			status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result, query);

			free(done);
			free(id);
			PQclear(result);
		}
	}

	const char * query0 = "select_new_jobs";
	st_db_postgresql_prepare(self, query0, "SELECT j.id, j.name, nextstart, EXTRACT('epoch' FROM interval), repetition, status, u.login, metadata, options, jt.name FROM job j LEFT JOIN jobtype jt ON j.type = jt.id LEFT JOIN users u ON j.login = u.id WHERE host = $1 AND j.id > $2 ORDER BY j.id");
	const char * query1 = "select_job_option";
	st_db_postgresql_prepare(self, query1, "SELECT * FROM each((SELECT options FROM job WHERE id = $1 LIMIT 1))");
	const char * query2 = "select_num_runs";
	st_db_postgresql_prepare(self, query2, "SELECT MAX(numrun) AS max FROM jobrun WHERE job = $1");

	char * hostid = st_db_postgresql_get_host(connect);
	char * job_id;
	asprintf(&job_id, "%ld", max_id);

	const char * param[] = { hostid, job_id };
	PGresult * result = PQexecPrepared(self->connect, query0, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	unsigned int nb_result = PQntuples(result);

	free(job_id);
	free(hostid);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query0);
	else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		void * new_addr = realloc(*jobs, (*nb_jobs + nb_result + 1) * (sizeof(struct st_job *)));

		if (new_addr == NULL) {
			st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Not enough memory to get new jobs");

			PQclear(result);

			return 1;
		}

		*jobs = new_addr;

		for (i = 0; i < nb_result; i++) {
			struct st_job * job = (*jobs)[*nb_jobs + i] = malloc(sizeof(struct st_job));
			bzero(job, sizeof(struct st_job));
			struct st_db_postgresql_job_data * job_data = malloc(sizeof(struct st_db_postgresql_job_data));
			bzero(job_data, sizeof(*job_data));
			char * job_id;

			job->db_config = connect->config;
			job->db_data = job_data;

			st_db_postgresql_get_long(result, i, 0, &job_data->job_id);
			st_db_postgresql_get_string_dup(result, i, 0, &job_id);
			st_db_postgresql_get_string_dup(result, i, 1, &job->name);
			st_db_postgresql_get_time(result, i, 2, &job->next_start);
			if (!PQgetisnull(result, i, 3))
				st_db_postgresql_get_long(result, i, 3, &job->interval);
			st_db_postgresql_get_long(result, i, 4, &job->repetition);

			job->sched_status = job->db_status = st_job_string_to_status(PQgetvalue(result, i, 5));
			job->stoped_by_user = false;
			job->updated = time(NULL);

			// login
			job->user = st_user_get(PQgetvalue(result, i, 6));

			// meta
			st_db_postgresql_get_string_dup(result, i, 7, &job->meta);

			// options
			job->option = st_hashtable_new2(st_util_string_compute_hash, st_util_basic_free);
			const char * param1[] = { job_id };
			PGresult * result1 = PQexecPrepared(self->connect, query1, 1, param1, NULL, NULL, 0);
			ExecStatusType status1 = PQresultStatus(result);

			if (status1 == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result1, query1);
			else if (PQresultStatus(result1) == PGRES_TUPLES_OK) {
				unsigned int j, nb_metadatas = PQntuples(result1);
				for (j = 0; j < nb_metadatas; j++)
					st_hashtable_put(job->option, strdup(PQgetvalue(result1, j, 0)), st_hashtable_val_string(strdup(PQgetvalue(result1, j, 1))));
			}

			PQclear(result1);

			// num_runs
			result1 = PQexecPrepared(self->connect, query2, 1, param1, NULL, NULL, 0);
			status1 = PQresultStatus(result1);

			if (status1 == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result1, query2);
			else if (status == PGRES_TUPLES_OK && PQntuples(result1) == 1 && !PQgetisnull(result1, 0, 0))
				st_db_postgresql_get_long(result1, 0, 0, &job->num_runs);
			PQclear(result1);

			// driver
			job->driver = st_job_get_driver(PQgetvalue(result, i, 9));

			free(job_id);
		}

		(*jobs)[*nb_jobs + i] = NULL;
		*nb_jobs += nb_result;
	}

	PQclear(result);

	return 0;
}


static int st_db_postgresql_get_user(struct st_database_connection * connect, struct st_user * user, const char * login) {
	if (connect == NULL || user == NULL || login == NULL)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query = "select_user_by_id_or_login";
	st_db_postgresql_prepare(self, query, "SELECT u.id, login, password, salt, fullname, email, homedirectory, isadmin, canarchive, canrestore, disabled, p.uuid FROM users u LEFT JOIN pool p ON u.pool = p.id WHERE login = $1 LIMIT 1");

	const char * param[] = { login };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_result == 1) {
		struct st_db_postgresql_user_data * user_data = user->db_data;
		if (!user_data)
			user->db_data = user_data = malloc(sizeof(struct st_db_postgresql_user_data));

		st_db_postgresql_get_long(result, 0, 0, &user_data->id);
		st_db_postgresql_get_string_dup(result, 0, 1, &user->login);
		st_db_postgresql_get_string_dup(result, 0, 2, &user->password);
		st_db_postgresql_get_string_dup(result, 0, 3, &user->salt);
		st_db_postgresql_get_string_dup(result, 0, 4, &user->fullname);
		st_db_postgresql_get_string_dup(result, 0, 5, &user->email);
		st_db_postgresql_get_string_dup(result, 0, 6, &user->home_directory);

		st_db_postgresql_get_bool(result, 0, 7, &user->is_admin);
		st_db_postgresql_get_bool(result, 0, 8, &user->can_archive);
		st_db_postgresql_get_bool(result, 0, 9, &user->can_restore);
		st_db_postgresql_get_bool(result, 0, 10, &user->disabled);

		user->pool = st_pool_get_by_uuid(PQgetvalue(result, 0, 11));
	}

	PQclear(result);

	return status != PGRES_TUPLES_OK || nb_result < 1;
}

static int st_db_postgresql_sync_user(struct st_database_connection * connect, struct st_user * user) {
	if (connect == NULL || user == NULL)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;
	const char * query = "select_user_by_id";
	st_db_postgresql_prepare(self, query, "SELECT login, password, salt, fullname, email, homedirectory, isadmin, canarchive, canrestore, disabled, uuid FROM users u LEFT JOIN pool p ON u.pool = p.id WHERE u.id = $1 LIMIT 1");

	struct st_db_postgresql_user_data * user_data = user->db_data;

	char * userid = NULL;
	asprintf(&userid, "%ld", user_data->id);

	const char * param[] = { userid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		free(user->login);
		user->login = NULL;
		st_db_postgresql_get_string_dup(result, 0, 0, &user->login);

		free(user->password);
		user->password = NULL;
		st_db_postgresql_get_string_dup(result, 0, 1, &user->password);

		free(user->salt);
		user->salt = NULL;
		st_db_postgresql_get_string_dup(result, 0, 2, &user->salt);

		free(user->fullname);
		user->fullname = NULL;
		st_db_postgresql_get_string_dup(result, 0, 3, &user->fullname);

		free(user->email);
		user->email = NULL;
		st_db_postgresql_get_string_dup(result, 0, 4, &user->email);

		free(user->home_directory);
		user->home_directory = NULL;
		st_db_postgresql_get_string_dup(result, 0, 5, &user->home_directory);

		st_db_postgresql_get_bool(result, 0, 6, &user->is_admin);
		st_db_postgresql_get_bool(result, 0, 7, &user->can_archive);
		st_db_postgresql_get_bool(result, 0, 8, &user->can_restore);

		st_db_postgresql_get_bool(result, 0, 9, &user->disabled);

		user->pool = st_pool_get_by_uuid(PQgetvalue(result, 0, 10));
	}

	PQclear(result);
	free(userid);

	return status == PGRES_TUPLES_OK;
}


static bool st_db_postgresql_add_report(struct st_database_connection * connect, struct st_job * job, struct st_archive * archive, const char * report) {
	if (connect == NULL || job == NULL || archive == NULL || report == NULL)
		return false;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_archive_data * archive_data = archive->db_data;
	struct st_db_postgresql_job_data * job_data = job->db_data;

	if (archive_data == NULL || job_data == NULL)
		return false;

	const char * query = "insert_report";
	st_db_postgresql_prepare(self, query, "INSERT INTO report(archive, jobrun, data) VALUES ($1, $2, $3)");

	char * jobrunid, * archiveid;
	asprintf(&jobrunid, "%ld", job_data->jobrun_id);
	asprintf(&archiveid, "%ld", archive_data->id);

	const char * param[] = { archiveid, jobrunid, report };
	PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	PQclear(result);
	free(jobrunid);
	free(archiveid);

	return status == PGRES_COMMAND_OK;
}

static bool st_db_postgresql_check_checksums_of_archive_volume(struct st_database_connection * connect, struct st_archive_volume * volume) {
	if (connect == NULL || volume == NULL || volume->digests == NULL)
		return false;

	if (volume->digests->nb_elements == 0)
		return true;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_archive_volume_data * volume_data = volume->db_data;
	if (volume_data == NULL || volume_data->id < 0)
		return false;

	const char * query = "select_compare_checksum_of_archive_volume";
	st_db_postgresql_prepare(self, query, "SELECT COUNT(*) > 0 FROM archivevolumetochecksumresult avcr LEFT JOIN checksumresult cr ON avcr.checksumresult = cr.id LEFT JOIN checksum c ON cr.checksum = c.id WHERE avcr.archivevolume = $1 AND cr.result = $2 AND c.name = $3");

	char * volumeid;
	asprintf(&volumeid, "%ld", volume_data->id);

	bool ok = true;
	unsigned int i, nb_digests;
	const void ** checksums = st_hashtable_keys(volume->digests, &nb_digests);
	for (i = 0; i < nb_digests && ok; i++) {
		struct st_hashtable_value digest = st_hashtable_get(volume->digests, checksums[i]);

		const char * param[] = { volumeid, digest.value.string, checksums[i] };
		PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_bool(result, 0, 0, &ok);

		PQclear(result);
	}
	free(checksums);
	free(volumeid);

	return ok;
}

static bool st_db_postgresql_check_checksums_of_file(struct st_database_connection * connect, struct st_archive_file * file) {
	if (connect == NULL || file == NULL || file->digests == NULL)
		return false;

	if (file->digests->nb_elements == 0)
		return true;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_archive_file_data * file_data = file->db_data;
	if (file_data == NULL || file_data->id < 0)
		return false;

	const char * query = "select_compare_checksum_of_file";
	st_db_postgresql_prepare(self, query, "SELECT COUNT(*) > 0 FROM archivefiletochecksumresult afcr LEFT JOIN checksumresult cr ON afcr.checksumresult = cr.id LEFT JOIN checksum c ON cr.checksum = c.id WHERE afcr.archivefile = $1 AND cr.result = $2 AND c.name = $3");

	char * fileid;
	asprintf(&fileid, "%ld", file_data->id);

	bool ok = true;
	unsigned int i, nb_digests;
	const void ** checksums = st_hashtable_keys(file->digests, &nb_digests);
	for (i = 0; i < nb_digests && ok; i++) {
		struct st_hashtable_value digest = st_hashtable_get(file->digests, checksums[i]);

		const char * param[] = { fileid, digest.value.string, checksums[i] };
		PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_bool(result, 0, 0, &ok);

		PQclear(result);
	}
	free(checksums);
	free(fileid);

	return ok;
}

static int st_db_postgresql_add_checksum_result(struct st_database_connection * connect, const char * checksum, char * checksum_result, char ** checksum_result_id) {
	struct st_db_postgresql_connection_private * self = connect->data;

	const char * query0 = "select checksum_by_name";
	st_db_postgresql_prepare(self, query0, "SELECT id FROM checksum WHERE name = $1 LIMIT 1");

	const char * param0[] = { checksum };
	PGresult * result = PQexecPrepared(self->connect, query0, 1, param0, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	char * checksumid;

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result, query0);
		PQclear(result);
		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_string_dup(result, 0, 0, &checksumid);
	} else {
		PQclear(result);
		return 2;
	}
	PQclear(result);

	const char * query1 = "select_checksumresult";
	st_db_postgresql_prepare(self, query1, "SELECT id FROM checksumresult WHERE checksum = $1 AND result = $2 LIMIT 1");

	const char * param1[] = { checksumid, checksum_result };
	result = PQexecPrepared(self->connect, query1, 2, param1, NULL, NULL, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result, query1);
		PQclear(result);
		free(checksumid);
		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_string_dup(result, 0, 0, checksum_result_id);
		PQclear(result);
		free(checksumid);
		return 0;
	}
	PQclear(result);

	const char * query2 = "insert_checksumresult";
	st_db_postgresql_prepare(self, query2, "INSERT INTO checksumresult(checksum, result) VALUES ($1, $2) RETURNING id");

	result = PQexecPrepared(self->connect, query2, 2, param1, NULL, NULL, 0);
	status = PQresultStatus(result);

	free(checksumid);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result, query2);
		PQclear(result);
		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_string_dup(result, 0, 0, checksum_result_id);
		PQclear(result);
		return 0;
	} else {
		PQclear(result);
		return 2;
	}
}

static struct st_archive * st_db_postgresql_get_archive_by_job(struct st_database_connection * connect, struct st_job * job) {
	if (connect == NULL || job == NULL)
		return NULL;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_job_data * job_data = job->db_data;
	if (job_data == NULL || job_data->job_id < 0)
		return NULL;

	const char * query = "select_archive_by_job";
	st_db_postgresql_prepare(self, query, "SELECT a.id, a.uuid, name, u.login FROM archive a LEFT JOIN users u ON a.owner = u.id WHERE a.id IN (SELECT archive FROM job WHERE id = $1 LIMIT 1)");

	char * jobid;
	asprintf(&jobid, "%ld", job_data->job_id);

	const char * param[] = { jobid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	struct st_archive * archive = NULL;

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		archive = malloc(sizeof(struct st_archive));

		st_db_postgresql_get_string(result, 0, 1, archive->uuid, 36);
		st_db_postgresql_get_string_dup(result, 0, 2, &archive->name);

		archive->volumes = NULL;
		archive->nb_volumes = 0;

		archive->user = st_user_get(PQgetvalue(result, 0, 3));

		archive->copy_of = NULL;

		struct st_db_postgresql_archive_data * archive_data = archive->db_data = malloc(sizeof(struct st_db_postgresql_archive_data));
		st_db_postgresql_get_long(result, 0, 0, &archive_data->id);
	}

	PQclear(result);
	free(jobid);

	return archive;
}

static struct st_archive_file * st_db_postgresql_get_archive_file_for_restore_directory(struct st_database_connection * connect, struct st_job * job, unsigned int * nb_files) {
	if (connect == NULL || job == NULL || nb_files == NULL)
		return NULL;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_job_data * job_data = job->db_data;
	if (self == NULL || job_data == NULL || job_data->job_id < 0)
		return NULL;

	const char * query = "select_archive_file_for_restore_directory";
	st_db_postgresql_prepare(self, query, "SELECT af.id, CASE WHEN rt.path IS NOT NULL THEN rt.path || SUBSTRING(af.name FROM CHAR_LENGTH(SUBSTRING(sf.path FROM '(.+/)[^/]+'))) ELSE af.name END AS name, af.type, af.mimetype, af.ownerid, af.owner, af.groupid, af.groups, af.perm, af.ctime, af.mtime, afv.checksumok, afv.checktime, af.size FROM job j LEFT JOIN archivevolume av ON j.archive = av.archive LEFT JOIN archivefiletoarchivevolume afv ON av.id = afv.archivevolume LEFT JOIN archivefile af ON af.id = afv.archivefile LEFT JOIN selectedfile sf ON af.parent = sf.id LEFT JOIN restoreto rt ON rt.job = j.id WHERE j.id = $1 AND af.type = 'directory'");

	char * jobid;
	asprintf(&jobid, "%ld", job_data->job_id);

	const char * param[] = { jobid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	*nb_files = PQntuples(result);

	struct st_archive_file * files = NULL;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && *nb_files > 0) {
		files = calloc(*nb_files, sizeof(struct st_archive_file));

		unsigned int i;
		for (i = 0; i < *nb_files; i++) {
			struct st_archive_file * file = files + i;
			st_db_postgresql_get_string_dup(result, i, 1, &file->name);
			file->type = st_archive_file_string_to_type(PQgetvalue(result, i, 2));
			st_db_postgresql_get_string_dup(result, i, 3, &file->mime_type);

			st_db_postgresql_get_uint(result, i, 4, &file->ownerid);
			st_db_postgresql_get_string(result, i, 5, file->owner, 32);
			st_db_postgresql_get_uint(result, i, 6, &file->groupid);
			st_db_postgresql_get_string(result, i, 7, file->group, 32);
			st_db_postgresql_get_uint(result, i, 8, &file->perm);
			st_db_postgresql_get_time(result, i, 9, &file->create_time);
			st_db_postgresql_get_time(result, i, 10, &file->modify_time);
			st_db_postgresql_get_bool(result, i, 11, &file->check_ok);
			if (PQgetisnull(result, i, 12))
				st_db_postgresql_get_time(result, i, 12, &file->check_time);
			st_db_postgresql_get_ssize(result, i, 13, &file->size);

			file->owner[0] = file->group[0] = '\0';
			file->modify_time = file->create_time;

			file->digests = NULL;

			file->archive = NULL;
			file->selected_path = NULL;

			struct st_db_postgresql_archive_file_data * file_data = file->db_data = malloc(sizeof(struct st_db_postgresql_archive_file_data));
			st_db_postgresql_get_long(result, i, 0, &file_data->id);
		}
	}

	PQclear(result);
	free(jobid);

	return files;
}

static int st_db_postgresql_get_archive_files_by_job_and_archive_volume(struct st_database_connection * connect, struct st_job * job, struct st_archive_volume * volume) {
	if (connect == NULL || job == NULL || volume == NULL)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_job_data * job_data = job->db_data;
	struct st_db_postgresql_archive_volume_data * archive_volume_data = volume->db_data;

	if (self == NULL || job_data == NULL || job_data->job_id < 0 || archive_volume_data == NULL || archive_volume_data->id < 0)
		return 2;

	const char * query;
	if (st_db_postgresql_has_selected_files_by_job(connect, job)) {
		query = "select_archive_files_by_job_and_archive_volume_with_selected_files";
		st_db_postgresql_prepare(self, query, "SELECT af.id, af.name, af.type, mimetype, ownerid, owner, groupid, groups, perm, af.ctime, af.mtime, afv.archivetime, afv.checksumok, afv.checktime, af.size, afv.blocknumber FROM job j LEFT JOIN archivevolume av ON j.archive = av.archive LEFT JOIN archivefiletoarchivevolume afv ON av.id = afv.archivevolume LEFT JOIN archivefile af ON afv.archivefile = af.id, (SELECT path, CHAR_LENGTH(path) AS length FROM selectedfile ssf2 LEFT JOIN jobtoselectedfile sjsf ON ssf2.id = sjsf.selectedfile WHERE sjsf.job = $1) AS ssf WHERE j.id = $1 AND av.id = $2 AND SUBSTR(af.name, 0, ssf.length + 1) = ssf.path ORDER BY af.id");
	} else {
		query = "select_archive_files_by_job_and_archive_volume";
		st_db_postgresql_prepare(self, query, "SELECT af.id, af.name, af.type, mimetype, ownerid, owner, groupid, groups, perm, af.ctime, af.mtime, afv.archivetime, afv.checksumok, afv.checktime, af.size, afv.blocknumber FROM job j LEFT JOIN archivevolume av ON j.archive = av.archive LEFT JOIN archivefiletoarchivevolume afv ON av.id = afv.archivevolume LEFT JOIN archivefile af ON afv.archivefile = af.id WHERE j.id = $1 AND av.id = $2 ORDER BY af.id");
	}

	const char * query2 = "select_checksumresult_of_file";
	st_db_postgresql_prepare(self, query2, "SELECT c.name, cr.result FROM archivefiletochecksumresult afcr LEFT JOIN checksumresult cr ON afcr.checksumresult = cr.id LEFT JOIN checksum c ON cr.checksum = c.id WHERE afcr.archivefile = $1");

	char * jobid, * archivevolumeid;
	asprintf(&jobid, "%ld", job_data->job_id);
	asprintf(&archivevolumeid, "%ld", archive_volume_data->id);

	const char * param[] = { jobid, archivevolumeid };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_tuples = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_tuples > 0) {
		volume->files = calloc(nb_tuples, sizeof(struct st_archive_files));
		volume->nb_files = nb_tuples;

		unsigned int i;
		for (i = 0; i < volume->nb_files; i++) {
			struct st_archive_file * file = malloc(sizeof(struct st_archive_file));
			bzero(file, sizeof(struct st_archive_file));

			st_db_postgresql_get_string_dup(result, i, 1, &file->name);
			file->type = st_archive_file_string_to_type(PQgetvalue(result, i, 2));
			st_db_postgresql_get_string_dup(result, i, 3, &file->mime_type);

			st_db_postgresql_get_uint(result, i, 4, &file->ownerid);
			st_db_postgresql_get_string(result, i, 5, file->owner, 32);
			st_db_postgresql_get_uint(result, i, 6, &file->groupid);
			st_db_postgresql_get_string(result, i, 7, file->group, 32);
			st_db_postgresql_get_uint(result, i, 8, &file->perm);
			st_db_postgresql_get_time(result, i, 9, &file->create_time);
			st_db_postgresql_get_time(result, i, 10, &file->modify_time);
			st_db_postgresql_get_time(result, i, 11, &file->archived_time);
			st_db_postgresql_get_bool(result, i, 12, &file->check_ok);
			if (!PQgetisnull(result, i, 13))
				st_db_postgresql_get_time(result, i, 13, &file->check_time);
			st_db_postgresql_get_ssize(result, i, 14, &file->size);

			file->archive = volume->archive;
			file->selected_path = NULL;

			struct st_db_postgresql_archive_file_data * file_data = file->db_data = malloc(sizeof(struct st_db_postgresql_archive_file_data));
			st_db_postgresql_get_long(result, i, 0, &file_data->id);

			struct st_archive_files * f = volume->files + i;
			f->file = file;
			st_db_postgresql_get_ssize(result, i, 15, &f->position);

			char * fileid;
			st_db_postgresql_get_string_dup(result, i, 0, &fileid);

			const char * param2[] = { fileid };
			PGresult * result2 = PQexecPrepared(self->connect, query2, 1, param2, NULL, NULL, 0);
			ExecStatusType status2 = PQresultStatus(result2);
			unsigned int nb_digest = PQntuples(result2);

			if (status2 == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result, query2);
			else if (status2 == PGRES_TUPLES_OK && nb_digest > 0) {
				file->digests = st_hashtable_new2(st_util_string_compute_hash, st_util_basic_free);

				unsigned int j;
				for (j = 0; j < nb_digest; j++) {
					char * checksum, * result;

					st_db_postgresql_get_string_dup(result2, j, 0, &checksum);
					st_db_postgresql_get_string_dup(result2, j, 1, &result);

					st_hashtable_put(file->digests, checksum, st_hashtable_val_string(result));
				}
			}

			PQclear(result2);
			free(fileid);
		}
	}

	PQclear(result);
	free(jobid);
	free(archivevolumeid);

	return 0;
}

static struct st_archive * st_db_postgresql_get_archive_volumes_by_job(struct st_database_connection * connect, struct st_job * job) {
	if (connect == NULL || job == NULL)
		return NULL;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_job_data * job_data = job->db_data;
	if (job_data == NULL || job_data->job_id < 0)
		return NULL;

	const char * query = "select_archive_volume_by_job";
	st_db_postgresql_prepare(self, query, "SELECT a.id, a.uuid, name, u.login FROM archive a LEFT JOIN users u ON a.owner = u.id WHERE a.id IN (SELECT archive FROM job WHERE id = $1 LIMIT 1)");

	char * jobid;
	asprintf(&jobid, "%ld", job_data->job_id);

	const char * param[] = { jobid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	struct st_archive * archive = NULL;

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		archive = malloc(sizeof(struct st_archive));

		st_db_postgresql_get_string(result, 0, 1, archive->uuid, 36);
		st_db_postgresql_get_string_dup(result, 0, 2, &archive->name);
		archive->metadatas = NULL;

		archive->volumes = NULL;
		archive->nb_volumes = 0;

		archive->user = st_user_get(PQgetvalue(result, 0, 3));

		archive->copy_of = NULL;

		struct st_db_postgresql_archive_data * archive_data = archive->db_data = malloc(sizeof(struct st_db_postgresql_archive_data));
		st_db_postgresql_get_long(result, 0, 0, &archive_data->id);
	}

	PQclear(result);

	if (archive == NULL) {
		free(jobid);
		return NULL;
	}

	if (st_db_postgresql_has_selected_files_by_job(connect, job)) {
		query = "select_archive_volumes_by_job_with_selected_files";
		st_db_postgresql_prepare(self, query, "SELECT DISTINCT av.id, av.sequence, av.starttime, av.endtime, av.checksumok, av.checktime, av.size, m.uuid, av.mediaposition FROM job j LEFT JOIN archivevolume av ON j.archive = av.archive LEFT JOIN archivefiletoarchivevolume afv ON av.id = afv.archivevolume LEFT JOIN archivefile af ON afv.archivefile = af.id LEFT JOIN media m ON av.media = m.id, (SELECT DISTINCT path, CHAR_LENGTH(path) AS length FROM selectedfile WHERE id IN (SELECT selectedfile FROM jobtoselectedfile WHERE job = $1)) AS sf WHERE j.id = $1 AND SUBSTR(af.name, 0, sf.length + 1) = sf.path ORDER BY sequence");
	} else {
		query = "select_archive_volumes_by_job";
		st_db_postgresql_prepare(self, query, "SELECT av.id, av.sequence, av.starttime, av.endtime, av.checksumok, av.checktime, av.size, m.uuid, av.mediaposition FROM job j LEFT JOIN archive a ON j.archive = a.id LEFT JOIN archivevolume av ON a.id = av.archive LEFT JOIN media m ON av.media = m.id WHERE j.id = $1 ORDER BY sequence");
	}

	result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	status = PQresultStatus(result);
	int nb_tuples = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_tuples > 0) {
		archive->volumes = calloc(nb_tuples, sizeof(struct st_archive_volume));
		archive->nb_volumes = nb_tuples;

		unsigned i;
		for (i = 0; i < archive->nb_volumes; i++) {
			struct st_archive_volume * volume = archive->volumes + i;

			st_db_postgresql_get_long(result, i, 1, &volume->sequence);
			st_db_postgresql_get_time(result, i, 2, &volume->start_time);
			st_db_postgresql_get_time(result, i, 3, &volume->end_time);
			st_db_postgresql_get_bool(result, i, 4, &volume->check_ok);
			volume->check_time = 0;
			if (!PQgetisnull(result, i, 5))
				st_db_postgresql_get_time(result, i, 5, &volume->check_time);

			st_db_postgresql_get_ssize(result, i, 6, &volume->size);
			volume->archive = archive;
			volume->job = NULL;

			char uuid[37];
			st_db_postgresql_get_string(result, i, 7, uuid, 37);
			volume->media = st_media_get_by_uuid(uuid);
			st_db_postgresql_get_long(result, i, 8, &volume->media_position);

			volume->digests = NULL;

			volume->files = NULL;
			volume->nb_files = 0;

			struct st_db_postgresql_archive_volume_data * archive_volume_data = volume->db_data = malloc(sizeof(struct st_db_postgresql_archive_volume_data));
			st_db_postgresql_get_long(result, i, 0, &archive_volume_data->id);

			st_db_postgresql_get_checksums_of_archive_volume(connect, volume);
		}
	}

	free(jobid);
	PQclear(result);

	return archive;
}

static unsigned int st_db_postgresql_get_checksums_of_archive_volume(struct st_database_connection * connect, struct st_archive_volume * volume) {
	if (connect == NULL || volume == NULL)
		return 0;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_archive_volume_data * volume_data = volume->db_data;
	if (volume_data == NULL || volume_data->id < 0)
		return 0;

	const char * query = "select_checksum_of_archive_volume";
	st_db_postgresql_prepare(self, query, "SELECT c.name, cr.result FROM archivevolumetochecksumresult avcr LEFT JOIN checksumresult cr ON avcr.checksumresult = cr.id LEFT JOIN checksum c ON cr.checksum = c.id WHERE archivevolume = $1");

	char * volumeid;
	asprintf(&volumeid, "%ld", volume_data->id);

	const char * param[] = { volumeid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	unsigned int nb_checksums = PQntuples(result);

	if (volume->digests != NULL)
		st_hashtable_clear(volume->digests);
	else
		volume->digests = st_hashtable_new2(st_util_string_compute_hash, st_util_basic_free);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_checksums > 0) {
		unsigned int i;
		for (i = 0; i < nb_checksums; i++) {
			char * checksum, * cksum_result;

			st_db_postgresql_get_string_dup(result, i, 0, &checksum);
			st_db_postgresql_get_string_dup(result, i, 1, &cksum_result);

			st_hashtable_put(volume->digests, checksum, st_hashtable_val_string(cksum_result));
		}
	}

	PQclear(result);
	free(volumeid);

	return nb_checksums;
}

static unsigned int st_db_postgresql_get_checksums_of_file(struct st_database_connection * connect, struct st_archive_file * file) {
	if (connect == NULL || file == NULL)
		return 0;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_archive_file_data * file_data = file->db_data;
	if (file_data == NULL || file_data->id < 0)
		return 0;

	const char * query = "select_checksum_of_file";
	st_db_postgresql_prepare(self, query, "SELECT c.name, cr.result FROM archivefiletochecksumresult afcr LEFT JOIN checksumresult cr ON afcr.checksumresult = cr.id LEFT JOIN checksum c ON cr.checksum = c.id WHERE archivefile = $1");

	char * fileid;
	asprintf(&fileid, "%ld", file_data->id);

	const char * param[] = { fileid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	unsigned int nb_checksums = PQntuples(result);

	if (file->digests != NULL)
		st_hashtable_clear(file->digests);
	else
		file->digests = st_hashtable_new2(st_util_string_compute_hash, st_util_basic_free);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_checksums > 0) {
		unsigned int i;
		for (i = 0; i < nb_checksums; i++) {
			char * checksum, * cksum_result;

			st_db_postgresql_get_string_dup(result, i, 0, &checksum);
			st_db_postgresql_get_string_dup(result, i, 1, &cksum_result);

			st_hashtable_put(file->digests, checksum, st_hashtable_val_string(cksum_result));
		}
	}

	PQclear(result);
	free(fileid);

	return nb_checksums;
}

static unsigned int st_db_postgresql_get_nb_volume_of_file(struct st_database_connection * connect, struct st_job * job, struct st_archive_file * file) {
	if (connect == NULL || job == NULL || file == NULL)
		return 0;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_job_data * job_data = job->db_data;
	struct st_db_postgresql_archive_file_data * file_data = file->db_data;
	if (job_data == NULL || job_data->job_id < 0 || file_data == NULL || file_data->id < 0)
		return 0;

	const char * query = "select_nb_volume_of_file";
	st_db_postgresql_prepare(self, query, "SELECT COUNT(*) FROM archivefiletoarchivevolume afv LEFT JOIN archivevolume av ON afv.archivevolume = av.id LEFT JOIN job j ON j.archive = av.archive WHERE j.id = $1 AND afv.archivefile = $2");

	char * jobid, * fileid;
	asprintf(&jobid, "%ld", job_data->job_id);
	asprintf(&fileid, "%ld", file_data->id);

	const char * param[] = { jobid, fileid };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	unsigned int nb_volume = 0;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_uint(result, 0, 0, &nb_volume);

	PQclear(result);
	free(jobid);
	free(fileid);

	return nb_volume;
}

static char * st_db_postgresql_get_restore_path_from_file(struct st_database_connection * connect, struct st_job * job, struct st_archive_file * file) {
	if (connect == NULL || job == NULL || file == NULL)
		return NULL;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_job_data * job_data = job->db_data;
	struct st_db_postgresql_archive_file_data * file_data = file->db_data;
	if (job_data == NULL || job_data->job_id < 0 || file_data == NULL || file_data->id < 0)
		return NULL;

	const char * query = "select_get_restore_path_from_job_and_file";
	st_db_postgresql_prepare(self, query, "SELECT rt.path || SUBSTRING(af.name FROM CHAR_LENGTH(SUBSTRING(sf.path FROM '(.+/)[^/]+'))) AS name FROM archivefile af LEFT JOIN selectedfile sf ON af.parent = sf.id, restoreto rt WHERE rt.job = $1 AND af.id = $2 LIMIT 1");

	char * jobid, * fileid;
	asprintf(&jobid, "%ld", job_data->job_id);
	asprintf(&fileid, "%ld", file_data->id);

	const char * param[] = { jobid, fileid };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	char * path = NULL;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_string_dup(result, 0, 0, &path);

	PQclear(result);
	free(jobid);
	free(fileid);

	return path;
}

static char * st_db_postgresql_get_restore_path_of_job(struct st_database_connection * connect, struct st_job * job) {
	if (connect == NULL || job == NULL)
		return NULL;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_job_data * job_data = job->db_data;
	if (job_data == NULL || job_data->job_id < 0)
		return NULL;

	const char * query = "select_get_restore_path_of_job";
	st_db_postgresql_prepare(self, query, "SELECT path FROM restoreto WHERE job = $1 LIMIT 1");

	char * jobid;
	asprintf(&jobid, "%ld", job_data->job_id);

	const char * param[] = { jobid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	char * path = NULL;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_string_dup(result, 0, 0, &path);

	PQclear(result);
	free(jobid);

	return path;
}

static ssize_t st_db_postgresql_get_restore_size_by_job(struct st_database_connection * connect, struct st_job * job) {
	if (connect == NULL || job == NULL)
		return false;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_job_data * job_data = job->db_data;
	if (job_data == NULL || job_data->job_id < 0)
		return -1;

	const char * query;
	if (st_db_postgresql_has_selected_files_by_job(connect, job)) {
		query = "select_size_of_archive_for_restore_with_selected_files";
		st_db_postgresql_prepare(self, query, "SELECT SUM(af.size) AS size FROM job j LEFT JOIN archivevolume av ON j.archive = av.archive LEFT JOIN archivefiletoarchivevolume afv ON av.id = afv.archivevolume LEFT JOIN archivefile af ON afv.archivefile = af.id, (SELECT path, CHAR_LENGTH(path) AS length FROM selectedfile ssf2 LEFT JOIN jobtoselectedfile sjsf ON ssf2.id = sjsf.selectedfile WHERE sjsf.job = $1) AS ssf WHERE j.id = $1 AND SUBSTR(af.name, 0, ssf.length + 1) = ssf.path AND af.type = 'regular file'");
	} else {
		query = "select_size_of_archive_for_restore";
		st_db_postgresql_prepare(self, query, "SELECT SUM(af.size) AS size FROM job j LEFT JOIN archivevolume av ON j.archive = av.archive LEFT JOIN archivefiletoarchivevolume afv ON av.id = afv.archivevolume LEFT JOIN archivefile af ON afv.archivefile = af.id WHERE j.id = $1 AND af.type = 'regular file'");
	}

	char * jobid;
	asprintf(&jobid, "%ld", job_data->job_id);

	const char * param[] = { jobid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	ssize_t size = -1;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_ssize(result, 0, 0, &size);

	PQclear(result);
	free(jobid);

	return size;
}

static bool st_db_postgresql_has_restore_to_by_job(struct st_database_connection * connect, struct st_job * job) {
	if (connect == NULL || job == NULL)
		return false;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_job_data * job_data = job->db_data;
	if (job_data == NULL || job_data->job_id < 0)
		return false;

	const char * query = "select_has_restore_to_by_job";
	st_db_postgresql_prepare(self, query, "SELECT COUNT(*) > 0 FROM restoreto WHERE job = $1");

	bool has_restore_to = false;

	char * jobid;
	asprintf(&jobid, "%ld", job_data->job_id);

	const char * param0[] = { jobid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param0, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_bool(result, 0, 0, &has_restore_to);

	PQclear(result);
	free(jobid);

	return has_restore_to;
}

static bool st_db_postgresql_has_selected_files_by_job(struct st_database_connection * connect, struct st_job * job) {
	if (connect == NULL || job == NULL)
		return false;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_job_data * job_data = job->db_data;
	if (job_data == NULL || job_data->job_id < 0)
		return false;

	const char * query = "select_has_selected_files_from_restore_archive";
	st_db_postgresql_prepare(self, query, "SELECT COUNT(*) > 0 FROM jobtoselectedfile WHERE job = $1");

	bool has_selected_files = false;

	char * jobid;
	asprintf(&jobid, "%ld", job_data->job_id);

	const char * param0[] = { jobid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param0, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_bool(result, 0, 0, &has_selected_files);

	PQclear(result);
	free(jobid);

	return has_selected_files;
}

static bool st_db_postgresql_mark_archive_file_as_checked(struct st_database_connection * connect, struct st_archive * archive, struct st_archive_file * file, bool ok) {
	if (connect == NULL || archive == NULL || file == NULL)
		return 1;

	struct st_db_postgresql_archive_data * archive_data = archive->db_data;
	if (archive_data->id < 0)
		return false;

	struct st_db_postgresql_archive_file_data * file_data = file->db_data;
	if (file_data->id < 0)
		return false;

	struct st_db_postgresql_connection_private * self = connect->data;

	char * fileid = NULL, * archiveid = NULL;
	asprintf(&fileid, "%ld", file_data->id);
	asprintf(&archiveid, "%ld", archive_data->id);

	const char * query = "mark_archive_file_as_checked";
	st_db_postgresql_prepare(self, query, "UPDATE archivefiletoarchivevolume SET checksumok = $3, checktime = NOW() WHERE archivevolume IN (SELECT id FROM archivevolume WHERE archive = $1) AND archivefile = $2");

	const char * param[] = { archiveid, fileid, ok ? "true" : "false" };
	PGresult * result = PQexecPrepared(self->connect, query, 3, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	bool query_ok = false;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_COMMAND_OK)
		query_ok = true;

	PQclear(result);
	free(archiveid);
	free(fileid);

	return query_ok;
}

static bool st_db_postgresql_mark_archive_volume_as_checked(struct st_database_connection * connect, struct st_archive_volume * volume, bool ok) {
	if (connect == NULL || volume == NULL)
		return 1;

	struct st_db_postgresql_archive_volume_data * volume_data = volume->db_data;
	if (volume_data->id < 0)
		return false;

	struct st_db_postgresql_connection_private * self = connect->data;

	char * volumeid = NULL;
	asprintf(&volumeid, "%ld", volume_data->id);

	const char * query = "mark_archive_volume_as_checked";
	st_db_postgresql_prepare(self, query, "UPDATE archivevolume SET checksumok = $2, checktime = NOW() WHERE id = $1");

	const char * param[] = { volumeid, ok ? "true" : "false" };
	PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	bool query_ok = false;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_COMMAND_OK)
		query_ok = true;

	PQclear(result);
	free(volumeid);

	return query_ok;
}

static int st_db_postgresql_sync_archive(struct st_database_connection * connect, struct st_archive * archive) {
	if (connect == NULL || archive == NULL)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_archive_data * archive_data = archive->db_data;
	struct st_db_postgresql_archive_data * copy_data = NULL;

	char * archiveid = NULL, * copyid = NULL;
	if (archive_data == NULL) {
		archive->db_data = archive_data = malloc(sizeof(struct st_db_postgresql_archive_data));
		archive_data->id = -1;
	} else
		asprintf(&archiveid, "%ld", archive_data->id);

	if (archive->copy_of != NULL) {
		copy_data = archive->copy_of->db_data;
		asprintf(&copyid, "%ld", copy_data->id);
	}

	if (archive_data->id < 0) {
		const char * query = "insert_archive";
		st_db_postgresql_prepare(self, query, "INSERT INTO archive(uuid, name, creator, owner, copyof) VALUES ($1, $2, $3, $3, $4) RETURNING id");

		char * userid = NULL;

		struct st_db_postgresql_user_data * user_data = archive->user->db_data;
		asprintf(&userid, "%ld", user_data->id);

		const char * param[] = {
			archive->uuid, archive->name, userid, copyid
		};
		PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_long(result, 0, 0, &archive_data->id);
			st_db_postgresql_get_string_dup(result, 0, 0, &archiveid);
		}

		PQclear(result);
		free(userid);

		struct st_hashtable * metadatas = st_util_json_from_string(archive->metadatas);
		if (metadatas != NULL) {
			if (st_hashtable_has_key(metadatas, "archive")) {
				const char * query2 = "insert_metadata_archive";
				st_db_postgresql_prepare(self, query2, "INSERT INTO metadata(key, value, id, type) VALUES ($1, $2, $3, 'archive')");

				struct st_hashtable_value archive = st_hashtable_get(metadatas, "archive");
				struct st_hashtable * archive_metadatas = st_util_json_from_string(archive.value.string);

				unsigned int nb_keys = 0;
				const void ** keys = st_hashtable_keys(archive_metadatas, &nb_keys);

				unsigned int i;
				for (i = 0; i < nb_keys; i++) {
					struct st_hashtable_value val = st_hashtable_get(archive_metadatas, keys[i]);
					char * value = NULL;
					if (val.type == st_hashtable_value_string)
						value = val.value.string;

					const char * param2[] = { keys[i], value, archiveid };
					PGresult * result2 = PQexecPrepared(self->connect, query2, 3, param2, NULL, NULL, 0);
					ExecStatusType status2 = PQresultStatus(result2);

					if (status2 == PGRES_FATAL_ERROR)
						st_db_postgresql_get_error(result2, query2);

					PQclear(result2);
				}

				free(keys);
				st_hashtable_free(archive_metadatas);
			}
			st_hashtable_free(metadatas);
		}
	}

	const char * query = "select_last_volume_of_archive";
	st_db_postgresql_prepare(self, query, "SELECT MAX(sequence) + 1 FROM archivevolume WHERE archive = $1");

	const char * param[] = { archiveid };
	PGresult * result = PQexecPrepared(self->connect, query, 1, param, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	unsigned int last_volume = 0;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_uint(result, 0, 0, &last_volume);

	PQclear(result);
	free(archiveid);
	free(copyid);

	unsigned int i;
	int failed = 0;
	for (i = 0; i < archive->nb_volumes && !failed; i++) {
		struct st_archive_volume * volume = archive->volumes + i;
		volume->sequence += last_volume;
		failed = st_db_postgresql_sync_volume(connect, archive, volume);
	}

	return failed;
}

static int st_db_postgresql_sync_file(struct st_database_connection * connect, struct st_archive * archive, struct st_archive_file * file, char ** file_id) {
	struct st_db_postgresql_archive_file_data * file_data = file->db_data;
	if (file_data != NULL) {
		asprintf(file_id, "%ld", file_data->id);
		return 0;
	} else {
		file->db_data = file_data = malloc(sizeof(struct st_db_postgresql_archive_file_data));
		file_data->id = -1;
	}

	struct st_db_postgresql_connection_private * self = connect->data;

	struct st_db_postgresql_selected_path_data * selected_data = file->selected_path->db_data;

	const char * query = "insert_archive_file";
	st_db_postgresql_prepare(self, query, "INSERT INTO archivefile(name, type, mimetype, ownerid, owner, groupid, groups, perm, ctime, mtime, size, parent) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12) RETURNING id");

	char * perm, * ownerid, * groupid, * size, * parent;
	asprintf(&perm, "%d", file->perm & 07777);
	asprintf(&ownerid, "%d", file->ownerid);
	asprintf(&groupid, "%d", file->groupid);
	asprintf(&size, "%zd", file->size);
	asprintf(&parent, "%ld", selected_data->id);

	char ctime[32], mtime[32];
	st_util_time_convert(&file->create_time, "%F %T", ctime, 32);
	st_util_time_convert(&file->modify_time, "%F %T", mtime, 32);

	const char * param[] = {
		file->name, st_archive_file_type_to_string(file->type),
		file->mime_type, ownerid, file->owner, groupid, file->group, perm,
		ctime, mtime, size, parent
	};
	PGresult * result = PQexecPrepared(self->connect, query, 12, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	free(perm);
	free(ownerid);
	free(groupid);
	free(size);
	free(parent);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &file_data->id);
		st_db_postgresql_get_string_dup(result, 0, 0, file_id);
	}

	PQclear(result);

	if (status == PGRES_FATAL_ERROR)
		return 1;

	struct st_hashtable * metadatas = st_util_json_from_string(archive->metadatas);
	if (metadatas != NULL) {
		if (st_hashtable_has_key(metadatas, "files")) {
			struct st_hashtable_value files = st_hashtable_get(metadatas, "files");
			struct st_hashtable * files_metadatas = st_util_json_from_string(files.value.string);

			if (st_hashtable_has_key(files_metadatas, file->name)) {
				struct st_hashtable_value vfile = st_hashtable_get(files_metadatas, file->name);
				struct st_hashtable * file_metadatas = st_util_json_from_string(vfile.value.string);

				const char * query2 = "insert_metadata_files";
				st_db_postgresql_prepare(self, query2, "INSERT INTO metadata(key, value, id, type) VALUES ($1, $2, $3, 'archivefile')");

				unsigned int nb_keys = 0;
				const void ** keys = st_hashtable_keys(file_metadatas, &nb_keys);

				unsigned int i;
				for (i = 0; i < nb_keys; i++) {
					struct st_hashtable_value val = st_hashtable_get(file_metadatas, keys[i]);
					char * value = NULL;
					if (val.type == st_hashtable_value_string)
						value = val.value.string;

					const char * param2[] = { keys[i], value, *file_id };
					PGresult * result2 = PQexecPrepared(self->connect, query2, 3, param2, NULL, NULL, 0);
					ExecStatusType status2 = PQresultStatus(result2);

					if (status2 == PGRES_FATAL_ERROR)
						st_db_postgresql_get_error(result2, query2);

					PQclear(result2);
				}

				free(keys);
				st_hashtable_free(file_metadatas);
			}

			st_hashtable_free(files_metadatas);
		}
		st_hashtable_free(metadatas);
	}

	if (file->digests == NULL || file->digests->nb_elements == 0)
		return 0;

	query = "insert_archive_file_to_checksum";
	st_db_postgresql_prepare(self, query, "INSERT INTO archivefiletochecksumresult VALUES ($1, $2)");

	unsigned int i;
	int failed = 0;
	unsigned int nb_digests;
	const void ** checksums = st_hashtable_keys(file->digests, &nb_digests);
	for (i = 0; i < nb_digests; i++) {
		char * checksum_result = NULL;
		struct st_hashtable_value digest = st_hashtable_get(file->digests, checksums[i]);

		failed = st_db_postgresql_add_checksum_result(connect, checksums[i], digest.value.string, &checksum_result);
		if (failed)
			break;

		const char * param[] = { *file_id, checksum_result };
		PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR) {
			st_db_postgresql_get_error(result, query);
			failed = 1;
		}

		PQclear(result);

		free(checksum_result);
	}
	free(checksums);

	return failed;
}

static int st_db_postgresql_sync_volume(struct st_database_connection * connect, struct st_archive * archive, struct st_archive_volume * volume) {
	struct st_db_postgresql_connection_private * self = connect->data;
	struct st_db_postgresql_archive_data * archive_data = archive->db_data;
	struct st_db_postgresql_archive_volume_data * archive_volume_data = volume->db_data;
	struct st_db_postgresql_job_data * job_data = volume->job->db_data;
	struct st_db_postgresql_media_data * media_data = volume->media->db_data;

	if (archive_volume_data == NULL) {
		volume->db_data = archive_volume_data = malloc(sizeof(struct st_db_postgresql_archive_volume_data));
		archive_volume_data->id = -1;
	}

	char * volumeid = NULL;

	if (archive_volume_data->id < 0) {
		const char * query = "insert_archive_volume";
		st_db_postgresql_prepare(self, query, "INSERT INTO archivevolume(sequence, size, starttime, endtime, archive, media, mediaposition, jobrun) VALUES ($1, $2, $3, $4, $5, $6, $7, $8) RETURNING id");

		char buffer_ctime[32], buffer_endtime[32];
		st_util_time_convert(&volume->start_time, "%F %T", buffer_ctime, 32);
		st_util_time_convert(&volume->end_time, "%F %T", buffer_endtime, 32);

		char * sequence, * size, * archiveid, * mediaid, * jobrunid, * mediaposition;
		asprintf(&sequence, "%ld", volume->sequence);
		asprintf(&size, "%zd", volume->size);
		asprintf(&archiveid, "%ld", archive_data->id);
		asprintf(&mediaid, "%ld", media_data->id);
		asprintf(&jobrunid, "%ld", job_data->jobrun_id);
		asprintf(&mediaposition, "%ld", volume->media_position);

		const char * param[] = {
			sequence, size, buffer_ctime, buffer_endtime, archiveid, mediaid, mediaposition, jobrunid
		};
		PGresult * result = PQexecPrepared(self->connect, query, 8, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_long(result, 0, 0, &archive_volume_data->id);
			st_db_postgresql_get_string_dup(result, 0, 0, &volumeid);
		}

		free(sequence);
		free(size);
		free(archiveid);
		free(mediaid);
		free(mediaposition);
		free(jobrunid);

		PQclear(result);

		if (status == PGRES_FATAL_ERROR)
			return 1;
	}

	const char * query = "insert_archive_volume_to_checksum";
	st_db_postgresql_prepare(self, query, "INSERT INTO archivevolumetochecksumresult VALUES ($1, $2)");

	unsigned int i;
	int failed = 0;

	if (volume->digests != NULL) {
		unsigned int nb_digests;
		const void ** checksums = st_hashtable_keys(volume->digests, &nb_digests);
		for (i = 0; i < nb_digests; i++) {
			char * checksum_result = NULL;
			struct st_hashtable_value digest = st_hashtable_get(volume->digests, checksums[i]);

			failed = st_db_postgresql_add_checksum_result(connect, checksums[i], digest.value.string, &checksum_result);
			if (failed)
				break;

			const char * param[] = { volumeid, checksum_result };
			PGresult * result = PQexecPrepared(self->connect, query, 2, param, NULL, NULL, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR) {
				st_db_postgresql_get_error(result, query);
				failed = 1;
			}

			PQclear(result);

			free(checksum_result);
		}
		free(checksums);
	}

	if (failed) {
		free(volumeid);
		return failed;
	}

	query = "insert_archivefiletoarchivevolume";
	st_db_postgresql_prepare(self, query, "INSERT INTO archivefiletoarchivevolume(archivevolume, archivefile, blocknumber, archivetime) VALUES ($1, $2, $3, $4)");

	for (i = 0; i < volume->nb_files && !failed; i++) {
		struct st_archive_files * f = volume->files + i;
		char * file_id;

		failed = st_db_postgresql_sync_file(connect, archive, f->file, &file_id);

		char * block_number;
		asprintf(&block_number, "%zd", f->position);

		char atime[32];
		st_util_time_convert(&f->file->archived_time, "%F %T", atime, 32);

		const char * param[] = { volumeid, file_id, block_number, atime };
		PGresult * result = PQexecPrepared(self->connect, query, 4, param, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);

		PQclear(result);
		free(block_number);
		free(file_id);
	}

	free(volumeid);
	return failed;
}



static struct st_vtl_config * st_db_postgresql_get_vtls(struct st_database_connection * connect, unsigned int * nb_vtls) {
	char * host_id = st_db_postgresql_get_host(connect);
	if (host_id == NULL)
		return NULL;

	struct st_db_postgresql_connection_private * self = connect->data;

	const char * query = "select_vtls";
	st_db_postgresql_prepare(self, query, "SELECT uuid, path, prefix, nbslots, nbdrives, name, deleted FROM vtl v LEFT JOIN mediaformat mf ON v.mediaformat = mf.id WHERE host = $1");

	const char * params[] = { host_id };
	PGresult * result = PQexecPrepared(self->connect, query, 1, params, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);
	int nb_result = PQntuples(result);

	free(host_id);

	struct st_vtl_config * cfg = NULL;

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result, query);
		if (nb_vtls != NULL)
			*nb_vtls = 0;
	} else if (status == PGRES_TUPLES_OK && nb_result > 0) {
		cfg = calloc(sizeof(struct st_vtl_config), nb_result);
		if (nb_vtls != NULL)
			*nb_vtls = nb_result;

		int i;
		for (i = 0; i < nb_result; i++) {
			struct st_vtl_config * conf = cfg + i;
			st_db_postgresql_get_string_dup(result, i, 0, &conf->uuid);
			st_db_postgresql_get_string_dup(result, i, 1, &conf->path);
			st_db_postgresql_get_string_dup(result, i, 2, &conf->prefix);
			st_db_postgresql_get_uint(result, i, 3, &conf->nb_slots);
			st_db_postgresql_get_uint(result, i, 4, &conf->nb_drives);
			st_db_postgresql_get_bool(result, i, 6, &conf->deleted);

			const char * format = PQgetvalue(result, i, 5);
			conf->format = st_media_format_get_by_name(format, st_media_format_mode_disk);
		}
	} else if (status == PGRES_TUPLES_OK && nb_result == 0) {
		cfg = malloc(0);
	}

	PQclear(result);

	return cfg;
}

static int st_db_postgresql_delete_vtl(struct st_database_connection * connect, struct st_vtl_config * config) {
	if (connect == NULL || config == NULL)
		return 1;

	char * host_id = st_db_postgresql_get_host(connect);
	if (host_id == NULL)
		return 1;

	struct st_db_postgresql_connection_private * self = connect->data;

	const char * query = "delete_vtl";
	st_db_postgresql_prepare(self, query, "DELETE FROM vtl WHERE path = $1 AND host = $2");

	const char * params[] = { config->path, host_id };
	PGresult * result = PQexecPrepared(self->connect, query, 2, params, NULL, NULL, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	PQclear(result);

	return status != PGRES_COMMAND_OK;
}


static void st_db_postgresql_prepare(struct st_db_postgresql_connection_private * self, const char * statement_name, const char * query) {
	if (!st_hashtable_has_key(self->cached_query, statement_name)) {
		PGresult * prepare = PQprepare(self->connect, statement_name, query, 0, NULL);
		ExecStatusType status = PQresultStatus(prepare);
		if (status == PGRES_FATAL_ERROR) {
			st_db_postgresql_get_error(prepare, statement_name);
			st_log_write_all(status == PGRES_COMMAND_OK ? st_log_level_debug : st_log_level_error, st_log_type_plugin_db, "Postgresql: new query prepared (%s) => {%s}, status: %s", statement_name, query, PQresStatus(status));
		} else
			st_hashtable_put(self->cached_query, strdup(statement_name), st_hashtable_val_null());
		PQclear(prepare);
	}
}

