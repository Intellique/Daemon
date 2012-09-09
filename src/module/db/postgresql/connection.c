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
*  Last modified: Sun, 09 Sep 2012 20:32:22 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
// free, malloc
#include <malloc.h>
// PQbackendPID, PQclear, PQerrorMessage, PQexec, PQfinish, PQfnumber, PQgetvalue, PQntuples, PQsetdbLogin, PQresultStatus, PQstatus
#include <postgresql/libpq-fe.h>
// uname
#include <sys/utsname.h>
// bool
#include <stdbool.h>
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
#include <stone/library/backup-db.h>
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

static int st_db_postgresql_add_job_record(struct st_database_connection * connection, struct st_job * job, const char * message);
static int st_db_postgresql_create_pool(struct st_database_connection * connection, struct st_pool * pool);
static ssize_t st_db_postgresql_get_available_size_by_pool(struct st_database_connection * connection, struct st_pool * pool);
static char * st_db_postgresql_get_host(struct st_database_connection * connection);
static int st_db_postgresql_get_nb_new_jobs(struct st_database_connection * connection, long * nb_new_jobs, time_t since, long last_max_jobs);
static int st_db_postgresql_get_new_jobs(struct st_database_connection * connection, struct st_job ** jobs, unsigned int nb_jobs, time_t since, long last_max_jobs);
static int st_db_postgresql_get_pool(struct st_database_connection * connection, struct st_pool * pool, long id, const char * uuid);
static int st_db_postgresql_get_tape(struct st_database_connection * connection, struct st_tape * tape, long id, const char * uuid, const char * label);
static int st_db_postgresql_get_tape_format(struct st_database_connection * connection, struct st_tape_format * tape_format, long id, unsigned char density_code);
static int st_db_postgresql_get_user(struct st_database_connection * connection, struct st_user * user, long user_id, const char * login);
static int st_db_postgresql_is_changer_contain_drive(struct st_database_connection * connection, struct st_changer * changer, struct st_drive * drive);
static int st_db_postgresql_refresh_job(struct st_database_connection * connection, struct st_job * job);
static int st_db_postgresql_sync_changer(struct st_database_connection * connection, struct st_changer * changer);
static int st_db_postgresql_sync_drive(struct st_database_connection * connection, struct st_drive * drive);
static int st_db_postgresql_sync_plugin_checksum(struct st_database_connection * connection, const char * plugin);
static int st_db_postgresql_sync_plugin_job(struct st_database_connection * connection, const char * plugin);
static int st_db_postgresql_sync_pool(struct st_database_connection * connection, struct st_pool * pool);
static int st_db_postgresql_sync_slot(struct st_database_connection * connection, struct st_slot * slot);
static int st_db_postgresql_sync_tape(struct st_database_connection * connection, struct st_tape * tape);
static int st_db_postgresql_sync_user(struct st_database_connection * connection, struct st_user * user);
static int st_db_postgresql_update_job(struct st_database_connection * connection, struct st_job * job);

static int st_db_postgresql_file_add_checksum(struct st_database_connection * connection, struct st_archive_file * file);
static int st_db_postgresql_file_link_to_volume(struct st_database_connection * connection, struct st_archive_file * file, struct st_archive_volume * volume);
static int st_db_postgresql_new_archive(struct st_database_connection * connection, struct st_archive * archive);
static int st_db_postgresql_new_file(struct st_database_connection * connection, struct st_archive_file * file);
static int st_db_postgresql_new_volume(struct st_database_connection * connection, struct st_archive_volume * volume);
static int st_db_postgresql_update_archive(struct st_database_connection * connection, struct st_archive * archive);
static int st_db_postgresql_update_volume(struct st_database_connection * connection, struct st_archive_volume * volume);

static int st_db_postgresql_add_backup(struct st_database_connection * connection, struct st_backupdb * backup);

static int st_db_postgresql_get_bool(PGresult * result, int row, int column, bool * value);
static int st_db_postgresql_get_double(PGresult * result, int row, int column, double * value);
static void st_db_postgresql_get_error(PGresult * result, const char * prepared_query);
static int st_db_postgresql_get_int(PGresult * result, int row, int column, int * value);
static int st_db_postgresql_get_long(PGresult * result, int row, int column, long * value);
static int st_db_postgresql_get_ssize(PGresult * result, int row, int column, ssize_t * value);
static int st_db_postgresql_get_string(PGresult * result, int row, int column, char * value);
static int st_db_postgresql_get_string_dup(PGresult * result, int row, int column, char ** value);
static int st_db_postgresql_get_time(PGresult * result, int row, int column, time_t * value);
static int st_db_postgresql_get_uchar(PGresult * result, int row, int column, unsigned char * value);
static int st_db_postgresql_get_uint(PGresult * result, int row, int column, unsigned int * value);
static void st_db_postgresql_prepare(struct st_db_postgresql_connetion_private * self, const char * statement_name, const char * query);

static struct st_database_connection_ops st_db_postgresql_con_ops = {
	.close = st_db_postgresql_con_close,
	.free  = st_db_postgresql_con_free,

	.cancel_transaction = st_db_postgresql_cancel_transaction,
	.finish_transaction = st_db_postgresql_finish_transaction,
	.start_transaction  = st_db_postgresql_start_transaction,

	.add_job_record             = st_db_postgresql_add_job_record,
	.create_pool                = st_db_postgresql_create_pool,
	.get_available_size_by_pool = st_db_postgresql_get_available_size_by_pool,
	.get_nb_new_jobs            = st_db_postgresql_get_nb_new_jobs,
	.get_new_jobs               = st_db_postgresql_get_new_jobs,
	.get_pool                   = st_db_postgresql_get_pool,
	.get_tape                   = st_db_postgresql_get_tape,
	.get_tape_format            = st_db_postgresql_get_tape_format,
	.get_user                   = st_db_postgresql_get_user,
	.is_changer_contain_drive   = st_db_postgresql_is_changer_contain_drive,
	.refresh_job                = st_db_postgresql_refresh_job,
	.sync_changer               = st_db_postgresql_sync_changer,
	.sync_drive                 = st_db_postgresql_sync_drive,
	.sync_plugin_checksum       = st_db_postgresql_sync_plugin_checksum,
	.sync_plugin_job            = st_db_postgresql_sync_plugin_job,
	.sync_pool                  = st_db_postgresql_sync_pool,
	.sync_tape                  = st_db_postgresql_sync_tape,
	.sync_user                  = st_db_postgresql_sync_user,
	.update_job                 = st_db_postgresql_update_job,

	.file_add_checksum   = st_db_postgresql_file_add_checksum,
	.file_link_to_volume = st_db_postgresql_file_link_to_volume,
	.new_archive         = st_db_postgresql_new_archive,
	.new_file            = st_db_postgresql_new_file,
	.new_volume          = st_db_postgresql_new_volume,
	.update_archive      = st_db_postgresql_update_archive,
	.update_volume       = st_db_postgresql_update_volume,

	.add_backup = st_db_postgresql_add_backup,
};


int st_db_postgresql_cancel_checkpoint(struct st_database_connection * connection, const char * checkpoint) {
	if (!connection || !checkpoint)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	char * query = 0;
	asprintf(&query, "ROLLBACK TO %s", checkpoint);

	PGresult * result = PQexec(self->db_con, query);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, 0);

	if (status != PGRES_COMMAND_OK)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql (cid #%ld): error while rollbacking a savepoint => %s", connection->id, PQerrorMessage(self->db_con));

	PQclear(result);
	free(query);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

int st_db_postgresql_cancel_transaction(struct st_database_connection * connection) {
	if (!connection)
		return -1;

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
		st_db_postgresql_get_error(result, 0);

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

	const char * query = "insert_job_record";
	st_db_postgresql_prepare(self, query, "INSERT INTO jobrecord VALUES (DEFAULT, $1, $2, $3, NOW(), $4)");

	char * jobid = 0, * numrun = 0;
	asprintf(&jobid, "%ld", job->id);
	asprintf(&numrun, "%ld", job->num_runs);

	const char * param[] = { jobid, st_job_status_to_string(job->sched_status), numrun, message };
	PGresult * result = PQexecPrepared(self->db_con, query, 4, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

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

	const char * query = "insert_pool";
	st_db_postgresql_prepare(self, query, "INSERT INTO pool(uuid, name, tapeformat) VALUES ($1, $2, $3) RETURNING id, growable");

	char * tape_format = 0;
	asprintf(&tape_format, "%ld", pool->format->id);

	const char * param[] = { pool->uuid, pool->name, tape_format, };
	PGresult * result = PQexecPrepared(self->db_con, query, 3, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &pool->id);
		st_db_postgresql_get_bool(result, 0, 1, &pool->growable);
	}

	PQclear(result);
	free(tape_format);

	return status == PGRES_FATAL_ERROR;
}

ssize_t st_db_postgresql_get_available_size_by_pool(struct st_database_connection * connection, struct st_pool * pool) {
	if (connection == NULL || pool == NULL)
		return 0;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	const char * query = "select_available_offline_size_by_pool";
	st_db_postgresql_prepare(self, query, "SELECT SUM(tf.capacity - t.endpos * t.blocksize::BIGINT) AS total FROM tape t LEFT JOIN tapeformat tf ON t.tapeformat = tf.id WHERE location = 'offline' AND pool = $1");

	char * poolid;
	asprintf(&poolid, "%ld", pool->id);

	const char * param[] = { poolid };
	PGresult * result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	free(poolid);

	ssize_t total_size = 0;
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1 && !PQgetisnull(result, 0, 0))
		st_db_postgresql_get_ssize(result, 0, 0, &total_size);

	PQclear(result);

	return total_size;
}

char * st_db_postgresql_get_host(struct st_database_connection * connection) {
	struct st_db_postgresql_connetion_private * self = connection->data;

	const char * query = "select_host_by_name";
	st_db_postgresql_prepare(self, query, "SELECT id FROM host WHERE name = $1 OR name || '.' || domaine = $1 LIMIT 1");

	struct utsname name;
	uname(&name);

	const char * param[] = { name.nodename };
	PGresult * result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

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

	const char * query0 = "select_nb_new_jobs1";
	st_db_postgresql_prepare(self, query0, "SELECT id FROM job WHERE id > $1 AND update < $2 AND host = $3 FOR SHARE");
	const char * query1 = "select_nb_new_jobs2";
	st_db_postgresql_prepare(self, query1, "SELECT COUNT(*) AS total FROM job WHERE id > $1 AND update < $2 AND host = $3");

	char csince[24], * lastmaxjobs = 0;
	struct tm tm_since;
	localtime_r(&since, &tm_since);
	strftime(csince, 23, "%F %T", &tm_since);
	asprintf(&lastmaxjobs, "%ld", last_max_jobs);

	const char * param[] = { lastmaxjobs, csince, hostid };
	PGresult * result = PQexecPrepared(self->db_con, "select_nb_new_jobs1", 3, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query0);

	PQclear(result);

	if (status == PGRES_FATAL_ERROR) {
		free(lastmaxjobs);
		free(hostid);
		return 1;
	}

	result = PQexecPrepared(self->db_con, "select_nb_new_jobs2", 3, param, 0, 0, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query1);
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

	const char * query0 = "select_new_jobs";
	st_db_postgresql_prepare(self, query0, "SELECT j.id, j.name, j.status, j.update, j.nextstart, j.interval, j.repetition, j.archive, j.backup, j.tape, j.pool, j.login, jt.name FROM job j LEFT JOIN jobtype jt ON j.type = jt.id WHERE j.id > $1 AND j.update < $2 AND j.host = $3 AND j.type = jt.id ORDER BY nextstart LIMIT $4");
	const char * query1 = "select_num_runs";
	st_db_postgresql_prepare(self, query1, "SELECT MAX(numrun) AS max FROM jobrecord WHERE job = $1");
	const char * query2 = "select_job_tape1";
	st_db_postgresql_prepare(self, query2, "SELECT av.sequence, t.uuid AS tape, av.tapeposition, afv.blocknumber, af.name, CASE WHEN af.type = 'regular file' THEN af.size ELSE 0 END AS size FROM archivevolume av LEFT JOIN archivefiletoarchivevolume afv ON av.id = afv.archivevolume LEFT JOIN archivefile af ON afv.archivefile = af.id LEFT JOIN tape t ON av.tape = t.id, (SELECT DISTINCT path, CHAR_LENGTH(path) AS length FROM selectedfile WHERE id IN (SELECT selectedfile FROM jobtoselectedfile WHERE job = $2)) AS sf WHERE archive = $1 AND SUBSTR(af.name, 0, sf.length + 1) = sf.path ORDER BY sequence, blocknumber");
	const char * query3 = "select_job_tape2";
	st_db_postgresql_prepare(self, query3, "SELECT av.sequence, t.uuid AS tape, av.tapeposition, 0 AS blocknumber, NULL AS name, SUM(av.size) AS size FROM archivevolume av LEFT JOIN tape t ON av.tape = t.id WHERE archive = $1 GROUP BY sequence, uuid, tapeposition, blocknumber ORDER BY sequence");
	const char * query4 = "select_pool_by_id";
	st_db_postgresql_prepare(self, query4, "SELECT uuid, tapeformat FROM pool WHERE id = $1 LIMIT 1");
	const char * query5 = "select_tapeformat_by_id";
	st_db_postgresql_prepare(self, query5, "SELECT densitycode FROM tapeformat WHERE id = $1 LIMIT 1");
	const char * query6 = "select_paths";
	st_db_postgresql_prepare(self, query6, "SELECT path FROM selectedfile WHERE id IN (SELECT selectedfile FROM jobtoselectedfile WHERE job = $1)");
	const char * query7 = "select_checksums";
	st_db_postgresql_prepare(self, query7, "SELECT id, name FROM checksum WHERE id IN (SELECT checksum FROM jobtochecksum WHERE job = $1)");
	const char * query8 = "select_job_meta";
	st_db_postgresql_prepare(self, query8, "SELECT * FROM each((SELECT metadata FROM job WHERE id = $1 LIMIT 1))");
	const char * query9 = "select_job_option";
	st_db_postgresql_prepare(self, query9, "SELECT * FROM each((SELECT options FROM job WHERE id = $1 LIMIT 1))");
	const char * query10 = "select_restore";
	st_db_postgresql_prepare(self, query10, "SELECT path, nbtruncpath FROM restoreto WHERE job = $1 LIMIT 1");
	const char * query11 = "select_archive";
	st_db_postgresql_prepare(self, query11, "SELECT a.id, a.name, a.ctime, a.endtime, t.pool, MAX(av.sequence) + 1 FROM archive a LEFT JOIN archivevolume av ON a.id = av.archive LEFT JOIN tape t ON av.tape = t.id WHERE a.id = $1 GROUP BY a.id, a.name, a.ctime, a.endtime, t.pool");
	const char * query12 = "select_archive_for_copy";
	st_db_postgresql_prepare(self, query12, "SELECT id, name, ctime, endtime FROM archive WHERE id = $1");
	const char * query13 = "select_backupdb";
	st_db_postgresql_prepare(self, query13, "SELECT id, timestamp FROM backup WHERE id = $1");
	const char * query14 = "select_backupdb_volumes";
	st_db_postgresql_prepare(self, query14, "SELECT id, sequence, tape, tapeposition FROM backupvolume WHERE backup = $1 ORDER BY sequence");

	char csince[24], * lastmaxjobs = 0, * nbjobs = 0;
	struct tm tm_since;
	localtime_r(&since, &tm_since);
	strftime(csince, 23, "%F %T", &tm_since);
	asprintf(&lastmaxjobs, "%ld", last_max_jobs);
	asprintf(&nbjobs, "%u", nb_jobs);

	const char * param[] = { lastmaxjobs, csince, hostid, nbjobs };
	PGresult * result = PQexecPrepared(self->db_con, query0, 4, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result, query0);

		PQclear(result);
		free(nbjobs);
		free(lastmaxjobs);
		free(hostid);
		return 1;
	}

	int i, nb_tuples = PQntuples(result);
	for (i = 0; i < nb_tuples; i++) {
		char * archiveid = 0, * backupid = 0, * jobid = 0, * poolid = 0;
		long userid;
		long tapeid = -1;

		jobs[i] = malloc(sizeof(struct st_job));
		bzero(jobs[i], sizeof(struct st_job));

		st_db_postgresql_get_long(result, i, 0, &jobs[i]->id);
		jobid = PQgetvalue(result, i, 0);
		st_db_postgresql_get_string_dup(result, i, 1, &jobs[i]->name);
		jobs[i]->db_status = st_job_string_to_status(PQgetvalue(result, i, 2));
		st_db_postgresql_get_time(result, i, 3, &jobs[i]->updated);

		st_db_postgresql_get_time(result, i, 4, &jobs[i]->start);
		jobs[i]->interval = 0;
		st_db_postgresql_get_long(result, i, 5, &jobs[i]->interval);
		st_db_postgresql_get_long(result, i, 6, &jobs[i]->repetition);

		if (!PQgetisnull(result, i, 7))
			archiveid = PQgetvalue(result, i, 7);
		if (!PQgetisnull(result, i, 8))
			backupid = PQgetvalue(result, i, 8);

		if (!PQgetisnull(result, i, 9))
			st_db_postgresql_get_long(result, i, 9, &tapeid);
		if (!PQgetisnull(result, i, 10))
			poolid = PQgetvalue(result, i, 10);

		st_db_postgresql_get_long(result, i, 11, &userid);
		jobs[i]->user = st_user_get(userid, 0);

		const char * param2[] = { jobid };
		PGresult * result2 = PQexecPrepared(self->db_con, query1, 1, param2, 0, 0, 0);
		ExecStatusType status2 = PQresultStatus(result2);

		if (status2 == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result2, query1);
		else if (status == PGRES_TUPLES_OK && PQntuples(result2) == 1 && !PQgetisnull(result2, 0, 0))
			st_db_postgresql_get_long(result2, 0, 0, &jobs[i]->num_runs);
		PQclear(result2);

		if (backupid) {
			struct st_backupdb * backup = jobs[i]->backup = malloc(sizeof(struct st_backupdb));

			const char * param3[] = { backupid };
			result2 = PQexecPrepared(self->db_con, query13, 1, param3, 0, 0, 0);
			ExecStatusType status3 = PQresultStatus(result2);

			if (status3 == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result2, query13);
			else if (status3 == PGRES_TUPLES_OK && PQntuples(result2) == 1) {
				st_db_postgresql_get_long(result2, 0, 0, &backup->id);
				st_db_postgresql_get_time(result2, 0, 1, &backup->ctime);
			}
			PQclear(result2);

			backup->volumes = 0;
			backup->nb_volumes = 0;

			result2 = PQexecPrepared(self->db_con, query14, 1, param3, 0, 0, 0);
			status3 = PQresultStatus(result2);
			int nb_result = PQntuples(result2);

			if (status3 == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result2, query13);
			else if (status3 == PGRES_TUPLES_OK && nb_result > 0) {
				backup->volumes = calloc(nb_result, sizeof(struct st_backupdb_volume));
				backup->nb_volumes = nb_result;

				int j;
				for (j = 0; j < nb_result; j++) {
					struct st_backupdb_volume * vol = backup->volumes + j;

					st_db_postgresql_get_long(result2, j, 0, &vol->id);
					st_db_postgresql_get_long(result2, j, 1, &vol->sequence);
					long tape_id = -1;
					st_db_postgresql_get_long(result2, j, 2, &tape_id);
					st_db_postgresql_get_long(result2, j, 3, &vol->tape_position);

					if (tape_id > -1)
						vol->tape = st_tape_get_by_id(tape_id);
				}
			}
			PQclear(result2);
		}

		if (tapeid > -1)
			jobs[i]->tape = st_tape_get_by_id(tapeid);

		if (poolid) {
			// TODO: use st_pool_get_by_id instead of st_pool_get_by_uuid
			char * pool_uuid = 0, * tapeformat = 0;
			unsigned char densitycode;

			const char * param3[] = { poolid };
			result2 = PQexecPrepared(self->db_con, query4, 1, param3, 0, 0, 0);
			ExecStatusType status3 = PQresultStatus(result2);

			if (status3 == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result2, query4);
			else if (status3 == PGRES_TUPLES_OK && PQntuples(result2) == 1) {
				st_db_postgresql_get_string_dup(result2, 0, 0, &pool_uuid);
				st_db_postgresql_get_string_dup(result2, 0, 1, &tapeformat);
			}
			PQclear(result2);

			const char * param4[] = { tapeformat };
			result2 = PQexecPrepared(self->db_con, query5, 1, param4, 0, 0, 0);
			status3 = PQresultStatus(result2);

			if (status3 == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result2, query5);
			else if (status3 == PGRES_TUPLES_OK && PQntuples(result2) == 1) {
				st_db_postgresql_get_uchar(result2, 0, 0, &densitycode);

				jobs[i]->pool = st_pool_get_by_uuid(pool_uuid);
			}
			PQclear(result2);

			free(tapeformat);
			free(pool_uuid);
		}

		result2 = PQexecPrepared(self->db_con, query6, 1, param2, 0, 0, 0);
		status2 = PQresultStatus(result2);

		if (status2 == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result2, query6);
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

		result2 = PQexecPrepared(self->db_con, query7, 1, param2, 0, 0, 0);
		status2 = PQresultStatus(result2);

		if (status2 == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result2, query7);
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

		// get job metadata
		jobs[i]->job_meta = st_hashtable_new2(st_util_compute_hash_string, st_util_basic_free);
		result2 = PQexecPrepared(self->db_con, query8, 1, param2, 0, 0, 0);

		if (status2 == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result2, query8);
		else if (PQresultStatus(result2) == PGRES_TUPLES_OK) {
			unsigned int j, nb_metadatas = PQntuples(result2);
			for (j = 0; j < nb_metadatas; j++)
				st_hashtable_put(jobs[i]->job_meta, strdup(PQgetvalue(result2, j, 0)), strdup(PQgetvalue(result2, j, 1)));
		}
		PQclear(result2);

		// get job option
		jobs[i]->job_option = st_hashtable_new2(st_util_compute_hash_string, st_util_basic_free);
		result2 = PQexecPrepared(self->db_con, query9, 1, param2, 0, 0, 0);

		if (status2 == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result2, query9);
		else if (PQresultStatus(result2) == PGRES_TUPLES_OK) {
			unsigned int j, nb_options = PQntuples(result2);
			for (j = 0; j < nb_options; j++)
				st_hashtable_put(jobs[i]->job_option, strdup(PQgetvalue(result2, j, 0)), strdup(PQgetvalue(result2, j, 1)));
		}
		PQclear(result2);

		// if job restore or copy or save into existing archive
		result2 = PQexecPrepared(self->db_con, query10, 1, param2, 0, 0, 0);
		status2 = PQresultStatus(result2);
		if (status2 == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result2, query10);
		else if (PQresultStatus(result2) == PGRES_TUPLES_OK && PQntuples(result2) == 1) {
			struct st_job_restore_to * rt = jobs[i]->restore_to = malloc(sizeof(struct st_job_restore_to));
			bzero(jobs[i]->restore_to, sizeof(struct st_job_restore_to));

			st_db_postgresql_get_string_dup(result2, 0, 0, &rt->path);
			st_db_postgresql_get_int(result2, 0, 1, &rt->nb_trunc_path);
		}
		PQclear(result2);

		if (archiveid) {
			if (jobs[i]->restore_to) {
				// job restore && copy
				const char * param3[] = { archiveid, jobid };

				const char * query;
				if (jobs[i]->nb_paths > 0) {
					query = query2;
					result2 = PQexecPrepared(self->db_con, query2, 2, param3, 0, 0, 0);
				} else {
					query = query3;
					result2 = PQexecPrepared(self->db_con, query3, 1, param3, 0, 0, 0);
				}

				status2 = PQresultStatus(result2);

				if (status2 == PGRES_FATAL_ERROR)
					st_db_postgresql_get_error(result2, query);
				else if (status2 == PGRES_TUPLES_OK) {
					int j, nb_tuples2 = PQntuples(result2);
					jobs[i]->block_numbers = calloc(nb_tuples2, sizeof(struct st_job_block_number));
					jobs[i]->nb_block_numbers = nb_tuples2;

					for (j = 0; j < nb_tuples2; j++) {
						struct st_job_block_number * block_number = jobs[i]->block_numbers + j;

						st_db_postgresql_get_long(result2, j, 0, &block_number->sequence);
						block_number->tape = st_tape_get_by_uuid(PQgetvalue(result2, j, 1));
						st_db_postgresql_get_long(result2, j, 2, &block_number->tape_position);
						st_db_postgresql_get_ssize(result2, j, 3, &block_number->block_number);
						st_db_postgresql_get_string_dup(result2, j, 4, &block_number->path);
						st_db_postgresql_get_ssize(result2, j, 5, &block_number->size);
					}
				}
				PQclear(result2);
			}

			if (!jobs[i]->restore_to || jobs[i]->nb_checksums > 0) {
				// job save with existing archive && copy
				const char * param3[] = { archiveid, };

				result2 = PQexecPrepared(self->db_con, query11, 1, param3, 0, 0, 0);

				if (status2 == PGRES_FATAL_ERROR)
					st_db_postgresql_get_error(result2, query11);
				else if (status2 == PGRES_TUPLES_OK && PQntuples(result2) == 1) {
					struct st_archive * archive = jobs[i]->archive = st_archive_new(jobs[i]);
					int poolid = -1;

					if (archive->name)
						free(archive->name);
					archive->name = 0;

					st_db_postgresql_get_long(result2, 0, 0, &archive->id);
					st_db_postgresql_get_string_dup(result2, 0, 1, &archive->name);
					st_db_postgresql_get_time(result2, 0, 2, &archive->ctime);
					st_db_postgresql_get_time(result2, 0, 3, &archive->endtime);
					st_db_postgresql_get_int(result2, 0, 4, &poolid);
					st_db_postgresql_get_uint(result2, 0, 5, &archive->next_sequence);

					if (poolid >= 0 && !jobs[i]->restore_to)
						jobs[i]->pool = st_pool_get_by_id(poolid);
				}

				PQclear(result2);
			}

			if (jobs[i]->restore_to && jobs[i]->nb_checksums > 0) {
				const char * param3[] = { archiveid, };

				result2 = PQexecPrepared(self->db_con, query12, 1, param3, 0, 0, 0);

				if (status2 == PGRES_FATAL_ERROR)
					st_db_postgresql_get_error(result2, query12);
				else if (status2 == PGRES_TUPLES_OK && PQntuples(result2) == 1) {
					struct st_archive * ar = jobs[i]->archive = malloc(sizeof(struct st_archive));

					st_db_postgresql_get_long(result2, 0, 0, &ar->id);
					st_db_postgresql_get_string_dup(result2, 0, 1, &ar->name);
					st_db_postgresql_get_time(result2, 0, 2, &ar->ctime);
					st_db_postgresql_get_time(result2, 0, 3, &ar->endtime);
				}

				PQclear(result2);
			}
		}

		jobs[i]->driver = st_job_get_driver(PQgetvalue(result, i, 12));
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
	const char * query;
	if (id > -1) {
		query = "select_pool_by_id";
		st_db_postgresql_prepare(self, query, "SELECT id, uuid, name, growable, tapeformat FROM pool WHERE id = $1 LIMIT 1");

		asprintf(&poolid, "%ld", id);

		const char * param[] = { poolid };
		result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
	} else {
		query = "select_pool_by_uuid";
		st_db_postgresql_prepare(self, query, "SELECT id, uuid, name, growable, tapeformat FROM pool WHERE uuid = $1 LIMIT 1");

		const char * param[] = { uuid };
		result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
	}
	ExecStatusType status = PQresultStatus(result);

	if (poolid)
		free(poolid);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &pool->id);
		st_db_postgresql_get_string(result, 0, 1, pool->uuid);
		st_db_postgresql_get_string(result, 0, 2, pool->name);
		st_db_postgresql_get_bool(result, 0, 3, &pool->growable);

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

int st_db_postgresql_get_tape(struct st_database_connection * connection, struct st_tape * tape, long id, const char * uuid, const char * label) {
	if (!connection || !tape)
		return 1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	PGresult * result = 0;
	const char * query;
	if (uuid) {
		query = "select_tape_by_uuid";
		st_db_postgresql_prepare(self, query, "SELECT id, uuid, label, mediumserialnumber, name, status, location, firstused, usebefore, loadcount, readcount, writecount, endpos, nbfiles, blocksize, haspartition, tapeformat, pool FROM tape WHERE uuid = $1 LIMIT 1");

		const char * param[] = { uuid };
		result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
	} else if (label) {
		query = "select_tape_by_label";
		st_db_postgresql_prepare(self, query, "SELECT id, uuid, label, mediumserialnumber, name, status, location, firstused, usebefore, loadcount, readcount, writecount, endpos, nbfiles, blocksize, haspartition, tapeformat, pool FROM tape WHERE label = $1 LIMIT 1");

		const char * param[] = { label };
		result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
	} else {
		query = "select_tape_by_id";
		st_db_postgresql_prepare(self, query, "SELECT id, uuid, label, mediumserialnumber, name, status, location, firstused, usebefore, loadcount, readcount, writecount, endpos, nbfiles, blocksize, haspartition, tapeformat, pool FROM tape WHERE id = $1 LIMIT 1");

		char * tapeid = 0;
		asprintf(&tapeid, "%ld", id);

		const char * param[] = { tapeid };
		result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);

		free(tapeid);
	}

	ExecStatusType status = PQresultStatus(result);
	int nb_tuples = PQntuples(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && nb_tuples == 1) {
		st_db_postgresql_get_long(result, 0, 0, &tape->id);
		st_db_postgresql_get_string(result, 0, 1, tape->uuid);
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

	return status != PGRES_TUPLES_OK || nb_tuples < 1;
}

int st_db_postgresql_get_tape_format(struct st_database_connection * connection, struct st_tape_format * tape_format, long id, unsigned char density_code) {
	if (!connection || !tape_format)
		return 1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	char * densitycode = 0, * tapeformatid = 0;
	PGresult * result = 0;

	const char * query;
	if (id > -1) {
		query = "select_tapeformat_by_id";
		st_db_postgresql_prepare(self, query, "SELECT id, name, datatype, mode, maxloadcount, maxreadcount, maxwritecount, maxopcount, lifespan, capacity, blocksize, densitycode, supportpartition, supportmam FROM tapeformat WHERE id = $1 LIMIT 1");

		asprintf(&tapeformatid, "%ld", id);

		const char * param[] = { tapeformatid };
		result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
	} else {
		query = "select_tapeformat_by_densitycode";
		st_db_postgresql_prepare(self, query, "SELECT id, name, datatype, mode, maxloadcount, maxreadcount, maxwritecount, maxopcount, lifespan, capacity, blocksize, densitycode, supportpartition, supportmam FROM tapeformat WHERE densityCode = $1 LIMIT 1");

		asprintf(&densitycode, "%d", density_code);

		const char * param[] = { densitycode };
		result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
	}
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
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

	const char * query = "select_user_by_id_or_login";
	st_db_postgresql_prepare(self, query, "SELECT id, login, password, salt, fullname, email, isadmin, canarchive, canrestore, disabled, pool FROM users WHERE id = $1 OR login = $2 LIMIT 1");

	char * userid = 0;
	asprintf(&userid, "%ld", user_id);

	const char * param[] = { userid, login };
	PGresult * result = PQexecPrepared(self->db_con, query, 2, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
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
		st_db_postgresql_get_bool(result, 0, 9, &user->disabled);

		long poolid = -1;
		st_db_postgresql_get_long(result, 0, 10, &poolid);
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

	const char * query = "select_changer_of_drive_by_model_vendor_serialnumber";
	st_db_postgresql_prepare(self, query, "SELECT changer FROM drive WHERE model = $1 AND vendor = $2 AND serialnumber = $3 LIMIT 1");

	char * changerid = 0;

	const char * param[] = { drive->model, drive->vendor, drive->serial_number };
	PGresult * result = PQexecPrepared(self->db_con, query, 3, param, 0, 0, 0);
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
	result = PQexecPrepared(self->db_con, query, 4, params2, 0, 0, 0);
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

int st_db_postgresql_refresh_job(struct st_database_connection * connection, struct st_job * job) {
	if (!connection || !job)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	const char * query = "select_job_since_by_id";
	st_db_postgresql_prepare(self, query, "SELECT status, update, nextstart FROM job WHERE id = $1");

	char * jobid = 0;
	asprintf(&jobid, "%ld", job->id);

	st_db_postgresql_start_transaction(connection);

	const char * param[] = { jobid };
	PGresult * result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		job->db_status = st_job_string_to_status(PQgetvalue(result, 0, 0));
		st_db_postgresql_get_time(result, 0, 1, &job->updated);
		st_db_postgresql_get_time(result, 0, 2, &job->start);

		PQclear(result);
		free(jobid);

		st_db_postgresql_cancel_transaction(connection);

		return 0;
	}
	PQclear(result);

	query = "select_job_by_id";
	st_db_postgresql_prepare(self, query, "SELECT id FROM job WHERE id = $1 LIMIT 1");
	result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
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
		const char * query = "select_changer_by_model_vendor_serialnumber";
		st_db_postgresql_prepare(self, query, "SELECT id, enable FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 FOR UPDATE NOWAIT");

		const char * param2[] = { changer->model, changer->vendor, changer->serial_number };
		PGresult * result = PQexecPrepared(self->db_con, query, 3, param2, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_long(result, 0, 0, &changer->id);
			st_db_postgresql_get_string_dup(result, 0, 0, &changerid);
			st_db_postgresql_get_bool(result, 0, 1, &changer->enabled);
		}

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			free(hostid);
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connection);
			return 3;
		}
	} else {
		const char * query = "select_changer_by_id";
		st_db_postgresql_prepare(self, query, "SELECT id, enable FROM changer WHERE id = $1 FOR UPDATE NOWAIT");

		char * changerid = 0;
		asprintf(&changerid, "%ld", changer->id);

		const char * param2[] = { changerid };
		PGresult * result = PQexecPrepared(self->db_con, query, 1, param2, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else
			st_db_postgresql_get_bool(result, 0, 1, &changer->enabled);

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
		const char * query = "insert_changer";
		st_db_postgresql_prepare(self, query, "INSERT INTO changer(device, status, barcode, model, vendor, firmwarerev, serialnumber, host) VALUES ($1, $2, $3, $4, $5, $6, $7, $8) RETURNING id");

		const char * param2[] = {
			changer->device, st_changer_status_to_string(changer->status), changer->barcode ? "true" : "false",
			changer->model, changer->vendor, changer->revision, changer->serial_number, hostid,
		};

		PGresult * result = PQexecPrepared(self->db_con, query, 8, param2, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR) {
			st_db_postgresql_get_error(result, query);
			free(result);
			free(hostid);
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connection);
			return -1;
		} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &changer->id);

		PQclear(result);
	} else {
		const char * query = "update_changer";
		st_db_postgresql_prepare(self, query, "UPDATE changer SET device = $1, status = $2, firmwarerev = $3 WHERE id = $4");

		const char * params2[] = { changer->device, st_changer_status_to_string(changer->status), changer->revision, changerid };
		PGresult * result = PQexecPrepared(self->db_con, query, 4, params2, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR) {
			st_db_postgresql_get_error(result, query);

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
		const char * query = "select_drive_by_model_vendor_serialnumber";
		st_db_postgresql_prepare(self, query, "SELECT id, operationduration, lastclean, driveformat, enable FROM drive WHERE model = $1 AND vendor = $2 AND serialnumber = $3 FOR UPDATE NOWAIT");

		const char * param[] = { drive->model, drive->vendor, drive->serial_number };
		PGresult * result = PQexecPrepared(self->db_con, query, 3, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
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

			st_db_postgresql_get_bool(result, 0, 4, &drive->enabled);
		}

		PQclear(result);

		if (status == PGRES_FATAL_ERROR) {
			if (transStatus == PQTRANS_IDLE)
				st_db_postgresql_cancel_transaction(connection);
			return 3;
		}
	} else {
		const char * query = "select_drive_by_id";
		st_db_postgresql_prepare(self, query, "SELECT enable FROM drive WHERE id = $1 FOR UPDATE NOWAIT");

		char * driveid = 0;
		asprintf(&driveid, "%ld", drive->id);

		const char * param[] = { driveid };
		PGresult * result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_bool(result, 0, 0, &drive->enabled);

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
		const char * query0 = "select_driveformat_by_densitycode";
		st_db_postgresql_prepare(self, query0, "SELECT id FROM driveformat WHERE densitycode = $1 LIMIT 1");
		const char * query1 = "insert_drive";
		st_db_postgresql_prepare(self, query1, "INSERT INTO drive(device, scsidevice, status, changernum, operationduration, lastclean, model, vendor, firmwarerev, serialnumber, changer, driveformat) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12) RETURNING id");

		char * densitycode = 0, * driveformat_id = 0;
		asprintf(&densitycode, "%u", drive->best_density_code);

		const char * param1[] = { densitycode };
		PGresult * result = PQexecPrepared(self->db_con, query0, 1, param1, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query0);
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
			drive->model, drive->vendor, drive->revision, drive->serial_number, changerid, driveformat_id,
		};
		result = PQexecPrepared(self->db_con, query1, 12, param2, 0, 0, 0);
		status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query1);
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
		const char * query0 = "select_driveformat_by_densitycode";
		st_db_postgresql_prepare(self, query0, "SELECT id FROM driveformat WHERE densitycode = $1 LIMIT 1");
		const char * query1 = "update_drive";
		st_db_postgresql_prepare(self, query1, "UPDATE drive SET device = $1, scsidevice = $2, status = $3, operationduration = $4, lastclean = $5, firmwarerev = $6, driveformat = $7 WHERE id = $8");

		char * densitycode = 0, * driveformat_id = 0;
		asprintf(&densitycode, "%u", drive->best_density_code);

		const char * param1[] = { densitycode };
		PGresult * result = PQexecPrepared(self->db_con, query0, 1, param1, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query0);
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
		result = PQexecPrepared(self->db_con, query1, 8, param2, 0, 0, 0);
		status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query1);

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

	const char * query = "select_checksum_by_name";
	st_db_postgresql_prepare(self, query, "SELECT name FROM checksum WHERE name = $1 LIMIT 1");

	const char * param[] = { plugin };
	PGresult * result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
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

	result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	PQclear(result);

	return status == PGRES_FATAL_ERROR;
}

int st_db_postgresql_sync_plugin_job(struct st_database_connection * connection, const char * plugin) {
	if (!connection || !plugin)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	const char * query = "select_jobtype_by_name";
	st_db_postgresql_prepare(self, query, "SELECT name FROM jobtype WHERE name = $1 LIMIT 1");

	const char * param[] = { plugin };
	PGresult * result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
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

	result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	PQclear(result);

	return status == PGRES_FATAL_ERROR;
}

int st_db_postgresql_sync_pool(struct st_database_connection * connection, struct st_pool * pool) {
	if (!connection || pool->id < 0)
		return 1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	const char * query = "select_pool_by_id";
	st_db_postgresql_prepare(self, query, "SELECT name, growable FROM pool WHERE id = $1 LIMIT 1");

	char * poolid = 0;
	asprintf(&poolid, "%ld", pool->id);

	const char * param[] = { poolid };
	PGresult * result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_string(result, 0, 0, pool->name);
		st_db_postgresql_get_bool(result, 0, 1, &pool->growable);
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
		const char * query = "select_slot_by_index_changer";
		st_db_postgresql_prepare(self, query, "SELECT id FROM changerslot WHERE index = $1 AND changer = $2 FOR UPDATE NOWAIT");

		char * changer_id = 0, * slot_index = 0;
		asprintf(&changer_id, "%ld", slot->changer->id);
		asprintf(&slot_index, "%td", slot - slot->changer->slots);

		const char * param[] = { slot_index, changer_id };
		PGresult * result = PQexecPrepared(self->db_con, query, 2, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
		else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &slot->id);

		PQclear(result);
		free(slot_index);
		free(changer_id);

		if (status == PGRES_FATAL_ERROR)
			return 2;
	} else {
		const char * query = "select_slot_by_id";
		st_db_postgresql_prepare(self, query, "SELECT id FROM changerslot WHERE id = $1 FOR UPDATE NOWAIT");

		char * slot_id = 0;
		asprintf(&slot_id, "%ld", slot->id);

		const char * param[] = { slot_id };
		PGresult * result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);

		PQclear(result);
		free(slot_id);

		if (status == PGRES_FATAL_ERROR)
			return 2;
	}

	if (slot->id > -1) {
		const char * query = "update_slot";
		st_db_postgresql_prepare(self, query, "UPDATE changerslot SET tape = $1 WHERE id = $2");

		char * slot_id = 0, * tape_id = 0;
		asprintf(&slot_id, "%ld", slot->id);
		if (slot->tape && slot->tape->id > -1)
			asprintf(&tape_id, "%ld", slot->tape->id);

		const char * param[] = { tape_id, slot_id };
		PGresult * result = PQexecPrepared(self->db_con, query, 2, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);

		PQclear(result);
		free(slot_id);
		if (tape_id)
			free(tape_id);

		return status == PGRES_FATAL_ERROR;
	} else {
		const char * query = "insert_slot";
		st_db_postgresql_prepare(self, query, "INSERT INTO changerslot(index, changer, tape, type) VALUES ($1, $2, $3, $4) RETURNING id");

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
		PGresult * result = PQexecPrepared(self->db_con, query, 4, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);
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
			const char * query = "select_tape_by_medium_serial_number";
			st_db_postgresql_prepare(self, query, "SELECT id, uuid, label, name, firstused, usebefore, loadcount, readcount, writecount, endpos, blocksize FROM tape WHERE mediumserialnumber = $1 FOR UPDATE NOWAIT");

			const char * param[] = { tape->medium_serial_number };
			PGresult * result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result, query);
			else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
				st_db_postgresql_get_long(result, 0, 0, &tape->id);
				st_db_postgresql_get_string(result, 0, 1, tape->uuid);
				st_db_postgresql_get_string(result, 0, 2, tape->label);
				st_db_postgresql_get_string(result, 0, 3, tape->name);
				st_db_postgresql_get_time(result, 0, 4, &tape->first_used);
				st_db_postgresql_get_time(result, 0, 5, &tape->use_before);

				long old_value = tape->load_count;
				if (!tape->mam_ok) {
					st_db_postgresql_get_long(result, 0, 6, &tape->load_count);
					tape->load_count += old_value;
				}

				old_value = tape->read_count;
				st_db_postgresql_get_long(result, 0, 7, &tape->read_count);
				tape->read_count += old_value;

				old_value = tape->write_count;
				st_db_postgresql_get_long(result, 0, 8, &tape->write_count);
				tape->write_count += old_value;

				if (tape->end_position == 0)
					st_db_postgresql_get_ssize(result, 0, 9, &tape->end_position);
				st_db_postgresql_get_ssize(result, 0, 10, &tape->block_size);
			}

			PQclear(result);
		}

		if (tape->uuid[0] != '\0') {
			const char * query = "select_tape_by_uuid";
			st_db_postgresql_prepare(self, query, "SELECT id, label, name, firstused, usebefore, loadcount, readcount, writecount, endpos, blocksize FROM tape WHERE uuid = $1 FOR UPDATE NOWAIT");

			const char * param[] = { tape->uuid };
			PGresult * result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result, query);
			else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
				st_db_postgresql_get_long(result, 0, 0, &tape->id);
				st_db_postgresql_get_string(result, 0, 1, tape->label);
				st_db_postgresql_get_string(result, 0, 2, tape->name);
				st_db_postgresql_get_time(result, 0, 3, &tape->first_used);
				st_db_postgresql_get_time(result, 0, 4, &tape->use_before);

				long old_value = tape->load_count;
				if (!tape->mam_ok) {
					st_db_postgresql_get_long(result, 0, 5, &tape->load_count);
					tape->load_count += old_value;
				}

				old_value = tape->read_count;
				st_db_postgresql_get_long(result, 0, 6, &tape->read_count);
				tape->read_count += old_value;

				old_value = tape->write_count;
				st_db_postgresql_get_long(result, 0, 7, &tape->write_count);
				tape->write_count += old_value;

				if (tape->end_position == 0)
					st_db_postgresql_get_ssize(result, 0, 8, &tape->end_position);
				st_db_postgresql_get_ssize(result, 0, 9, &tape->block_size);
			}

			PQclear(result);
		}

		if (tape->id < 0 && tape->label[0] != '\0') {
			const char * query = "select_tape_by_label";
			st_db_postgresql_prepare(self, query, "SELECT id, name, firstused, usebefore, loadcount, readcount, writecount, endpos, blocksize FROM tape WHERE label = $1 FOR UPDATE NOWAIT");

			const char * param[] = { tape->label };
			PGresult * result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result, query);
			else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
				st_db_postgresql_get_long(result, 0, 0, &tape->id);
				st_db_postgresql_get_string(result, 0, 1, tape->name);
				st_db_postgresql_get_time(result, 0, 2, &tape->first_used);
				st_db_postgresql_get_time(result, 0, 3, &tape->use_before);

				long old_value = tape->load_count;
				if (!tape->mam_ok) {
					st_db_postgresql_get_long(result, 0, 4, &tape->load_count);
					tape->load_count += old_value;
				}

				old_value = tape->read_count;
				st_db_postgresql_get_long(result, 0, 5, &tape->read_count);
				tape->read_count += old_value;

				old_value = tape->write_count;
				st_db_postgresql_get_long(result, 0, 6, &tape->write_count);
				tape->write_count += old_value;

				if (tape->end_position == 0)
					st_db_postgresql_get_ssize(result, 0, 7, &tape->end_position);
				st_db_postgresql_get_ssize(result, 0, 8, &tape->block_size);
			}

			PQclear(result);
		}
	} else {
		const char * query = "select_tape_by_id";
		st_db_postgresql_prepare(self, query, "SELECT id FROM tape WHERE id = $1 FOR UPDATE NOWAIT");

		char * tape_id = 0;
		asprintf(&tape_id, "%ld", tape->id);

		const char * param[] = { tape_id };
		PGresult * result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query);

		PQclear(result);
		free(tape_id);

		if (status == PGRES_FATAL_ERROR)
			return 2;
	}

	if (tape->medium_serial_number[0] != '\0' || tape->uuid[0] != '\0' || tape->label[0] != '\0') {
		if (tape->id > -1) {
			const char * query = "update_tape";
			st_db_postgresql_prepare(self, query, "UPDATE tape SET uuid = $1, name = $2, status = $3, location = $4, loadcount = $5, readcount = $6, writecount = $7, endpos = $8, nbfiles = $9, blocksize = $10, haspartition = $11, pool = $12 WHERE id = $13");

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
			PGresult * result = PQexecPrepared(self->db_con, query, 13, param, 0, 0, 0);
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
			free(id);
			if (pool)
				free(pool);

			return status == PGRES_FATAL_ERROR;
		} else {
			const char * query = "insert_tape";
			st_db_postgresql_prepare(self, query, "INSERT INTO tape(uuid, label, mediumserialnumber, name, status, location, firstused, usebefore, loadcount, readcount, writecount, endpos, nbfiles, blocksize, haspartition, tapeformat, pool) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17) RETURNING id");

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
			PGresult * result = PQexecPrepared(self->db_con, query, 17, param, 0, 0, 0);
			ExecStatusType status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result, query);
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

	const char * query = "select_user_by_id";
	st_db_postgresql_prepare(self, query, "SELECT password, salt, fullname, email, isadmin, canarchive, canrestore, disabled, pool FROM users WHERE id = $1 LIMIT 1");

	char * userid = 0;
	asprintf(&userid, "%ld", user->id);

	const char * param[] = { userid };
	PGresult * result = PQexecPrepared(self->db_con, query, 1, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);
	else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		if (user->password)
			free(user->password);
		st_db_postgresql_get_string_dup(result, 0, 0, &user->password);

		if (user->salt)
			free(user->salt);
		st_db_postgresql_get_string_dup(result, 0, 1, &user->salt);

		if (user->fullname)
			free(user->fullname);
		st_db_postgresql_get_string_dup(result, 0, 2, &user->fullname);

		if (user->email)
			free(user->email);
		st_db_postgresql_get_string_dup(result, 0, 3, &user->email);

		st_db_postgresql_get_bool(result, 0, 4, &user->is_admin);
		st_db_postgresql_get_bool(result, 0, 5, &user->can_archive);
		st_db_postgresql_get_bool(result, 0, 6, &user->can_restore);

		st_db_postgresql_get_bool(result, 0, 7, &user->disabled);

		long poolid = -1;
		st_db_postgresql_get_long(result, 0, 8, &poolid);
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

	const char * query0 = "select_job_before_update";
	st_db_postgresql_prepare(self, query0, "SELECT status FROM job WHERE id = $1 LIMIT 1");

	char * cdone = 0, * crepetition = 0, * jobid = 0;
	asprintf(&jobid, "%ld", job->id);
	asprintf(&cdone, "%f", job->done);
	asprintf(&crepetition, "%ld", job->repetition);

	const char * param[] = { jobid, cdone, st_job_status_to_string(job->sched_status), crepetition };
	PGresult * result = PQexecPrepared(self->db_con, query0, 1, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result, query0);
		PQclear(result);

		free(jobid);
		free(cdone);
		free(crepetition);

		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 0) {
		PQclear(result);

		free(jobid);
		free(cdone);
		free(crepetition);

		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		job->db_status = st_job_string_to_status(PQgetvalue(result, 0, 0));
	}

	PQclear(result);

	if (job->db_status != job->sched_status && job->sched_status == st_job_status_stopped) {
		free(jobid);
		free(crepetition);
		free(cdone);

		return 0;
	}

	const char * query1 = "update_job";
	st_db_postgresql_prepare(self, query1, "UPDATE job SET done = $2, status = $3, repetition = $4, update = NOW() WHERE id = $1");

	result = PQexecPrepared(self->db_con, query1, 4, param, 0, 0, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query1);

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

	const char * query0 = "select_checksumresult";
	st_db_postgresql_prepare(self, query0, "SELECT id FROM checksumresult WHERE checksum = $1 AND result = $2 LIMIT 1");
	const char * query1 = "insert_checksumresult";
	st_db_postgresql_prepare(self, query1, "INSERT INTO checksumresult(checksum, result) VALUES ($1, $2) RETURNING id");
	const char * query2 = "insert_archive_file_to_checksum";
	st_db_postgresql_prepare(self, query2, "INSERT INTO archivefiletochecksumresult(archivefile, checksumresult) VALUES ($1, $2)");

	unsigned int i;
	for (i = 0; i < file->nb_checksums; i++) {
		char * checksumid = 0, * checksumresultid = 0, * fileid = 0;
		asprintf(&checksumid, "%ld", file->archive->job->checksum_ids[i]);

		const char * param[] = { checksumid, file->digests[i] };
		PGresult * result = PQexecPrepared(self->db_con, query0, 2, param, 0, 0, 0);
		ExecStatusType status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR) {
			st_db_postgresql_get_error(result, query0);

			PQclear(result);
			free(checksumid);

			return 1;
		} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_string_dup(result, 0, 0, &checksumresultid);

		PQclear(result);

		if (!checksumresultid) {
			result = PQexecPrepared(self->db_con, query1, 2, param, 0, 0, 0);
			status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR) {
				st_db_postgresql_get_error(result, query1);

				PQclear(result);
				free(checksumid);

				return 1;
			} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
				st_db_postgresql_get_string_dup(result, 0, 0, &checksumresultid);

			PQclear(result);
		}

		asprintf(&fileid, "%ld", file->id);

		const char * param2[] = { fileid, checksumresultid };
		result = PQexecPrepared(self->db_con, query2, 2, param2, 0, 0, 0);
		status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query2);

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

	const char * query = "insert_archivefiletoarchivevolume";
	st_db_postgresql_prepare(self, query, "INSERT INTO archivefiletoarchivevolume(archivevolume, archivefile, blocknumber) VALUES ($1, $2, $3)");

	char * volumeid = 0, * fileid = 0, * block_number = 0;
	asprintf(&volumeid, "%ld", volume->id);
	asprintf(&fileid, "%ld", file->id);
	asprintf(&block_number, "%zd", file->position);

	const char * param[] = { volumeid, fileid, block_number, };
	PGresult * result = PQexecPrepared(self->db_con, query, 3, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	PQclear(result);
	free(volumeid);
	free(fileid);
	free(block_number);

	return status == PGRES_FATAL_ERROR;
}

int st_db_postgresql_new_archive(struct st_database_connection * connection, struct st_archive * archive) {
	if (!connection || !archive)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	const char * query0 = "get_job_metadata";
	st_db_postgresql_prepare(self, query0, "SELECT metadata FROM job WHERE id = $1 LIMIT 1");
	const char * query1 = "insert_archive";
	st_db_postgresql_prepare(self, query1, "INSERT INTO archive(name, ctime, creator, owner, metadata, copyof) VALUES ($1, $2, $3, $3, $4, $5) RETURNING id");
	const char * query2 = "copy_link_volume_file";
	st_db_postgresql_prepare(self, query2, "SELECT af.id FROM archivefiletoarchivevolume afv LEFT JOIN archivevolume av ON afv.archivevolume = av.id AND av.id = $1 LEFT JOIN archivefile af ON afv.archivefile = af.id WHERE $2 = af.name LIMIT 1");
	const char * query3 = "copy_link_volume_file2";
	st_db_postgresql_prepare(self, query3, "INSERT INTO archivefiletoarchivevolume(archivevolume, archivefile, blocknumber) VALUES ($1, $2, $3)");

	char * jobid;
	asprintf(&jobid, "%ld", archive->job->id);

	const char * param0[] = { jobid };
	PGresult * result = PQexecPrepared(self->db_con, query0, 1, param0, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	free(jobid);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result, query0);
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

	char * copyof = 0;
	if (archive->copy_of)
		asprintf(&copyof, "%ld", archive->copy_of->id);

	const char * param1[] = { archive->name, ctime, loginid, metadata, copyof };
	result = PQexecPrepared(self->db_con, query1, 5, param1, 0, 0, 0);
	status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result, query1);
		PQclear(result);
		free(loginid);
		free(metadata);
		free(copyof);
		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_long(result, 0, 0, &archive->id);

	PQclear(result);
	free(loginid);
	free(metadata);
	free(copyof);

	if (status != PGRES_TUPLES_OK || !archive->copy_of)
		return 1;

	unsigned int i, j;
	for (i = 0; i < archive->nb_volumes; i++)
		st_db_postgresql_new_volume(connection, archive->volumes + i);

	char * archive_copy_id = 0;
	asprintf(&archive_copy_id, "%ld", archive->copy_of->id);

	for (i = 0; i < archive->nb_volumes; i++) {
		struct st_archive_volume * v = archive->volumes + i;

		char * archive_volume_id = 0;
		asprintf(&archive_volume_id, "%ld", v->id);

		for (j = 0; j < v->nb_files; j++) {
			const char * param2[] = { archive_copy_id, v->files[j].file->name, };

			result = PQexecPrepared(self->db_con, query2, 2, param2, 0, 0, 0);

			if (status == PGRES_FATAL_ERROR) {
				st_db_postgresql_get_error(result, query1);
				PQclear(result);
			} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 0) {
				PQclear(result);
				continue;
			}

			char * file_id, * block_number;
			st_db_postgresql_get_string_dup(result, 0, 0, &file_id);
			asprintf(&block_number, "%zd", v->files[j].position);

			PQclear(result);

			const char * param3[] = { archive_volume_id, file_id, block_number, };
			result = PQexecPrepared(self->db_con, query3, 3, param3, 0, 0, 0);

			if (status == PGRES_FATAL_ERROR)
				st_db_postgresql_get_error(result, query3);

			free(file_id);
			free(block_number);

			PQclear(result);
		}

		free(archive_volume_id);

		st_db_postgresql_update_volume(connection, v);
	}

	if (archive_copy_id)
		free(archive_copy_id);

	return 0;
}

int st_db_postgresql_new_file(struct st_database_connection * connection, struct st_archive_file * file) {
	if (!connection || !file)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	const char * query = "insert_archive_file";
	st_db_postgresql_prepare(self, query, "INSERT INTO archivefile(name, type, mimetype, ownerid, owner, groupid, groups, perm, ctime, mtime, size) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11) RETURNING id");

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
		"", ownerid, file->owner, groupid, file->group, perm,
		ctime, mtime, size,
	};
	PGresult * result = PQexecPrepared(self->db_con, query, 11, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	free(perm);
	free(ownerid);
	free(groupid);
	free(size);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result, query);

		PQclear(result);

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

	const char * query = "insert_archive_volume";
	st_db_postgresql_prepare(self, query, "INSERT INTO archivevolume(sequence, ctime, archive, tape, tapeposition) VALUES ($1, $2, $3, $4, $5) RETURNING id");

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
	PGresult * result = PQexecPrepared(self->db_con, query, 5, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result, query);

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

	const char * query = "update_archive";
	st_db_postgresql_prepare(self, query, "UPDATE archive SET endtime = $1 WHERE id = $2");

	char * archiveid = 0;
	asprintf(&archiveid, "%ld", archive->id);

	char endtime[32];
	struct tm local_current;
	localtime_r(&archive->endtime, &local_current);
	strftime(endtime, 32, "%F %T", &local_current);

	const char * param[] = { endtime, archiveid };
	PGresult * result = PQexecPrepared(self->db_con, query, 2, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query);

	PQclear(result);
	free(archiveid);

	return status != PGRES_COMMAND_OK;
}

int st_db_postgresql_update_volume(struct st_database_connection * connection, struct st_archive_volume * volume) {
	if (!connection || !volume)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	const char * query0 = "update_archive_volume";
	st_db_postgresql_prepare(self, query0, "UPDATE archivevolume SET size = $1, endtime = $2 WHERE id = $3");

	char * volumeid = 0, * size = 0;
	asprintf(&volumeid, "%ld", volume->id);
	asprintf(&size, "%zd", volume->size);

	char endtime[32];
	struct tm local_current;
	localtime_r(&volume->endtime, &local_current);
	strftime(endtime, 32, "%F %T", &local_current);

	const char * param[] = { size, endtime, volumeid };
	PGresult * result = PQexecPrepared(self->db_con, query0, 3, param, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result, query0);

	PQclear(result);
	free(size);

	if (status != PGRES_COMMAND_OK) {
		free(volumeid);
		return 1;
	}

	const char * query1 = "select_checksumresult";
	st_db_postgresql_prepare(self, query1, "SELECT id FROM checksumresult WHERE checksum = $1 AND result = $2 LIMIT 1");
	const char * query2 = "insert_checksumresult";
	st_db_postgresql_prepare(self, query2, "INSERT INTO checksumresult VALUES (DEFAULT, $1, $2) RETURNING id");
	const char * query3 = "insert_archive_volume_to_checksum";
	st_db_postgresql_prepare(self, query3, "INSERT INTO archivevolumetochecksumresult VALUES ($1, $2)");

	unsigned int i;
	for (i = 0; i < volume->nb_checksums; i++) {
		char * checksumid = 0, * checksumresultid = 0;
		asprintf(&checksumid, "%ld", volume->archive->job->checksum_ids[i]);

		const char * param2[] = { checksumid, volume->digests[i] };
		result = PQexecPrepared(self->db_con, query1, 2, param2, 0, 0, 0);
		status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR) {
			st_db_postgresql_get_error(result, query1);

			PQclear(result);
			free(checksumid);
			free(volumeid);

			return 1;
		} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_string_dup(result, 0, 0, &checksumresultid);

		PQclear(result);

		if (!checksumresultid) {
			result = PQexecPrepared(self->db_con, query2, 2, param2, 0, 0, 0);
			status = PQresultStatus(result);

			if (status == PGRES_FATAL_ERROR) {
				st_db_postgresql_get_error(result, query2);

				free(checksumid);
				free(volumeid);
				return 1;
			} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
				st_db_postgresql_get_string_dup(result, 0, 0, &checksumresultid);

			PQclear(result);
		}

		const char * param3[] = { volumeid, checksumresultid };
		result = PQexecPrepared(self->db_con, query3, 2, param3, 0, 0, 0);
		status = PQresultStatus(result);

		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result, query3);

		PQclear(result);
		free(checksumresultid);
		free(checksumid);
	}

	free(volumeid);

	return 0;
}


int st_db_postgresql_add_backup(struct st_database_connection * connection, struct st_backupdb * backup) {
	if (!connection || !backup)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	const char * query = "select_nb_tapes";
	st_db_postgresql_prepare(self, query, "SELECT COUNT(*) FROM tape WHERE status = 'in use'");

	PGresult * result = PQexecPrepared(self->db_con, query, 0, 0, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);

	char * nb_tapes = 0;
	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result, query);
		PQclear(result);
		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_string_dup(result, 0, 0, &nb_tapes);

	PQclear(result);

	query = "select_nb_archives";
	st_db_postgresql_prepare(self, query, "SELECT COUNT(*) FROM archive");

	result = PQexecPrepared(self->db_con, query, 0, 0, 0, 0, 0);
	status = PQresultStatus(result);

	char * nb_archives = 0;
	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result, query);
		free(nb_tapes);
		PQclear(result);
		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
		st_db_postgresql_get_string_dup(result, 0, 0, &nb_archives);

	PQclear(result);

	query = "insert_backup";
	st_db_postgresql_prepare(self, query, "INSERT INTO backup(timestamp, nbtape, nbarchive) VALUES ($1, $2, $3) RETURNING id");

	char ctime[32];
	struct tm local_current;
	localtime_r(&backup->ctime, &local_current);
	strftime(ctime, 32, "%F %T", &local_current);

	const char * param[] = { ctime, nb_tapes, nb_archives };
	result = PQexecPrepared(self->db_con, query, 3, param, 0, 0, 0);
	status = PQresultStatus(result);

	char * backupid = 0;

	if (status == PGRES_FATAL_ERROR) {
		st_db_postgresql_get_error(result, query);
		PQclear(result);
		free(nb_tapes);
		free(nb_archives);
		return 1;
	} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &backup->id);
		st_db_postgresql_get_string_dup(result, 0, 0, &backupid);
	}

	PQclear(result);

	query = "insert_backup_volume";
	st_db_postgresql_prepare(self, query, "INSERT INTO backupvolume(sequence, backup, tape, tapeposition) VALUES ($1, $2, $3, $4)");

	unsigned int i;
	for (i = 0; i < backup->nb_volumes; i++) {
		struct st_backupdb_volume * vol = backup->volumes + i;

		char * seq = 0, * tape_id = 0, * tape_position = 0;
		asprintf(&seq, "%ld", vol->sequence);
		asprintf(&tape_id, "%ld", vol->tape->id);
		asprintf(&tape_position, "%ld", vol->tape_position);

		const char * param1[] = { seq, backupid, tape_id, tape_position };
		result = PQexecPrepared(self->db_con, query, 4, param1, 0, 0, 0);
		status = PQresultStatus(result);

		free(seq);
		free(tape_id);
		free(tape_position);

		if (status == PGRES_FATAL_ERROR) {
			st_db_postgresql_get_error(result, query);
			PQclear(result);
		}
	}

	free(backupid);
	free(nb_tapes);
	free(nb_archives);

	return 0;
}


int st_db_postgresql_get_bool(PGresult * result, int row, int column, bool * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value)
		*val = strcmp(value, "t") ? false : true;

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

void st_db_postgresql_get_error(PGresult * result, const char * prepared_query) {
	char * error = PQresultErrorMessage(result);

	if (prepared_query)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: error {%s} => {%s}", prepared_query, error);
	else
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql: error => {%s}", error);
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

int st_db_postgresql_get_uint(PGresult * result, int row, int column, unsigned int * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%u", val) == 1)
		return 0;
	return -1;
}

void st_db_postgresql_prepare(struct st_db_postgresql_connetion_private * self, const char * statement_name, const char * query) {
	if (!st_hashtable_has_key(self->cached, statement_name)) {
		PGresult * prepare = PQprepare(self->db_con, statement_name, query, 0, 0);
		ExecStatusType status = PQresultStatus(prepare);
		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(prepare, statement_name);
		else
			st_hashtable_put(self->cached, strdup(statement_name), 0);
		PQclear(prepare);

		st_log_write_all(status == PGRES_COMMAND_OK ? st_log_level_debug : st_log_level_error, st_log_type_plugin_db, "Postgresql: new query prepared (%s) => {%s}, status: %s", statement_name, query, PQresStatus(status));
	}
}

