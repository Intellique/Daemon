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
*  Last modified: Tue, 13 Mar 2012 18:48:47 +0100                         *
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

#include <stone/job.h>
#include <stone/library/archive.h>
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/tape.h>
#include <stone/log.h>
#include <stone/user.h>

#include "common.h"

static void st_db_postgresql_check(struct st_database_connection * connection);
static int st_db_postgresql_con_close(struct st_database_connection * connection);
static int st_db_postgresql_con_free(struct st_database_connection * connection);

static int st_db_postgresql_cancel_checkpoint(struct st_database_connection * connection, const char * checkpoint);
static int st_db_postgresql_cancel_transaction(struct st_database_connection * connection);
static int st_db_postgresql_create_checkpoint(struct st_database_connection * connection, const char * checkpoint);
static int st_db_postgresql_finish_transaction(struct st_database_connection * connection);
static int st_db_postgresql_start_transaction(struct st_database_connection * connection);

static int st_db_postgresql_add_job_record(struct st_database_connection * db, struct st_job * job, const char * message);
static int st_db_postgresql_create_pool(struct st_database_connection * db, struct st_pool * pool);
static char * st_db_postgresql_get_host(struct st_database_connection * connection);
static int st_db_postgresql_get_nb_new_jobs(struct st_database_connection * db, long * nb_new_jobs, time_t since, long last_max_jobs);
static int st_db_postgresql_get_new_jobs(struct st_database_connection * db, struct st_job ** jobs, unsigned int nb_jobs, time_t since, long last_max_jobs);
static int st_db_postgresql_get_pool(struct st_database_connection * db, struct st_pool * pool, long id, const char * uuid);
static int st_db_postgresql_get_tape(struct st_database_connection * db, struct st_tape * tape, long id, const char * uuid);
static int st_db_postgresql_get_tape_format(struct st_database_connection * db, struct st_tape_format * tape_format, long id, unsigned char density_code);
static int st_db_postgresql_get_user(struct st_database_connection * db, struct st_user * user, long user_id, const char * login);
static int st_db_postgresql_is_changer_contain_drive(struct st_database_connection * db, struct st_changer * changer, struct st_drive * drive);
static int st_db_postgresql_refresh_job(struct st_database_connection * db, struct st_job * job);
static int st_db_postgresql_sync_changer(struct st_database_connection * db, struct st_changer * changer);
static int st_db_postgresql_sync_drive(struct st_database_connection * db, struct st_drive * drive);
static int st_db_postgresql_sync_plugin_checksum(struct st_database_connection * db, const char * plugin);
static int st_db_postgresql_sync_plugin_job(struct st_database_connection * db, const char * plugin);
static int st_db_postgresql_sync_pool(struct st_database_connection * db, struct st_pool * pool);
static int st_db_postgresql_sync_slot(struct st_database_connection * db, struct st_slot * slot);
static int st_db_postgresql_sync_tape(struct st_database_connection * db, struct st_tape * tape);
static int st_db_postgresql_sync_user(struct st_database_connection * db, struct st_user * user);
static int st_db_postgresql_update_job(struct st_database_connection * db, struct st_job * job);

static int st_db_postgresql_file_add_checksum(struct st_database_connection * db, struct st_archive_file * file);
static int st_db_postgresql_file_link_to_volume(struct st_database_connection * db, struct st_archive_file * file, struct st_archive_volume * volume);
static int st_db_postgresql_new_archive(struct st_database_connection * db, struct st_archive * archive);
static int st_db_postgresql_new_file(struct st_database_connection * db, struct st_archive_file * file);
static int st_db_postgresql_new_volume(struct st_database_connection * db, struct st_archive_volume * volume);
static int st_db_postgresql_update_archive(struct st_database_connection * db, struct st_archive * archive);
static int st_db_postgresql_update_volume(struct st_database_connection * db, struct st_archive_volume * volume);

static int st_db_postgresql_get_bool(PGresult * result, int row, int column, char * value);
static int st_db_postgresql_get_double(PGresult * result, int row, int column, double * value);
static void st_db_postgresql_get_error(PGresult * result);
static int st_db_postgresql_get_int(PGresult * result, int row, int column, int * value);
static int st_db_postgresql_get_long(PGresult * result, int row, int column, long * value);
static int st_db_postgresql_get_ssize(PGresult * result, int row, int column, ssize_t * value);
static int st_db_postgresql_get_string(PGresult * result, int row, int column, char * value);
static int st_db_postgresql_get_string_dup(PGresult * result, int row, int column, char ** value);
static int st_db_postgresql_get_time(PGresult * result, int row, int column, time_t * value);
static int st_db_postgresql_get_uchar(PGresult * result, int row, int column, unsigned char * value);
static void st_db_postgresql_prepare(struct st_db_postgresql_connetion_private * self, const char * statement_name, const char * query);

static struct st_database_connection_ops st_db_postgresql_con_ops = {
	.close = st_db_postgresql_con_close,
	.free  = st_db_postgresql_con_free,

	.cancel_transaction = st_db_postgresql_cancel_transaction,
	.finish_transaction = st_db_postgresql_finish_transaction,
	.start_transaction  = st_db_postgresql_start_transaction,

	.add_job_record           = st_db_postgresql_add_job_record,
	.create_pool              = st_db_postgresql_create_pool,
	.get_nb_new_jobs          = st_db_postgresql_get_nb_new_jobs,
	.get_new_jobs             = st_db_postgresql_get_new_jobs,
	.get_pool                 = st_db_postgresql_get_pool,
	.get_tape                 = st_db_postgresql_get_tape,
	.get_tape_format          = st_db_postgresql_get_tape_format,
	.get_user                 = st_db_postgresql_get_user,
	.is_changer_contain_drive = st_db_postgresql_is_changer_contain_drive,
	.refresh_job              = st_db_postgresql_refresh_job,
	.sync_changer             = st_db_postgresql_sync_changer,
	.sync_drive               = st_db_postgresql_sync_drive,
	.sync_plugin_checksum     = st_db_postgresql_sync_plugin_checksum,
	.sync_plugin_job          = st_db_postgresql_sync_plugin_job,
	.sync_pool                = st_db_postgresql_sync_pool,
	.sync_tape                = st_db_postgresql_sync_tape,
	.sync_user                = st_db_postgresql_sync_user,
	.update_job               = st_db_postgresql_update_job,

	.file_add_checksum   = st_db_postgresql_file_add_checksum,
	.file_link_to_volume = st_db_postgresql_file_link_to_volume,
	.new_archive         = st_db_postgresql_new_archive,
	.new_file            = st_db_postgresql_new_file,
	.new_volume          = st_db_postgresql_new_volume,
	.update_archive      = st_db_postgresql_update_archive,
	.update_volume       = st_db_postgresql_update_volume,
};


int st_db_postgresql_cancel_checkpoint(struct st_database_connection * connection, const char * checkpoint) {
	if (!connection || !checkpoint)
		return -1;

	st_log_write_all(st_log_level_debug, st_log_type_plugin_db, "Rollback a transaction from savepoint '%s' (cid #%ld)", checkpoint, connection->id);

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	char * query = 0;
	asprintf(&query, "ROLLBACK TO %s", checkpoint);

	PGresult * result = PQexec(self->db_con, query);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);

	if (status != PGRES_COMMAND_OK)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql (cid #%ld): error while rollbacking a savepoint => %s", connection->id, PQerrorMessage(self->db_con));

	PQclear(result);
	free(query);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

int st_db_postgresql_cancel_transaction(struct st_database_connection * connection) {
	if (!connection)
		return -1;

	st_log_write_all(st_log_level_debug, st_log_type_plugin_db, "Rollback a transaction (cid #%ld)", connection->id);

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	PGresult * result = PQexec(self->db_con, "ROLLBACK");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql (cid #%ld): error while cancelling a transaction => %s", connection->id, PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

int st_db_postgresql_create_checkpoint(struct st_database_connection * connection, const char * checkpoint) {
	if (!connection || !checkpoint)
		return -1;

	st_log_write_all(st_log_level_debug, st_log_type_plugin_db, "Create savepoint '%s' (cid #%ld)", checkpoint, connection->id);

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	char * query = 0;
	asprintf(&query, "SAVEPOINT %s", checkpoint);

	PGresult * result = PQexec(self->db_con, query);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);

	if (status != PGRES_COMMAND_OK)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql (cid #%ld): error while creating a savepoint => %s", connection->id, PQerrorMessage(self->db_con));

	PQclear(result);
	free(query);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

int st_db_postgresql_finish_transaction(struct st_database_connection * connection) {
	if (!connection)
		return -1;

	st_log_write_all(st_log_level_debug, st_log_type_plugin_db, "Commit a transaction (cid #%ld)", connection->id);

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	PGresult * result = PQexec(self->db_con, "COMMIT");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql (cid #%ld): error while committing a transaction (%s)", connection->id, PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

int st_db_postgresql_start_transaction(struct st_database_connection * connection) {
	if (!connection)
		return -1;

	st_log_write_all(st_log_level_debug, st_log_type_plugin_db, "Begin a transaction (cid #%ld)", connection->id);

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	PGresult * result = PQexec(self->db_con, "BEGIN");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql (cid #%ld): error while starting a transaction (%s)", connection->id, PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}


int st_db_postgresql_add_job_record(struct st_database_connection * connection, struct st_job * job, const char * message) {
	if (!connection || !job || !message)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "insert_job_record", "INSERT INTO jobrecord VALUES (DEFAULT, $1, $2, $3, NOW(), $4)");

	char * jobid = 0, * numrun = 0;
	asprintf(&jobid, "%ld", job->id);
	asprintf(&numrun, "%ld", job->num_runs);

	const char * param[] = { jobid, st_job_status_to_string(job->sched_status), numrun, message };
	PGresult * result = PQexecPrepared(self->db_con, "insert_job_record", 4, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);

	PQclear(result);
	free(numrun);
	free(jobid);

	return status != PGRES_COMMAND_OK;
}

// need to be checked when there is a transaction
// and prepared statements
void st_db_postgresql_check(struct st_database_connection * connection) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	if (PQstatus(self->db_con) != CONNECTION_OK) {
		int i;
		for (i = 0; i < 3; i++) {
			st_log_write_all(st_log_level_warning, st_log_type_plugin_db, "Postgresql (cid #%ld): Try to reset database connection", connection->id);

			PQreset(self->db_con);
			st_hashtable_clear(self->cached);

			if (PQstatus(self->db_con) != CONNECTION_OK) {
				st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql (cid #%ld): Failed to reset database connection", connection->id);
				sleep(30);
			} else
				break;
		}
	}
}

int st_db_postgresql_con_close(struct st_database_connection * connection) {
	if (!connection)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;

	if (!self->db_con)
		return 0;

	PQfinish(self->db_con);
	st_hashtable_free(self->cached);

	self->db_con = 0;

	return 0;
}

int st_db_postgresql_con_free(struct st_database_connection * connection) {
	if (!connection)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;

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

int st_db_postgresql_create_pool(struct st_database_connection * connection, struct st_pool * pool) {
	if (!connection || !pool)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "insert_pool", "INSERT INTO pool VALUES (DEFAULT, $1, $2, DEFAULT, $3, '{}') RETURNING *");

	char * tape_format = 0;
	asprintf(&tape_format, "%ld", pool->format->id);

	const char * param[] = { pool->uuid, pool->name, tape_format, };
	PGresult * result = PQexecPrepared(self->db_con, "insert_pool", 3, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &pool->id);
		st_db_postgresql_get_uchar(result, 0, 3, &pool->growable);
	}

	PQclear(result);
	free(tape_format);

	return status == PGRES_FATAL_ERROR;
}

char * st_db_postgresql_get_host(struct st_database_connection * connection) {
	struct st_db_postgresql_connetion_private * self = connection->data;

	st_db_postgresql_prepare(self, "select_host_by_name", "SELECT id FROM host WHERE name = $1 OR name || '.' || domaine = $1 LIMIT 1");

	struct utsname name;
	uname(&name);

	const char * param[] = { name.nodename };
	PGresult * result = PQexecPrepared(self->db_con, "select_host_by_name", 1, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);

	char * hostid = 0;

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_string_dup(result, 0, 0, &hostid);
	else
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql (cid #%ld): Host not found into database (%s)", connection->id, name.nodename);

	PQclear(result);

	return hostid;
}

int st_db_postgresql_get_nb_new_jobs(struct st_database_connection * connection, long * nb_new_jobs, time_t since, long last_max_jobs) {
	if (!connection || !nb_new_jobs)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	char * hostid = st_db_postgresql_get_host(connection);
	if (!hostid)
		return 1;

	st_db_postgresql_prepare(self, "select_nb_new_jobs1", "SELECT id FROM job WHERE id > $1 AND update < $2 AND host = $3 FOR SHARE");
	st_db_postgresql_prepare(self, "select_nb_new_jobs2", "SELECT COUNT(*) AS total FROM job WHERE id > $1 AND update < $2 AND host = $3");

	char csince[24], * lastmaxjobs = 0;
	struct tm tm_since;
	localtime_r(&since, &tm_since);
	strftime(csince, 23, "%F %T", &tm_since);
	asprintf(&lastmaxjobs, "%ld", last_max_jobs);

	const char * param[] = { lastmaxjobs, csince, hostid };
	PGresult * result = PQexecPrepared(self->db_con, "select_nb_new_jobs1", 3, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);

	PQclear(result);

	if (status == PGRES_FATAL_ERROR) {
		free(lastmaxjobs);
		free(hostid);
		return 1;
	}

	result = PQexecPrepared(self->db_con, "select_nb_new_jobs2", 3, param, 0, 0, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_long(result, 0, 0, nb_new_jobs);

	PQclear(result);
	free(lastmaxjobs);
	free(hostid);

	return status != PGRES_TUPLES_OK;
}

int st_db_postgresql_get_new_jobs(struct st_database_connection * connection, struct st_job ** jobs, unsigned int nb_jobs, time_t since, long last_max_jobs) {
	if (!connection || !jobs)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	char * hostid = st_db_postgresql_get_host(connection);
	if (!hostid)
		return 1;

	st_db_postgresql_prepare(self, "select_new_jobs", "SELECT j.*, jt.name FROM job j, jobtype jt WHERE j.id > $1 AND j.update < $2 AND j.host = $3 AND j.type = jt.id ORDER BY nextstart LIMIT $4");
	st_db_postgresql_prepare(self, "select_num_runs", "SELECT MAX(numrun) AS max FROM jobrecord WHERE job = $1");
	st_db_postgresql_prepare(self, "select_paths", "SELECT path FROM selectedfile WHERE id IN (SELECT selectedfile FROM jobtoselectedfile WHERE job = $1)");
	st_db_postgresql_prepare(self, "select_checksums", "SELECT * FROM checksum WHERE id IN (SELECT checksum FROM jobtochecksum WHERE job = $1)");
	st_db_postgresql_prepare(self, "select_archive", "SELECT * FROM tape WHERE id IN (SELECT tape FROM archivevolume WHERE archive = $1)");
	st_db_postgresql_prepare(self, "select_job_tape", "SELECT v.sequence, t.uuid, v.tapeposition, v.size FROM archivevolume v, tape t WHERE v.tape = t.id AND v.archive = $1 ORDER BY v.sequence");
	st_db_postgresql_prepare(self, "select_restore", "SELECT * FROM restoreto WHERE job = $1 LIMIT 1");

	char csince[24], * lastmaxjobs = 0, * nbjobs = 0;
	struct tm tm_since;
	localtime_r(&since, &tm_since);
	strftime(csince, 23, "%F %T", &tm_since);
	asprintf(&lastmaxjobs, "%ld", last_max_jobs);
	asprintf(&nbjobs, "%u", nb_jobs);

	const char * param[] = { lastmaxjobs, csince, hostid, nbjobs };
	PGresult * result = PQexecPrepared(self->db_con, "select_new_jobs", 4, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result);

		PQclear(result);
		free(nbjobs);
		free(lastmaxjobs);
		free(hostid);
		return 1;
	}

	int i, nb_tuples = PQntuples(result);
	for (i = 0; i < nb_tuples; i++) {
		char * archiveid = 0, * jobid = 0, * poolid = 0;
		long userid;
		long tapeid = -1;

		jobs[i] = malloc(sizeof(struct st_job));
		bzero(jobs[i], sizeof(struct st_job));

		st_db_postgresql_get_long(result, i, 0, &jobs[i]->id);
		jobid = PQgetvalue(result, i, 0);
		st_db_postgresql_get_string_dup(result, i, 1, &jobs[i]->name);
		jobs[i]->db_status = st_job_string_to_status(PQgetvalue(result, i, 7));
		st_db_postgresql_get_time(result, i, 8, &jobs[i]->updated);

		st_db_postgresql_get_time(result, i, 3, &jobs[i]->start);
		jobs[i]->interval = 0;
		st_db_postgresql_get_long(result, i, 4, &jobs[i]->interval);
		st_db_postgresql_get_long(result, i, 5, &jobs[i]->repetition);
		if (!PQgetisnull(result, i, 9))
			archiveid = PQgetvalue(result, i, 9);
		st_db_postgresql_get_long(result, i, 11, &userid);
		if (!PQgetisnull(result, i, 12))
			poolid = PQgetvalue(result, i, 12);
		jobs[i]->num_runs = 0;
		if (!PQgetisnull(result, i, 13))
			st_db_postgresql_get_long(result, i, 13, &tapeid);

		const char * param2[] = { jobid };
		PGresult * result2 = PQexecPrepared(self->db_con, "select_num_runs", 1, param2, 0, 0, 0);
		ExecStatusType status2 = PQresultStatus(result2);

		if (status2 == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result2);
		else if (status == PGRES_TUPLES_OK && PQntuples(result2) == 1 && !PQgetisnull(result2, 0, 0))
			st_db_postgresql_get_long(result2, 0, 0, &jobs[i]->num_runs);
		PQclear(result2);

		jobs[i]->user = st_user_get(userid, 0);

		if (poolid) {
			st_db_postgresql_prepare(self, "select_pool_by_id", "SELECT uuid, tapeformat FROM pool WHERE id = $1 LIMIT 1");
			st_db_postgresql_prepare(self, "select_tapeformat_by_id", "SELECT densitycode FROM tapeformat WHERE id = $1 LIMIT 1");

			char * pool_uuid = 0, * tapeformat = 0;
			unsigned char densitycode;

			const char * param3[] = { poolid };
			result2 = PQexecPrepared(self->db_con, "select_pool_by_id", 1, param3, 0, 0, 0);
			ExecStatusType status3 = PQresultStatus(result2);

			if (status3 == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result2);
			else if (status3 == PGRES_TUPLES_OK && PQntuples(result2) == 1) {
				st_db_postgresql_get_string_dup(result2, 0, 0, &pool_uuid);
				st_db_postgresql_get_string_dup(result2, 0, 1, &tapeformat);
			}
			PQclear(result2);

			const char * param4[] = { tapeformat };
			result2 = PQexecPrepared(self->db_con, "select_tapeformat_by_id", 1, param4, 0, 0, 0);
			status3 = PQresultStatus(result2);

			if (status3 == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result2);
			else if (status3 == PGRES_TUPLES_OK && PQntuples(result2) == 1) {
				st_db_postgresql_get_uchar(result2, 0, 0, &densitycode);

				jobs[i]->pool = st_pool_get_by_uuid(pool_uuid);
			}
			PQclear(result2);

			free(tapeformat);
			free(pool_uuid);
		}

		result2 = PQexecPrepared(self->db_con, "select_paths", 1, param2, 0, 0, 0);
		status2 = PQresultStatus(result2);

		if (status2 == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result2);
		else if (status2 == PGRES_TUPLES_OK) {
			jobs[i]->nb_paths = PQntuples(result2);
			jobs[i]->paths = 0;

			if (jobs[i]->nb_paths > 0) {
				jobs[i]->paths = calloc(jobs[i]->nb_paths, sizeof(char *));

				unsigned int j;
				for (j = 0; j < jobs[i]->nb_paths; j++)
					st_db_postgresql_get_string_dup(result2, j, 0, jobs[i]->paths + j);
			}
		}
		PQclear(result2);

		result2 = PQexecPrepared(self->db_con, "select_checksums", 1, param2, 0, 0, 0);
		status2 = PQresultStatus(result2);

		if (status2 == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result2);
		else if (PQresultStatus(result2) == PGRES_TUPLES_OK) {
			jobs[i]->nb_checksums = PQntuples(result2);
			jobs[i]->checksums = 0;

			if (jobs[i]->nb_checksums > 0) {
				jobs[i]->checksum_ids = calloc(jobs[i]->nb_checksums, sizeof(long));
				jobs[i]->checksums = calloc(jobs[i]->nb_checksums, sizeof(char *));

				unsigned int j;
				for (j = 0; j < jobs[i]->nb_checksums; j++) {
					st_db_postgresql_get_long(result2, j, 0, jobs[i]->checksum_ids + j);
					st_db_postgresql_get_string_dup(result2, j, 1, jobs[i]->checksums + j);
				}
			}
		}
		PQclear(result2);

		if (archiveid) {
			const char * param3[] = { archiveid };
			result2 = PQexecPrepared(self->db_con, "select_job_tape", 1, param3, 0, 0, 0);
			status2 = PQresultStatus(result2);

			if (status2 == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result2);
			else if (status2 == PGRES_TUPLES_OK) {
				int j, nb_tuples2 = PQntuples(result2);
				jobs[i]->tapes = calloc(nb_tuples2, sizeof(struct st_job_tape));
				jobs[i]->nb_tapes = nb_tuples2;

				for (j = 0; j < nb_tuples2; j++) {
					struct st_job_tape * tape = jobs[i]->tapes + j;
					st_db_postgresql_get_long(result2, j, 0, &tape->sequence);
					tape->tape = st_tape_get_by_uuid(PQgetvalue(result2, j, 1));
					st_db_postgresql_get_long(result2, j, 2, &tape->tape_position);
					st_db_postgresql_get_ssize(result2, j, 3, &tape->size);
				}
			}
			PQclear(result2);
		}

		if (tapeid > -1)
			jobs[i]->tape = st_tape_get_by_id(tapeid);

		result2 = PQexecPrepared(self->db_con, "select_restore", 1, param2, 0, 0, 0);
		status2 = PQresultStatus(result2);
		if (status2 == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result2);
		else if (PQresultStatus(result2) == PGRES_TUPLES_OK && PQntuples(result2) == 1) {
			struct st_job_restore_to * rt = jobs[i]->restore_to = malloc(sizeof(struct st_job_restore_to));
			bzero(jobs[i]->restore_to, sizeof(struct st_job_restore_to));

			st_db_postgresql_get_string_dup(result2, 0, 1, &rt->path);
			st_db_postgresql_get_int(result2, 0, 2, &rt->nb_trunc_path);
		}
		PQclear(result2);

		jobs[i]->driver = st_job_get_driver(PQgetvalue(result, i, 15));
	}

	PQclear(result);
	free(nbjobs);
	free(lastmaxjobs);
	free(hostid);

	return 0;
}

int st_db_postgresql_get_pool(struct st_database_connection * connection, struct st_pool * pool, long id, const char * uuid) {
	if (!connection || !pool)
		return 1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	char * poolid = 0;

	PGresult * result = 0;
	if (id > -1) {
		st_db_postgresql_prepare(self, "select_pool_by_id", "SELECT * FROM pool WHERE id = $1 LIMIT 1");

		asprintf(&poolid, "%ld", id);

		const char * param[] = { poolid };
		result = PQexecPrepared(self->db_con, "select_pool_by_id", 1, param, 0, 0, 0);
	} else {
		st_db_postgresql_prepare(self, "select_pool_by_uuid", "SELECT * FROM pool WHERE uuid = $1 LIMIT 1");

		const char * param[] = { uuid };
		result = PQexecPrepared(self->db_con, "select_pool_by_uuid", 1, param, 0, 0, 0);
	}
	ExecStatusType status = PQresultStatus(result);

	if (poolid)
		free(poolid);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &pool->id);
		st_db_postgresql_get_string(result, 0, 1, pool->uuid);
		st_db_postgresql_get_string(result, 0, 2, pool->name);
		st_db_postgresql_get_uchar(result, 0, 3, &pool->growable);

		long tapeformatid;
		st_db_postgresql_get_long(result, 0, 4, &tapeformatid);
		pool->format = st_tape_format_get_by_id(tapeformatid);
	} else {
		pool->id = -1;
		pool->format = 0;
	}

	PQclear(result);

	return status != PGRES_TUPLES_OK || pool->id == -1;
}

int st_db_postgresql_get_tape(struct st_database_connection * connection, struct st_tape * tape, long id, const char * uuid) {
	if (!connection || !tape)
		return 1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	PGresult * result = 0;
	if (uuid) {
		st_db_postgresql_prepare(self, "select_tape_by_uuid", "SELECT * FROM tape WHERE uuid = $1 LIMIT 1");

		const char * param[] = { uuid };
		result = PQexecPrepared(self->db_con, "select_tape_by_uuid", 1, param, 0, 0, 0);
	} else {
		st_db_postgresql_prepare(self, "select_tape_by_id", "SELECT * FROM tape WHERE id = $1 LIMIT 1");

		char * tapeid = 0;
		asprintf(&tapeid, "%ld", id);

		const char * param[] = { tapeid };
		result = PQexecPrepared(self->db_con, "select_tape_by_id", 1, param, 0, 0, 0);

		free(tapeid);
	}

	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &tape->id);
		st_db_postgresql_get_string(result, 0, 2, tape->label);
		st_db_postgresql_get_string(result, 0, 3, tape->medium_serial_number);
		st_db_postgresql_get_string(result, 0, 4, tape->name);
		tape->status = st_tape_string_to_status(PQgetvalue(result, 0, 5));
		tape->location = st_tape_string_to_location(PQgetvalue(result, 0, 6));
		st_db_postgresql_get_time(result, 0, 7, &tape->first_used);
		st_db_postgresql_get_time(result, 0, 8, &tape->use_before);
		st_db_postgresql_get_long(result, 0, 9, &tape->load_count);
		st_db_postgresql_get_long(result, 0, 10, &tape->read_count);
		st_db_postgresql_get_long(result, 0, 11, &tape->write_count);
		st_db_postgresql_get_ssize(result, 0, 12, &tape->end_position);
		st_db_postgresql_get_int(result, 0, 13, &tape->nb_files);
		st_db_postgresql_get_ssize(result, 0, 14, &tape->block_size);
		st_db_postgresql_get_bool(result, 0, 15, &tape->has_partition);

		long tapeformatid = -1, poolid = -1;
		st_db_postgresql_get_long(result, 0, 16, &tapeformatid);
		st_db_postgresql_get_long(result, 0, 17, &poolid);

		if (tapeformatid > -1)
			tape->format = st_tape_format_get_by_id(tapeformatid);
		else {
			st_log_write_all(st_log_level_error, st_log_type_plugin_db, "There is a tape into databse without tapeformat");
			return 1;
		}

		if (poolid > -1)
			tape->pool = st_pool_get_by_id(poolid);
	}

	PQclear(result);

	return status != PGRES_TUPLES_OK;
}

int st_db_postgresql_get_tape_format(struct st_database_connection * connection, struct st_tape_format * tape_format, long id, unsigned char density_code) {
	if (!connection || !tape_format)
		return 1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	char * densitycode = 0, * tapeformatid = 0;
	PGresult * result = 0;

	if (id < -1) {
		st_db_postgresql_prepare(self, "select_tapeformat_by_id", "SELECT * FROM tapeformat WHERE id = $1 LIMIT 1");

		asprintf(&tapeformatid, "%ld", id);

		const char * param[] = { tapeformatid };
		result = PQexecPrepared(self->db_con, "select_tapeformat_by_id", 1, param, 0, 0, 0);
	} else {
		st_db_postgresql_prepare(self, "select_tapeformat_by_densitycode", "SELECT * FROM tapeformat WHERE densityCode = $1 LIMIT 1");

		asprintf(&densitycode, "%d", density_code);

		const char * param[] = { densitycode };
		result = PQexecPrepared(self->db_con, "select_tapeformat_by_densitycode", 1, param, 0, 0, 0);
	}
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &tape_format->id);
		st_db_postgresql_get_string(result, 0, 1, tape_format->name);
		tape_format->type = st_tape_string_to_format_data(PQgetvalue(result, 0, 2));
		tape_format->mode = st_tape_string_to_format_mode(PQgetvalue(result, 0, 3));
		st_db_postgresql_get_long(result, 0, 4, &tape_format->max_load_count);
		st_db_postgresql_get_long(result, 0, 5, &tape_format->max_read_count);
		st_db_postgresql_get_long(result, 0, 6, &tape_format->max_write_count);
		st_db_postgresql_get_long(result, 0, 7, &tape_format->max_op_count);
		st_db_postgresql_get_long(result, 0, 8, &tape_format->life_span);
		st_db_postgresql_get_ssize(result, 0, 9, &tape_format->capacity);
		st_db_postgresql_get_ssize(result, 0, 10, &tape_format->block_size);
		if (density_code)
			tape_format->density_code = density_code;
		else
			st_db_postgresql_get_uchar(result, 0, 11, &tape_format->density_code);
		st_db_postgresql_get_bool(result, 0, 12, &tape_format->support_partition);
		st_db_postgresql_get_bool(result, 0, 13, &tape_format->support_mam);
	} else
		tape_format->id = -1;

	PQclear(result);

	if (densitycode)
		free(densitycode);
	if (tapeformatid)
		free(tapeformatid);

	return status != PGRES_TUPLES_OK || tape_format->id == -1;
}

int st_db_postgresql_get_user(struct st_database_connection * connection, struct st_user * user, long user_id, const char * login) {
	if (!connection || (user_id < 0 && !login))
		return 1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_user_by_id_or_login", "SELECT * FROM users WHERE id = $1 OR login = $2 LIMIT 1");

	char * userid = 0;
	asprintf(&userid, "%ld", user_id);

	const char * param[] = { userid, login };
	PGresult * result = PQexecPrepared(self->db_con, "select_user_by_id_or_login", 2, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &user->id);
		st_db_postgresql_get_string_dup(result, 0, 1, &user->login);
		st_db_postgresql_get_string_dup(result, 0, 2, &user->password);
		st_db_postgresql_get_string_dup(result, 0, 3, &user->salt);
		st_db_postgresql_get_string_dup(result, 0, 4, &user->fullname);
		st_db_postgresql_get_string_dup(result, 0, 5, &user->email);
		st_db_postgresql_get_bool(result, 0, 6, &user->is_admin);
		st_db_postgresql_get_bool(result, 0, 7, &user->can_archive);
		st_db_postgresql_get_bool(result, 0, 8, &user->can_restore);
		st_db_postgresql_get_int(result, 0, 9, &user->nb_connection);
		st_db_postgresql_get_time(result, 0, 10, &user->last_connection);
		st_db_postgresql_get_bool(result, 0, 11, &user->disabled);

		long poolid = -1;
		st_db_postgresql_get_long(result, 0, 12, &poolid);
		if (poolid > -1)
			user->pool = st_pool_get_by_id(poolid);
	}

	PQclear(result);
	free(userid);

	return status != PGRES_TUPLES_OK || PQntuples(result) < 1;
}

int st_db_postgresql_init_connection(struct st_database_connection * connection, struct st_db_postgresql_private * driver_private) {
	if (!connection || !connection->driver)
		return -1;

	PGconn * con = PQsetdbLogin(driver_private->host, driver_private->port, 0, 0, driver_private->db, driver_private->user, driver_private->password);
	ConnStatusType status = PQstatus(con);

	if (status == CONNECTION_BAD) {
		PQfinish(con);
		return -2;
	}

	connection->id = PQbackendPID(con);
	connection->ops = &st_db_postgresql_con_ops;

	struct st_db_postgresql_connetion_private * self = malloc(sizeof(struct st_db_postgresql_connetion_private));
	self->db_con = con;
	self->cached = st_hashtable_new2(st_util_compute_hash_string, st_util_basic_free);

	connection->data = self;

	return 0;
}

int st_db_postgresql_is_changer_contain_drive(struct st_database_connection * connection, struct st_changer * changer, struct st_drive * drive) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_changer_of_drive_by_model_vendor_serialnumber", "SELECT changer FROM drive WHERE model = $1 AND vendor = $2 AND serialnumber = $3 LIMIT 1");

	char * changerid = 0;

	const char * param[] = { drive->model, drive->vendor, drive->serial_number };
	PGresult * result = PQexecPrepared(self->db_con, "select_changer_of_drive_by_model_vendor_serialnumber", 3, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result);

		PQclear(result);
		return 0;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_string_dup(result, 0, 0, &changerid);
		PQclear(result);
	} else {
		PQclear(result);
		return 0;
	}

	st_db_postgresql_prepare(self, "select_changer_by_model_vendor_serialnumber_id", "SELECT id FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 AND id = $4 LIMIT 1");

	const char * params2[] = { changer->model, changer->vendor, changer->serial_number, changerid };
	result = PQexecPrepared(self->db_con, "select_changer_by_model_vendor_serialnumber_id", 4, params2, 0, 0, 0);
	status = PQresultStatus(result);

	int ok = 0;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	else if (PQntuples(result) == 1)
		ok = 1;

	PQclear(result);
	free(changerid);

	return ok;
}

int st_db_postgresql_refresh_job(struct st_database_connection * connection, struct st_job * job) {
	if (!connection || !job)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_job_since_by_id", "SELECT * FROM job WHERE id = $1 AND update >= $2");

	char * jobid = 0;
	asprintf(&jobid, "%ld", job->id);

	char updated[24];
	struct tm tm_updated;
	localtime_r(&job->updated, &tm_updated);
	strftime(updated, 23, "%F %T", &tm_updated);

	st_db_postgresql_start_transaction(connection);

	const char * param[] = { jobid, updated };
	PGresult * result = PQexecPrepared(self->db_con, "select_job_since_by_id", 2, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		job->db_status = st_job_string_to_status(PQgetvalue(result, 0, 4));
		st_db_postgresql_get_time(result, 0, 12, &job->updated);
		st_db_postgresql_get_time(result, 0, 5, &job->start);

		PQclear(result);
		free(jobid);

		st_db_postgresql_cancel_transaction(connection);

		return 0;
	}
	PQclear(result);

	st_db_postgresql_prepare(self, "select_job_by_id", "SELECT * FROM job WHERE id = $1 LIMIT 1");
	result = PQexecPrepared(self->db_con, "select_job_by_id", 1, param, 0, 0, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) != 1)
		job->id = -1;

	PQclear(result);
	free(jobid);

	st_db_postgresql_cancel_transaction(connection);

	return 0;
}

int st_db_postgresql_sync_changer(struct st_database_connection * connection, struct st_changer * changer) {
	if (!connection || !changer)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	PGTransactionStatusType transStatus = PQtransactionStatus(self->db_con);
	if (transStatus == PQTRANS_INERROR)
		return 2;

	char * hostid = st_db_postgresql_get_host(connection);
	if (!hostid)
		return 1;

	if (transStatus == PQTRANS_IDLE)
		st_db_postgresql_start_transaction(connection);

	char * changerid = 0;
	if (changer->id < 0) {
		st_db_postgresql_prepare(self, "select_changer_by_model_vendor_serialnumber", "SELECT id FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 FOR UPDATE NOWAIT");

		const char * param2[] = { changer->model, changer->vendor, changer->serial_number };
		PGresult * result = PQexecPrepared(self->db_con, "select_changer_by_model_vendor_serialnumber", 3, param2, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_long(result, 0, 0, &changer->id);
			st_db_postgresql_get_string_dup(result, 0, 0, &changerid);
		}

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			free(hostid);
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connection);
			return 3;
		}
	} else {
		st_db_postgresql_prepare(self, "select_changer_by_id", "SELECT id FROM changer WHERE id = $1 FOR UPDATE NOWAIT");

		char * changerid = 0;
		asprintf(&changerid, "%ld", changer->id);

		const char * param2[] = { changerid };
		PGresult * result = PQexecPrepared(self->db_con, "select_changer_by_id", 1, param2, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);

		PQclear(result);
		free(changerid);

		if (status == PGRES_FATAL_ERROR) {
			free(hostid);
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connection);
			return 3;
		}
	}

	if (changer->id < 0) {
		st_db_postgresql_prepare(self, "insert_changer", "INSERT INTO changer VALUES (DEFAULT, $1, $2, $3, $4, $5, $6, $7, $8) RETURNING id");

		const char * param2[] = {
			changer->device, st_changer_status_to_string(changer->status), changer->barcode ? "true" : "false",
			changer->model, changer->vendor, changer->revision, changer->serial_number, hostid
		};

		PGresult * result = PQexecPrepared(self->db_con, "insert_changer", 8, param2, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR) {
			st_db_postgresql_get_error(result);
			free(result);
			free(hostid);
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connection);
			return -1;
		} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &changer->id);

		PQclear(result);
	} else {
		st_db_postgresql_prepare(self, "update_changer", "UPDATE changer SET device = $1, status = $2, firmwarerev = $3 WHERE id = $4");

		const char * params2[] = { changer->device, st_changer_status_to_string(changer->status), changer->revision, changerid };
		PGresult * result = PQexecPrepared(self->db_con, "update_changer", 4, params2, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR) {
			st_db_postgresql_get_error(result);

			free(changerid);
			free(hostid);
			PQclear(result);

			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connection);
			return -2;
		}

		PQclear(result);
	}

	free(changerid);
	free(hostid);

	st_db_postgresql_create_checkpoint(connection, "sync_changer_before_drive");

	unsigned int i;
	int status = 0;
	for (i = 0; i < changer->nb_drives && !status; i++)
		status = st_db_postgresql_sync_drive(connection, changer->drives + i);

	if (status)
		st_db_postgresql_cancel_checkpoint(connection, "sync_changer_before_drive");

	status = 0;
	st_db_postgresql_create_checkpoint(connection, "sync_changer_before_slot");

	for (i = changer->nb_drives; i < changer->nb_slots && !status; i++)
		status = st_db_postgresql_sync_slot(connection, changer->slots + i);

	if (status)
		st_db_postgresql_cancel_checkpoint(connection, "sync_changer_before_slot");

	if (transStatus == PQTRANS_IDLE)
		st_db_postgresql_finish_transaction(connection);

	return 0;
}

int st_db_postgresql_sync_drive(struct st_database_connection * connection, struct st_drive * drive) {
	if (!connection || !drive)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	PGTransactionStatusType transStatus = PQtransactionStatus(self->db_con);
	if (transStatus == PQTRANS_INERROR)
		return 2;
	else if (transStatus == PQTRANS_IDLE)
		st_db_postgresql_start_transaction(connection);

	if (drive->id < 0) {
		st_db_postgresql_prepare(self, "select_drive_by_model_vendor_serialnumber", "SELECT id, operationduration, lastclean, driveformat FROM drive WHERE model = $1 AND vendor = $2 AND serialnumber = $3 FOR UPDATE NOWAIT");

		const char * param[] = { drive->model, drive->vendor, drive->serial_number };
		PGresult * result = PQexecPrepared(self->db_con, "select_drive_by_model_vendor_serialnumber", 3, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_long(result, 0, 0, &drive->id);

			double old_operation_duration;
			st_db_postgresql_get_double(result, 0, 1, &old_operation_duration);
			drive->operation_duration += old_operation_duration;

			st_db_postgresql_get_time(result, 0, 2, &drive->last_clean);

			unsigned char densitycode;
			st_db_postgresql_get_uchar(result, 0, 3, &densitycode);

			if (densitycode > drive->best_density_code)
				drive->best_density_code = densitycode;
		}

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connection);
			return 3;
		}
	} else {
		st_db_postgresql_prepare(self, "select_drive_by_id", "SELECT id FROM drive WHERE id = $1 FOR UPDATE NOWAIT");

		char * driveid = 0;
		asprintf(&driveid, "%ld", drive->id);

		const char * param[] = { driveid };
		PGresult * result = PQexecPrepared(self->db_con, "select_drive_by_id", 1, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		PQclear(result);
		free(driveid);

		if (status == PGRES_FATAL_ERROR) {
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connection);
			return 3;
		}
	}

	char * changerid = 0;
	if (drive->changer)
		asprintf(&changerid, "%ld", drive->changer->id);

	if (drive->id < 0) {
		st_db_postgresql_prepare(self, "select_driveformat_by_densitycode", "SELECT id FROM driveformat WHERE densitycode = $1 LIMIT 1");
		st_db_postgresql_prepare(self, "insert_drive", "INSERT INTO drive VALUES (DEFAULT, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12) RETURNING id");

		char * densitycode = 0, * driveformat_id = 0;
		asprintf(&densitycode, "%u", drive->best_density_code);

		const char * param1[] = { densitycode };
		PGresult * result = PQexecPrepared(self->db_con, "select_driveformat_by_densitycode", 1, param1, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_string_dup(result, 0, 0, &driveformat_id);

		PQclear(result);

		char * changer_num = 0, * op_duration = 0;
		if (drive->changer)
			asprintf(&changer_num, "%td", drive - drive->changer->drives);
		else
			changer_num = strdup("0");
		asprintf(&op_duration, "%.3f", drive->operation_duration);
		char last_clean[24];
		struct tm tm_last_clean;
		localtime_r(&drive->last_clean, &tm_last_clean);
		strftime(last_clean, 23, "%F %T", &tm_last_clean);

		const char * param2[] = {
			drive->device, drive->scsi_device, st_drive_status_to_string(drive->status), changer_num, op_duration, last_clean,
			drive->model, drive->vendor, drive->revision, drive->serial_number, changerid, driveformat_id
		};
		result = PQexecPrepared(self->db_con, "insert_drive", 12, param2, 0, 0, 0);
		status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &drive->id);

		PQclear(result);
		free(op_duration);
		free(changer_num);
		if (driveformat_id)
			free(driveformat_id);
		free(densitycode);

		if (status == PGRES_FATAL_ERROR) {
			if (drive->changer)
				free(changerid);
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connection);
			return 3;
		}
	} else {
		st_db_postgresql_prepare(self, "select_driveformat_by_densitycode", "SELECT id FROM driveformat WHERE densitycode = $1 LIMIT 1");
		st_db_postgresql_prepare(self, "update_drive", "UPDATE drive SET device = $1, scsidevice = $2, status = $3, operationduration = $4, lastclean = $5, firmwarerev = $6, driveformat = $7 WHERE id = $8");

		char * densitycode = 0, * driveformat_id = 0;
		asprintf(&densitycode, "%u", drive->best_density_code);

		const char * param1[] = { densitycode };
		PGresult * result = PQexecPrepared(self->db_con, "select_driveformat_by_densitycode", 1, param1, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_string_dup(result, 0, 0, &driveformat_id);
		PQclear(result);

		char * driveid = 0, * op_duration = 0;
		asprintf(&driveid, "%ld", drive->id);
		asprintf(&op_duration, "%.3f", drive->operation_duration);

		char last_clean[24];
		struct tm tm_last_clean;
		localtime_r(&drive->last_clean, &tm_last_clean);
		strftime(last_clean, 23, "%F %T", &tm_last_clean);

		const char * param2[] = {
			drive->device, drive->scsi_device, st_drive_status_to_string(drive->status),
			op_duration, last_clean, drive->revision, driveformat_id, driveid
		};
		result = PQexecPrepared(self->db_con, "update_drive", 8, param2, 0, 0, 0);
		status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);

		PQclear(result);
		free(op_duration);
		free(driveid);
		if (driveformat_id)
			free(driveformat_id);
		free(densitycode);

		if (status == PGRES_FATAL_ERROR) {
			if (drive->changer)
				free(changerid);
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connection);
			return 3;
		}
	}

	if (drive->changer)
		free(changerid);

	st_db_postgresql_create_checkpoint(connection, "sync_drive_before_slot");

	if (drive->slot && st_db_postgresql_sync_slot(connection, drive->slot))
		st_db_postgresql_cancel_checkpoint(connection, "sync_drive_before_slot");

	if (transStatus == PQTRANS_IDLE)
		st_db_postgresql_finish_transaction(connection);

	return 0;
}

int st_db_postgresql_sync_plugin_checksum(struct st_database_connection * connection, const char * plugin) {
	if (!connection || !plugin)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_checksum_by_name", "SELECT name FROM checksum WHERE name = $1 LIMIT 1");

	const char * param[] = { plugin };
	PGresult * result = PQexecPrepared(self->db_con, "select_checksum_by_name", 1, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	int found = 0;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		found = 1;

	PQclear(result);

	if (found)
		return 0;

	st_db_postgresql_prepare(self, "insert_checksum", "INSERT INTO checksum VALUES (DEFAULT, $1)");

	result = PQexecPrepared(self->db_con, "insert_checksum", 1, param, 0, 0, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);

	PQclear(result);

	return status == PGRES_FATAL_ERROR;
}

int st_db_postgresql_sync_plugin_job(struct st_database_connection * connection, const char * plugin) {
	if (!connection || !plugin)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_jobtype_by_name", "SELECT name FROM jobtype WHERE name = $1 LIMIT 1");

	const char * param[] = { plugin };
	PGresult * result = PQexecPrepared(self->db_con, "select_jobtype_by_name", 1, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	int found = 0;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		found = 1;

	PQclear(result);

	if (found)
		return 0;

	st_db_postgresql_prepare(self, "insert_jobtype", "INSERT INTO jobtype VALUES (DEFAULT, $1)");

	result = PQexecPrepared(self->db_con, "insert_jobtype", 1, param, 0, 0, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);

	PQclear(result);

	return status == PGRES_FATAL_ERROR;
}

int st_db_postgresql_sync_pool(struct st_database_connection * connection, struct st_pool * pool) {
	if (!connection || pool->id < 0)
		return 1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_pool_by_id", "SELECT * FROM pool WHERE id = $1 LIMIT 1");

	char * poolid = 0;
	asprintf(&poolid, "%ld", pool->id);

	const char * param[] = { poolid };
	PGresult * result = PQexecPrepared(self->db_con, "select_pool_by_id", 1, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_string(result, 0, 2, pool->name);
		st_db_postgresql_get_uchar(result, 0, 3, &pool->growable);
	}

	PQclear(result);
	free(poolid);

	return status != PGRES_TUPLES_OK;
}

int st_db_postgresql_sync_slot(struct st_database_connection * connection, struct st_slot * slot) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	if (slot->tape && st_db_postgresql_sync_tape(connection, slot->tape))
		return 1;

	if (slot->id < 0) {
		st_db_postgresql_prepare(self, "select_slot_by_index_changer", "SELECT id FROM changerslot WHERE index = $1 AND changer = $2 FOR UPDATE NOWAIT");

		char * changer_id = 0, * slot_index = 0;
		asprintf(&changer_id, "%ld", slot->changer->id);
		asprintf(&slot_index, "%td", slot - slot->changer->slots);

		const char * param[] = { slot_index, changer_id };
		PGresult * result = PQexecPrepared(self->db_con, "select_slot_by_index_changer", 2, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &slot->id);

		PQclear(result);
		free(slot_index);
		free(changer_id);

		if (status == PGRES_FATAL_ERROR)
			return 2;
	} else {
		st_db_postgresql_prepare(self, "select_slot_by_id", "SELECT id FROM changerslot WHERE id = $1 FOR UPDATE NOWAIT");

		char * slot_id = 0;
		asprintf(&slot_id, "%ld", slot->id);

		const char * param[] = { slot_id };
		PGresult * result = PQexecPrepared(self->db_con, "select_slot_by_id", 1, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);

		PQclear(result);
		free(slot_id);

		if (status == PGRES_FATAL_ERROR)
			return 2;
	}

	if (slot->id > -1) {
		st_db_postgresql_prepare(self, "update_slot", "UPDATE changerslot SET tape = $1 WHERE id = $2");

		char * slot_id = 0, * tape_id = 0;
		asprintf(&slot_id, "%ld", slot->id);
		if (slot->tape && slot->tape->id > -1)
			asprintf(&tape_id, "%ld", slot->tape->id);

		const char * param[] = { tape_id, slot_id };
		PGresult * result = PQexecPrepared(self->db_con, "update_slot", 2, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);

		PQclear(result);
		free(slot_id);
		if (tape_id)
			free(tape_id);

		return status == PGRES_FATAL_ERROR;
	} else {
		st_db_postgresql_prepare(self, "insert_slot", "INSERT INTO changerslot VALUES (DEFAULT, $1, $2, $3, $4) RETURNING id");

		char * changer_id = 0, * slot_index = 0, * tape_id = 0;
		asprintf(&changer_id, "%ld", slot->changer->id);
		asprintf(&slot_index, "%td", slot - slot->changer->slots);
		if (slot->tape && slot->tape->id > -1)
			asprintf(&tape_id, "%ld", slot->tape->id);
		char * type = "default";
		if (slot->drive)
			type = "drive";
		else if (slot->is_import_export_slot)
			type = "import / export";

		const char * param[] = { slot_index, changer_id, tape_id, type };
		PGresult * result = PQexecPrepared(self->db_con, "insert_slot", 4, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &slot->id);

		PQclear(result);

		if (tape_id)
			free(tape_id);
		free(slot_index);
		free(changer_id);

		return status == PGRES_FATAL_ERROR;
	}
}

int st_db_postgresql_sync_tape(struct st_database_connection * connection, struct st_tape * tape) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	if (tape->id < 0) {
		if (tape->medium_serial_number[0] != '\0') {
			st_db_postgresql_prepare(self, "select_tape_by_medium_serial_number", "SELECT * FROM tape WHERE mediumserialnumber = $1 FOR UPDATE NOWAIT");

			const char * param[] = { tape->medium_serial_number };
			PGresult * result = PQexecPrepared(self->db_con, "select_tape_by_medium_serial_number", 1, param, 0, 0, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result);
			else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
				st_db_postgresql_get_long(result, 0, 0, &tape->id);
				st_db_postgresql_get_string(result, 0, 1, tape->uuid);
				st_db_postgresql_get_string(result, 0, 2, tape->label);
				st_db_postgresql_get_string(result, 0, 4, tape->name);
				st_db_postgresql_get_time(result, 0, 7, &tape->first_used);
				st_db_postgresql_get_time(result, 0, 8, &tape->use_before);

				long old_value = tape->load_count;
				if (!tape->mam_ok) {
					st_db_postgresql_get_long(result, 0, 9, &tape->load_count);
					tape->load_count += old_value;
				}

				old_value = tape->read_count;
				st_db_postgresql_get_long(result, 0, 10, &tape->read_count);
				tape->read_count += old_value;

				old_value = tape->write_count;
				st_db_postgresql_get_long(result, 0, 11, &tape->write_count);
				tape->write_count += old_value;

				if (tape->end_position == 0)
					st_db_postgresql_get_ssize(result, 0, 13, &tape->end_position);
				st_db_postgresql_get_ssize(result, 0, 14, &tape->block_size);
			}

			PQclear(result);
		}

		if (tape->uuid[0] != '\0') {
			st_db_postgresql_prepare(self, "select_tape_by_uuid", "SELECT * FROM tape WHERE uuid = $1 FOR UPDATE NOWAIT");

			const char * param[] = { tape->uuid };
			PGresult * result = PQexecPrepared(self->db_con, "select_tape_by_uuid", 1, param, 0, 0, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result);
			else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
				st_db_postgresql_get_long(result, 0, 0, &tape->id);
				st_db_postgresql_get_string(result, 0, 2, tape->label);
				st_db_postgresql_get_string(result, 0, 4, tape->name);
				st_db_postgresql_get_time(result, 0, 7, &tape->first_used);
				st_db_postgresql_get_time(result, 0, 8, &tape->use_before);

				long old_value = tape->load_count;
				if (!tape->mam_ok) {
					st_db_postgresql_get_long(result, 0, 9, &tape->load_count);
					tape->load_count += old_value;
				}

				old_value = tape->read_count;
				st_db_postgresql_get_long(result, 0, 10, &tape->read_count);
				tape->read_count += old_value;

				old_value = tape->write_count;
				st_db_postgresql_get_long(result, 0, 11, &tape->write_count);
				tape->write_count += old_value;

				if (tape->end_position == 0)
					st_db_postgresql_get_ssize(result, 0, 13, &tape->end_position);
				st_db_postgresql_get_ssize(result, 0, 14, &tape->block_size);
			}

			PQclear(result);
		}

		if (tape->id < 0 && tape->label[0] != '\0') {
			st_db_postgresql_prepare(self, "select_tape_by_label", "SELECT * FROM tape WHERE label = $1 FOR UPDATE NOWAIT");

			const char * param[] = { tape->label };
			PGresult * result = PQexecPrepared(self->db_con, "select_tape_by_label", 1, param, 0, 0, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result);
			else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
				st_db_postgresql_get_long(result, 0, 0, &tape->id);
				st_db_postgresql_get_string(result, 0, 2, tape->label);
				st_db_postgresql_get_string(result, 0, 4, tape->name);
				st_db_postgresql_get_time(result, 0, 7, &tape->first_used);
				st_db_postgresql_get_time(result, 0, 8, &tape->use_before);

				long old_value = tape->load_count;
				if (!tape->mam_ok) {
					st_db_postgresql_get_long(result, 0, 9, &tape->load_count);
					tape->load_count += old_value;
				}

				old_value = tape->read_count;
				st_db_postgresql_get_long(result, 0, 10, &tape->read_count);
				tape->read_count += old_value;

				old_value = tape->write_count;
				st_db_postgresql_get_long(result, 0, 11, &tape->write_count);
				tape->write_count += old_value;

				if (tape->end_position == 0)
					st_db_postgresql_get_ssize(result, 0, 13, &tape->end_position);
				st_db_postgresql_get_ssize(result, 0, 14, &tape->block_size);
			}

			PQclear(result);
		}
	} else {
		st_db_postgresql_prepare(self, "select_tape_by_id", "SELECT id FROM tape WHERE id = $1 FOR UPDATE NOWAIT");

		char * tape_id = 0;
		asprintf(&tape_id, "%ld", tape->id);

		const char * param[] = { tape_id };
		PGresult * result = PQexecPrepared(self->db_con, "select_tape_by_id", 1, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);

		PQclear(result);
		free(tape_id);

		if (status == PGRES_FATAL_ERROR)
			return 2;
	}

	if (tape->medium_serial_number[0] != '\0' || tape->uuid[0] != '\0' || tape->label[0] != '\0') {
		if (tape->id > -1) {
			st_db_postgresql_prepare(self, "update_tape", "UPDATE tape SET uuid = $1, name = $2, status = $3, location = $4, loadcount = $5, readcount = $6, writecount = $7, endpos = $8, nbfiles = $9, blocksize = $10, haspartition = $11, pool = $12 WHERE id = $13");

			char * load, * read, * write, * endpos, * nbfiles, * blocksize, * pool = 0, * id;
			asprintf(&load, "%ld", tape->load_count);
			asprintf(&read, "%ld", tape->read_count);
			asprintf(&write, "%ld", tape->write_count);
			asprintf(&endpos, "%zd", tape->end_position);
			asprintf(&nbfiles, "%u", tape->nb_files);
			asprintf(&blocksize, "%zd", tape->block_size);
			if (tape->pool)
				asprintf(&pool, "%ld", tape->pool->id);
			asprintf(&id, "%ld", tape->id);

			const char * param[] = {
				*tape->uuid ? tape->uuid : 0, tape->name, st_tape_status_to_string(tape->status),
				st_tape_location_to_string(tape->location),
				load, read, write, endpos, nbfiles, blocksize,
				tape->has_partition ? "true" : "false", pool, id
			};
			PGresult * result = PQexecPrepared(self->db_con, "update_tape", 13, param, 0, 0, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result);

			PQclear(result);
			free(load);
			free(read);
			free(write);
			free(endpos);
			free(nbfiles);
			free(blocksize);
			free(id);
			if (pool)
				free(pool);

			return status == PGRES_FATAL_ERROR;
		} else {
			st_db_postgresql_prepare(self, "insert_tape", "INSERT INTO tape VALUES (DEFAULT, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17) RETURNING id");

			char buffer_first_used[32];
			char buffer_use_before[32];

			struct tm tv;
			localtime_r(&tape->first_used, &tv);
			strftime(buffer_first_used, 32, "%F %T", &tv);

			localtime_r(&tape->use_before, &tv);
			strftime(buffer_use_before, 32, "%F %T", &tv);

			char * load, * read, * write, * endpos, * nbfiles, * blocksize, * tapeformat, * pool = 0;
			asprintf(&load, "%ld", tape->load_count);
			asprintf(&read, "%ld", tape->read_count);
			asprintf(&write, "%ld", tape->write_count);
			asprintf(&endpos, "%zd", tape->end_position);
			asprintf(&nbfiles, "%u", tape->nb_files);
			asprintf(&blocksize, "%zd", tape->block_size);
			asprintf(&tapeformat, "%ld", tape->format->id);
			if (tape->pool)
				asprintf(&pool, "%ld", tape->pool->id);

			const char * param[] = {
				*tape->uuid ? tape->uuid : 0,
				*tape->label ? tape->label : 0,
				*tape->medium_serial_number ? tape->medium_serial_number : 0,
				tape->name, st_tape_status_to_string(tape->status),
				st_tape_location_to_string(tape->location),
				buffer_first_used, buffer_use_before,
				load, read, write, endpos, nbfiles, blocksize,
				tape->has_partition ? "true" : "false", tapeformat, pool
			};
			PGresult * result = PQexecPrepared(self->db_con, "insert_tape", 17, param, 0, 0, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result);
			else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
				st_db_postgresql_get_long(result, 0, 0, &tape->id);

			PQclear(result);
			free(load);
			free(read);
			free(write);
			free(endpos);
			free(nbfiles);
			free(blocksize);
			free(tapeformat);
			if (pool)
				free(pool);

			return status == PGRES_FATAL_ERROR;
		}
	}

	return 0;
}

int st_db_postgresql_sync_user(struct st_database_connection * connection, struct st_user * user) {
	if (!connection || !user || user->id < 0)
		return 1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_user_by_id", "SELECT * FROM users WHERE id = $1 LIMIT 1");

	char * userid = 0;
	asprintf(&userid, "%ld", user->id);

	const char * param[] = { userid };
	PGresult * result = PQexecPrepared(self->db_con, "select_user_by_id", 1, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		if (user->password)
			free(user->password);
		st_db_postgresql_get_string_dup(result, 0, 2, &user->password);

		if (user->salt)
			free(user->salt);
		st_db_postgresql_get_string_dup(result, 0, 3, &user->salt);

		if (user->fullname)
			free(user->fullname);
		st_db_postgresql_get_string_dup(result, 0, 4, &user->fullname);

		if (user->email)
			free(user->email);
		st_db_postgresql_get_string_dup(result, 0, 5, &user->email);

		st_db_postgresql_get_bool(result, 0, 6, &user->is_admin);
		st_db_postgresql_get_bool(result, 0, 7, &user->can_archive);
		st_db_postgresql_get_bool(result, 0, 8, &user->can_restore);

		st_db_postgresql_get_int(result, 0, 9, &user->nb_connection);
		st_db_postgresql_get_time(result, 0, 10, &user->last_connection);
		st_db_postgresql_get_bool(result, 0, 11, &user->disabled);

		long poolid = -1;
		st_db_postgresql_get_long(result, 0, 12, &poolid);
		if (poolid > -1 && (!user->pool || poolid != user->pool->id))
			user->pool = st_pool_get_by_id(poolid);
	}

	PQclear(result);
	free(userid);

	return status == PGRES_TUPLES_OK;
}

int st_db_postgresql_update_job(struct st_database_connection * connection, struct st_job * job) {
	if (!connection || !job)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "update_job", "UPDATE job SET done = $1, status = $2, repetition = $3, update = NOW() WHERE id = $4");

	char * cdone = 0, * crepetition = 0, * jobid = 0;
	asprintf(&cdone, "%f", job->done);
	asprintf(&crepetition, "%ld", job->repetition);
	asprintf(&jobid, "%ld", job->id);

	const char * param[] = { cdone, st_job_status_to_string(job->sched_status), crepetition, jobid };
	PGresult * result = PQexecPrepared(self->db_con, "update_job", 4, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);

	PQclear(result);
	free(jobid);
	free(crepetition);
	free(cdone);

	return status != PGRES_COMMAND_OK;
}


int st_db_postgresql_file_add_checksum(struct st_database_connection * connection, struct st_archive_file * file) {
	if (!connection || !file)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_checksumresult", "SELECT id FROM checksumresult WHERE checksum = $1 AND result = $2 LIMIT 1");
	st_db_postgresql_prepare(self, "insert_checksumresult", "INSERT INTO checksumresult VALUES (DEFAULT, $1, $2) RETURNING id");
	st_db_postgresql_prepare(self, "insert_archive_file_to_checksum", "INSERT INTO archivefiletochecksumresult VALUES ($1, $2)");

	unsigned int i;
	for (i = 0; i < file->nb_checksums; i++) {
		char * checksumid = 0, * checksumresultid = 0, * fileid = 0;
		asprintf(&checksumid, "%ld", file->archive->job->checksum_ids[i]);

		const char * param[] = { checksumid, file->digests[i] };
		PGresult * result = PQexecPrepared(self->db_con, "select_checksumresult", 2, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR) {
			st_db_postgresql_get_error(result);

			PQclear(result);
			free(checksumid);

			return 1;
		} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_string_dup(result, 0, 0, &checksumresultid);

		PQclear(result);

		if (!checksumresultid) {
			result = PQexecPrepared(self->db_con, "insert_checksumresult", 2, param, 0, 0, 0);
			status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR) {
				st_db_postgresql_get_error(result);

				PQclear(result);
				free(checksumid);

				return 1;
			} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
				st_db_postgresql_get_string_dup(result, 0, 0, &checksumresultid);

			PQclear(result);
		}

		asprintf(&fileid, "%ld", file->id);

		const char * param2[] = { fileid, checksumresultid };
		result = PQexecPrepared(self->db_con, "insert_archive_file_to_checksum", 2, param2, 0, 0, 0);
		status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);

		PQclear(result);
		free(fileid);
		free(checksumresultid);
		free(checksumid);
	}

	return 0;
}

int st_db_postgresql_file_link_to_volume(struct st_database_connection * connection, struct st_archive_file * file, struct st_archive_volume * volume) {
	if (!connection || !file)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "insert_archivefiletoarchivevolume", "INSERT INTO archivefiletoarchivevolume VALUES ($1, $2)");

	char * volumeid = 0, * fileid = 0;
	asprintf(&volumeid, "%ld", volume->id);
	asprintf(&fileid, "%ld", file->id);

	const char * param[] = { volumeid, fileid };
	PGresult * result = PQexecPrepared(self->db_con, "insert_archivefiletoarchivevolume", 2, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);

	PQclear(result);
	free(volumeid);
	free(fileid);

	return status == PGRES_FATAL_ERROR;
}

int st_db_postgresql_new_archive(struct st_database_connection * connection, struct st_archive * archive) {
	if (!connection || !archive)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "get_job_metadata", "SELECT metadata FROM job WHERE id = $1 LIMIT 1");
	st_db_postgresql_prepare(self, "insert_archive", "INSERT INTO archive VALUES (DEFAULT, $1, $2, NULL, $3, $4) RETURNING id");

	char * jobid;
	asprintf(&jobid, "%ld", archive->job->id);

	const char * param1[] = { jobid };
	PGresult * result = PQexecPrepared(self->db_con, "get_job_metadata", 1, param1, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	free(jobid);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result);
		PQclear(result);
		return 1;
	}

	char * metadata;
	if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_string_dup(result, 0, 0, &metadata);

	PQclear(result);

	char ctime[32];
	struct tm local_current;
	localtime_r(&archive->ctime, &local_current);
	strftime(ctime, 32, "%F %T", &local_current);

	char * loginid = 0;
	asprintf(&loginid, "%ld", archive->user->id);

	const char * param2[] = { archive->name, ctime, loginid, metadata };
	result = PQexecPrepared(self->db_con, "insert_archive", 4, param2, 0, 0, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result);
		PQclear(result);
		free(loginid);
		free(metadata);
		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_long(result, 0, 0, &archive->id);

	PQclear(result);
	free(loginid);
	free(metadata);

	return status != PGRES_TUPLES_OK;
}

int st_db_postgresql_new_file(struct st_database_connection * connection, struct st_archive_file * file) {
	if (!connection || !file)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "insert_archive_file", "INSERT INTO archivefile VALUES (DEFAULT, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10) RETURNING id");

	char * perm = 0, * ownerid = 0, * groupid = 0, * size = 0;
	asprintf(&perm, "%o", file->perm & 0x777);
	asprintf(&ownerid, "%d", file->ownerid);
	asprintf(&groupid, "%d", file->groupid);
	asprintf(&size, "%zd", file->size);

	char ctime[32];
	struct tm local_current;
	localtime_r(&file->ctime, &local_current);
	strftime(ctime, 32, "%F %T", &local_current);

	char mtime[32];
	localtime_r(&file->ctime, &local_current);
	strftime(mtime, 32, "%F %T", &local_current);

	const char * param[] = {
		file->name, st_archive_file_type_to_string(file->type),
		ownerid, file->owner, groupid, file->group, perm,
		ctime, mtime, size,
	};
	PGresult * result = PQexecPrepared(self->db_con, "insert_archive_file", 10, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result);

		PQclear(result);

		free(perm);
		free(ownerid);
		free(groupid);
		free(size);

		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &file->id);
	}

	PQclear(result);

	return 0;
}

int st_db_postgresql_new_volume(struct st_database_connection * connection, struct st_archive_volume * volume) {
	if (!connection || !volume)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "insert_archive_volume", "INSERT INTO archivevolume VALUES (DEFAULT, $1, 0, $2, NULL, $3, $4, $5) RETURNING id");

	char * sequence = 0, * archiveid = 0, * tapeid = 0, * tapeposition = 0;
	asprintf(&sequence, "%ld", volume->sequence);
	asprintf(&archiveid, "%ld", volume->archive->id);
	asprintf(&tapeid, "%ld", volume->tape->id);
	asprintf(&tapeposition, "%ld", volume->tape_position);

	char ctime[32];
	struct tm local_current;
	localtime_r(&volume->ctime, &local_current);
	strftime(ctime, 32, "%F %T", &local_current);

	const char * param[] = { sequence, ctime, archiveid, tapeid, tapeposition };
	PGresult * result = PQexecPrepared(self->db_con, "insert_archive_volume", 5, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result);

		PQclear(result);
		free(tapeposition);
		free(tapeid);
		free(archiveid);
		free(sequence);

		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_long(result, 0, 0, &volume->id);

	PQclear(result);

	free(tapeposition);
	free(tapeid);
	free(archiveid);
	free(sequence);

	return status != PGRES_TUPLES_OK;
}

int st_db_postgresql_update_archive(struct st_database_connection * connection, struct st_archive * archive) {
	if (!connection || !archive)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "update_archive", "UPDATE archive SET endtime = $1 WHERE id = $2");

	char * archiveid = 0;
	asprintf(&archiveid, "%ld", archive->id);

	char endtime[32];
	struct tm local_current;
	localtime_r(&archive->endtime, &local_current);
	strftime(endtime, 32, "%F %T", &local_current);

	const char * param[] = { endtime, archiveid };
	PGresult * result = PQexecPrepared(self->db_con, "update_archive", 2, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);

	PQclear(result);
	free(archiveid);

	return status != PGRES_COMMAND_OK;
}

int st_db_postgresql_update_volume(struct st_database_connection * connection, struct st_archive_volume * volume) {
	if (!connection || !volume)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "update_archive_volume", "UPDATE archivevolume SET size = $1, endtime = $2 WHERE id = $3");

	char * volumeid = 0, * size = 0;
	asprintf(&volumeid, "%ld", volume->id);
	asprintf(&size, "%zd", volume->size);

	char endtime[32];
	struct tm local_current;
	localtime_r(&volume->endtime, &local_current);
	strftime(endtime, 32, "%F %T", &local_current);

	const char * param[] = { size, endtime, volumeid };
	PGresult * result = PQexecPrepared(self->db_con, "update_archive_volume", 3, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);

	PQclear(result);
	free(size);

	if (status != PGRES_COMMAND_OK)
		return 1;

	st_db_postgresql_prepare(self, "select_checksumresult", "SELECT id FROM checksumresult WHERE checksum = $1 AND result = $2 LIMIT 1");
	st_db_postgresql_prepare(self, "insert_checksumresult", "INSERT INTO checksumresult VALUES (DEFAULT, $1, $2) RETURNING id");
	st_db_postgresql_prepare(self, "insert_archive_volume_to_checksum", "INSERT INTO archivevolumetochecksumresult VALUES ($1, $2)");

	unsigned int i;
	for (i = 0; i < volume->nb_checksums; i++) {
		char * checksumid = 0, * checksumresultid = 0;
		asprintf(&checksumid, "%ld", volume->archive->job->checksum_ids[i]);

		const char * param2[] = { checksumid, volume->digests[i] };
		result = PQexecPrepared(self->db_con, "select_checksumresult", 2, param2, 0, 0, 0);
		status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR) {
			st_db_postgresql_get_error(result);

			PQclear(result);
			free(checksumid);

			return 1;
		} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_string_dup(result, 0, 0, &checksumresultid);

		PQclear(result);

		if (!checksumresultid) {
			result = PQexecPrepared(self->db_con, "insert_checksumresult", 2, param2, 0, 0, 0);
			status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR) {
				st_db_postgresql_get_error(result);

				free(checksumid);
				return 1;
			} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
				st_db_postgresql_get_string_dup(result, 0, 0, &checksumresultid);

			PQclear(result);
		}

		const char * param3[] = { volumeid, checksumresultid };
		result = PQexecPrepared(self->db_con, "insert_archive_volume_to_checksum", 2, param3, 0, 0, 0);
		status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);

		PQclear(result);
		free(checksumresultid);
		free(checksumid);
	}

	free(volumeid);

	return 0;
}


int st_db_postgresql_get_bool(PGresult * result, int row, int column, char * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value)
		*val = strcmp(value, "t") ? 0 : 1;

	return value != 0;
}

int st_db_postgresql_get_double(PGresult * result, int row, int column, double * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%lg", val) == 1)
		return 0;
	return -1;
}

void st_db_postgresql_get_error(PGresult * result) {
	char * error = PQresultErrorMessage(result);

	st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: error => %s", error);
}

int st_db_postgresql_get_int(PGresult * result, int row, int column, int * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%d", val) == 1)
		return 0;
	return -1;
}

int st_db_postgresql_get_long(PGresult * result, int row, int column, long * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%ld", val) == 1)
		return 0;
	return -1;
}

int st_db_postgresql_get_ssize(PGresult * result, int row, int column, ssize_t * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%zd", val) == 1)
		return 0;
	return -1;
}

int st_db_postgresql_get_string(PGresult * result, int row, int column, char * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value) {
		strcpy(val, value);
		return 0;
	}
	return -1;
}

int st_db_postgresql_get_string_dup(PGresult * result, int row, int column, char ** val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value) {
		*val = strdup(value);
		return 0;
	}
	return -1;
}

int st_db_postgresql_get_time(PGresult * result, int row, int column, time_t * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	struct tm tv;

	int failed = strptime(value, "%F %T", &tv) ? 0 : 1;
	if (!failed)
		*val = mktime(&tv);

	return failed;
}

int st_db_postgresql_get_uchar(PGresult * result, int row, int column, unsigned char * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	unsigned int tmp_val;
	if (value && sscanf(value, "%u", &tmp_val) == 1) {
		*val = tmp_val;
		return 0;
	}
	return -1;
}

void st_db_postgresql_prepare(struct st_db_postgresql_connetion_private * self, const char * statement_name, const char * query) {
	if (!st_hashtable_has_key(self->cached, statement_name)) {
		PGresult * prepare = PQprepare(self->db_con, statement_name, query, 0, 0);
		ExecStatusType status = PQresultStatus(prepare);
		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(prepare);
		else
			st_hashtable_put(self->cached, strdup(statement_name), 0);
		PQclear(prepare);

		st_log_write_all(status == PGRES_COMMAND_OK ? st_log_level_debug : st_log_level_error, st_log_type_plugin_db, "Postgresql: new query prepared (%s) => {%s}, status: %s", statement_name, query, PQresStatus(status));
	}
}

