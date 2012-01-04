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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Wed, 04 Jan 2012 11:19:21 +0100                         *
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
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/tape.h>
#include <stone/log.h>
#include <stone/user.h>

#include "common.h"

static void st_db_postgresql_check(struct st_database_connection * connection);
static int st_db_postgresql_con_close(struct st_database_connection * connection);
static int st_db_postgresql_con_free(struct st_database_connection * connection);

static int st_db_postgresql_cancel_transaction(struct st_database_connection * connection);
static int st_db_postgresql_finish_transaction(struct st_database_connection * connection);
static int st_db_postgresql_start_transaction(struct st_database_connection * connection, short read_only);

static int st_db_postgresql_add_job_record(struct st_database_connection * db, struct st_job * job, const char * message);
static int st_db_postgresql_create_pool(struct st_database_connection * db, struct st_pool * pool);
static char * st_db_postgresql_get_host(struct st_database_connection * connection);
static int st_db_postgresql_get_nb_new_jobs(struct st_database_connection * db, long * nb_new_jobs, time_t since, long last_max_jobs);
static int st_db_postgresql_get_new_jobs(struct st_database_connection * db, struct st_job ** jobs, unsigned int nb_jobs, time_t since, long last_max_jobs);
static int st_db_postgresql_get_pool(struct st_database_connection * db, struct st_pool * pool, const char * uuid);
static int st_db_postgresql_get_tape_format(struct st_database_connection * db, struct st_tape_format * tape_format, unsigned char density_code);
static int st_db_postgresql_get_user(struct st_database_connection * db, struct st_user * user, long user_id, const char * login);
static int st_db_postgresql_is_changer_contain_drive(struct st_database_connection * db, struct st_changer * changer, struct st_drive * drive);
static int st_db_postgresql_refresh_job(struct st_database_connection * db, struct st_job * job);
static int st_db_postgresql_sync_changer(struct st_database_connection * db, struct st_changer * changer);
static int st_db_postgresql_sync_drive(struct st_database_connection * db, struct st_drive * drive);
static int st_db_postgresql_sync_plugin_checksum(struct st_database_connection * db, const char * plugin);
static int st_db_postgresql_sync_pool(struct st_database_connection * db, struct st_pool * pool);
static int st_db_postgresql_sync_slot(struct st_database_connection * db, struct st_slot * slot);
static int st_db_postgresql_sync_tape(struct st_database_connection * db, struct st_tape * tape);
static int st_db_postgresql_sync_user(struct st_database_connection * db, struct st_user * user);
static int st_db_postgresql_update_job(struct st_database_connection * db, struct st_job * job);

static int st_db_postgresql_get_bool(PGresult * result, int row, int column, char * value);
static int st_db_postgresql_get_double(PGresult * result, int row, int column, double * value);
static void st_db_postgresql_get_error(PGresult * result);
//static int st_db_postgresql_get_int(PGresult * result, int row, int column, int * value);
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
	.get_tape_format          = st_db_postgresql_get_tape_format,
	.get_user                 = st_db_postgresql_get_user,
	.is_changer_contain_drive = st_db_postgresql_is_changer_contain_drive,
	.refresh_job              = st_db_postgresql_refresh_job,
	.sync_changer             = st_db_postgresql_sync_changer,
	.sync_drive               = st_db_postgresql_sync_drive,
	.sync_plugin_checksum     = st_db_postgresql_sync_plugin_checksum,
	.sync_pool                = st_db_postgresql_sync_pool,
	.sync_tape                = st_db_postgresql_sync_tape,
	.sync_user                = st_db_postgresql_sync_user,
	.update_job               = st_db_postgresql_update_job,
};


int st_db_postgresql_add_job_record(struct st_database_connection * connection, struct st_job * job, const char * message) {
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

int st_db_postgresql_cancel_transaction(struct st_database_connection * connection) {
	if (!connection)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	PGresult * result = PQexec(self->db_con, "ROLLBACK;");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql (cid #%ld): error while cancelling a transaction => %s", connection->id, PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

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
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "insert_pool", "INSERT INTO pool VALUES (DEFAULT, $1, $2, DEFAULT, DEFAULT, DEFAULT, $3)");

	char * tape_format = 0;
	asprintf(&tape_format, "%ld", pool->format->id);

	const char * param[] = { pool->uuid, pool->name, tape_format, };
	PGresult * result = PQexecPrepared(self->db_con, "insert_pool", 3, param, 0, 0, 0);
	if (PQresultStatus(result) == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	PQclear(result);
	free(tape_format);

	st_db_postgresql_get_pool(connection, pool, pool->uuid);

	return 0;
}

int st_db_postgresql_finish_transaction(struct st_database_connection * connection) {
	if (!connection)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	PGresult * result = PQexec(self->db_con, "COMMIT;");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql (cid #%ld): error while committing a transaction (%s)", connection->id, PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

char * st_db_postgresql_get_host(struct st_database_connection * connection) {
	struct st_db_postgresql_connetion_private * self = connection->data;

	struct utsname name;
	uname(&name);

	st_db_postgresql_prepare(self, "select_host_by_name", "SELECT id FROM host WHERE name = $1 OR name || '.' || domaine = $1 LIMIT 1");

	const char * params1[] = { name.nodename };
	PGresult * result = PQexecPrepared(self->db_con, "select_host_by_name", 1, params1, 0, 0, 0);

	if (PQresultStatus(result) == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		char * hostid = 0;
		st_db_postgresql_get_string_dup(result, 0, 0, &hostid);
		PQclear(result);
		return hostid;
	} else {
		PQclear(result);
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql (cid #%ld): Host not found into database (%s)", connection->id, name.nodename);
		return 0;
	}
}

int st_db_postgresql_get_nb_new_jobs(struct st_database_connection * connection, long * nb_new_jobs, time_t since, long last_max_jobs) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	char * hostid = st_db_postgresql_get_host(connection);
	if (!hostid)
		return 1;

	st_db_postgresql_prepare(self, "select_nb_new_jobs", "SELECT COUNT(*) AS total FROM job WHERE id > $1 AND update > $2 AND host = $3");

	char csince[24], * lastmaxjobs = 0;
	struct tm tm_since;
	localtime_r(&since, &tm_since);
	strftime(csince, 23, "%F %T", &tm_since);

	asprintf(&lastmaxjobs, "%ld", last_max_jobs);

	const char * param[] = { lastmaxjobs, csince, hostid };
	PGresult * result = PQexecPrepared(self->db_con, "select_nb_new_jobs", 3, param, 0, 0, 0);

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, nb_new_jobs);
		PQclear(result);
		free(lastmaxjobs);
		free(hostid);
		return 0;
	}

	PQclear(result);
	free(lastmaxjobs);
	free(hostid);
	return 1;
}

int st_db_postgresql_get_new_jobs(struct st_database_connection * connection, struct st_job ** jobs, unsigned int nb_jobs, time_t since, long last_max_jobs) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	char * hostid = st_db_postgresql_get_host(connection);
	if (!hostid)
		return 1;

	st_db_postgresql_prepare(self, "select_new_jobs", "SELECT j.*, jt.name FROM job j, jobtype jt WHERE j.id > $1 AND j.update > $2 AND j.host = $3 AND j.type = jt.id LIMIT $4");
	st_db_postgresql_prepare(self, "select_num_runs", "SELECT MAX(numrun) AS max FROM jobrecord WHERE job = $1");
	st_db_postgresql_prepare(self, "select_paths", "SELECT path FROM selectedfile WHERE id IN (SELECT selectedfile FROM jobtoselectedfile WHERE job = $1)");
	st_db_postgresql_prepare(self, "select_checksums", "SELECT name FROM checksum WHERE id IN (SELECT checksum FROM jobtochecksum WHERE job = $1)");

	char csince[24], * lastmaxjobs = 0, * nbjobs = 0;
	struct tm tm_since;
	localtime_r(&since, &tm_since);
	strftime(csince, 23, "%F %T", &tm_since);

	asprintf(&lastmaxjobs, "%ld", last_max_jobs);
	asprintf(&nbjobs, "%u", nb_jobs);

	const char * param[] = { lastmaxjobs, csince, hostid, nbjobs };
	PGresult * result = PQexecPrepared(self->db_con, "select_new_jobs", 4, param, 0, 0, 0);

	if (PQresultStatus(result) != PGRES_TUPLES_OK) {
		PQclear(result);
		free(nbjobs);
		free(lastmaxjobs);
		free(hostid);
		return 1;
	}

	int i;
	for (i = 0; i < PQntuples(result); i++) {
		char * jobid = 0, * poolid = 0;
		long userid;

		jobs[i] = malloc(sizeof(struct st_job));

		st_db_postgresql_get_long(result, i, 0, &jobs[i]->id);
		st_db_postgresql_get_string_dup(result, i, 0, &jobid);
		st_db_postgresql_get_string_dup(result, i, 1, &jobs[i]->name);
		jobs[i]->db_status = st_job_string_to_status(PQgetvalue(result, i, 4));
		st_db_postgresql_get_time(result, i, 12, &jobs[i]->updated);

		st_db_postgresql_get_time(result, i, 5, &jobs[i]->start);
		jobs[i]->interval = 0;
		st_db_postgresql_get_long(result, i, 6, &jobs[i]->interval);
		st_db_postgresql_get_long(result, i, 7, &jobs[i]->repetition);
		st_db_postgresql_get_long(result, i, 9, &userid);
		st_db_postgresql_get_string_dup(result, i, 10, &poolid);
		jobs[i]->num_runs = 0;

		const char * param2[] = { jobid };
		PGresult * result2 = PQexecPrepared(self->db_con, "select_num_runs", 1, param2, 0, 0, 0);

		if (PQresultStatus(result2) == PGRES_TUPLES_OK && PQntuples(result2) == 1 && !PQgetisnull(result2, 0, 0))
			st_db_postgresql_get_long(result2, 0, 0, &jobs[i]->num_runs);
		PQclear(result2);

		jobs[i]->user = st_user_get(userid, 0);

		if (poolid) {
			st_db_postgresql_prepare(self, "select_pool_by_id", "SELECT * FROM pool WHERE id = $1 LIMIT 1");
			st_db_postgresql_prepare(self, "select_tapeformat_by_id", "SELECT * FROM tapeformat WHERE id = $1 LIMIT 1");

			const char * param3[] = { poolid };
			result2 = PQexecPrepared(self->db_con, "select_pool_by_id", 1, param3, 0, 0, 0);

			char * pool_uuid = 0, * tapeformat = 0;
			unsigned char densitycode;

			if (PQresultStatus(result2) == PGRES_TUPLES_OK && PQntuples(result2) == 1) {
				st_db_postgresql_get_string_dup(result2, 0, 1, &pool_uuid);
				st_db_postgresql_get_string_dup(result2, 0, 6, &tapeformat);
			}
			PQclear(result2);

			const char * param4[] = { tapeformat };
			result2 = PQexecPrepared(self->db_con, "select_tapeformat_by_id", 1, param4, 0, 0, 0);
			if (PQresultStatus(result2) == PGRES_TUPLES_OK && PQntuples(result2) == 1) {
				st_db_postgresql_get_uchar(result2, 0, 11, &densitycode);

				jobs[i]->pool = st_pool_get_pool_by_uuid(pool_uuid, 0, densitycode);
			}
			PQclear(result2);

			free(tapeformat);
			free(pool_uuid);
		}

		result2 = PQexecPrepared(self->db_con, "select_paths", 1, param2, 0, 0, 0);
		if (PQresultStatus(result2) == PGRES_TUPLES_OK) {
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
		if (PQresultStatus(result2) == PGRES_TUPLES_OK) {
			jobs[i]->nb_checksums = PQntuples(result2);
			jobs[i]->checksums = 0;

			if (jobs[i]->nb_checksums > 0) {
				jobs[i]->checksums = calloc(jobs[i]->nb_checksums, sizeof(char *));

				unsigned int j;
				for (j = 0; j < jobs[i]->nb_checksums; j++)
					st_db_postgresql_get_string_dup(result2, j, 0, jobs[i]->checksums + j);
			}
		}
		PQclear(result2);

		jobs[i]->driver = st_job_get_driver(PQgetvalue(result, i, 13));

		if (poolid)
			free(poolid);
		free(jobid);
	}

	PQclear(result);
	free(nbjobs);
	free(lastmaxjobs);
	free(hostid);

	return 0;
}

int st_db_postgresql_get_pool(struct st_database_connection * connection, struct st_pool * pool, const char * uuid) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_pool_by_uuid", "SELECT * FROM pool WHERE uuid = $1 LIMIT 1");

	const char * param[] = { uuid };
	PGresult * result = PQexecPrepared(self->db_con, "select_pool_by_uuid", 1, param, 0, 0, 0);

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &pool->id);
		st_db_postgresql_get_string(result, 0, 1, pool->uuid);
		st_db_postgresql_get_string(result, 0, 2, pool->name);
		st_db_postgresql_get_long(result, 0, 3, &pool->retention);
		st_db_postgresql_get_time(result, 0, 4, &pool->retention_limit);
		st_db_postgresql_get_uchar(result, 0, 5, &pool->auto_recycle);
		pool->format = 0;

		PQclear(result);
		return 0;
	}

	PQclear(result);
	return 1;
}

int st_db_postgresql_get_tape_format(struct st_database_connection * connection, struct st_tape_format * tape_format, unsigned char density_code) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_tapeformat_by_densitycode", "SELECT * FROM tapeformat WHERE densityCode = $1 LIMIT 1");

	char * densitycode = 0;
	asprintf(&densitycode, "%d", density_code);
	const char * params[] = { densitycode };
	PGresult * result = PQexecPrepared(self->db_con, "select_tapeformat_by_densitycode", 1, params, 0, 0, 0);

	free(densitycode);

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &tape_format->id);
		st_db_postgresql_get_string(result, 0, 1, tape_format->name);
		tape_format->density_code = density_code;
		tape_format->type = st_tape_string_to_format_data(PQgetvalue(result, 0, 2));
		tape_format->mode = st_tape_string_to_format_mode(PQgetvalue(result, 0, 3));
		st_db_postgresql_get_long(result, 0, 4, &tape_format->max_load_count);
		st_db_postgresql_get_long(result, 0, 5, &tape_format->max_read_count);
		st_db_postgresql_get_long(result, 0, 6, &tape_format->max_write_count);
		st_db_postgresql_get_long(result, 0, 7, &tape_format->max_op_count);
		st_db_postgresql_get_long(result, 0, 8, &tape_format->life_span);
		st_db_postgresql_get_ssize(result, 0, 9, &tape_format->capacity);
		st_db_postgresql_get_ssize(result, 0, 10, &tape_format->block_size);
		st_db_postgresql_get_bool(result, 0, 11, &tape_format->support_partition);

		PQclear(result);
		return 0;
	}

	PQclear(result);
	return 1;
}

int st_db_postgresql_get_user(struct st_database_connection * connection, struct st_user * user, long user_id, const char * login) {
	if (user_id < 0 && !login)
		return 1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_user_by_id_or_login", "SELECT * FROM users WHERE id = $1 OR login = $2 LIMIT 1");

	char * userid = 0;
	asprintf(&userid, "%ld", user_id);

	const char * params[] = { userid, login };
	PGresult * result = PQexecPrepared(self->db_con, "select_user_by_id_or_login", 2, params, 0, 0, 0);

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_long(result, 0, 0, &user->id);
		st_db_postgresql_get_string_dup(result, 0, 1, &user->login);
		st_db_postgresql_get_string_dup(result, 0, 2, &user->password);
		st_db_postgresql_get_string_dup(result, 0, 3, &user->salt);
		st_db_postgresql_get_string_dup(result, 0, 4, &user->fullname);
		st_db_postgresql_get_string_dup(result, 0, 5, &user->email);
		st_db_postgresql_get_bool(result, 0, 6, &user->is_admin);
		st_db_postgresql_get_bool(result, 0, 7, &user->can_archive);
		st_db_postgresql_get_bool(result, 0, 8, &user->can_restore);

		PQclear(result);
		free(userid);
		return 0;
	} else {
		PQclear(result);
		free(userid);
		return 1;
	}
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

	const char * params[] = { drive->model, drive->vendor, drive->serial_number };
	PGresult * result = PQexecPrepared(self->db_con, "select_changer_of_drive_by_model_vendor_serialnumber", 3, params, 0, 0, 0);

	char * changerid = 0;
	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_string_dup(result, 0, 0, &changerid);
		PQclear(result);
	} else {
		PQclear(result);
		return 0;
	}

	st_db_postgresql_prepare(self, "select_changer_by_model_vendor_serialnumber_id", "SELECT id FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 AND id = $4 LIMIT 1");

	const char * params2[] = { changer->model, changer->vendor, changer->serial_number, changerid };
	result = PQexecPrepared(self->db_con, "select_changer_by_model_vendor_serialnumber_id", 4, params2, 0, 0, 0);

	int ok = PQntuples(result) == 1;

	PQclear(result);
	free(changerid);

	return ok;
}

int st_db_postgresql_refresh_job(struct st_database_connection * connection, struct st_job * job) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_job_since_by_id", "SELECT * FROM job WHERE id = $1 AND update >= $2 LIMIT 1");

	char * jobid = 0;
	asprintf(&jobid, "%ld", job->id);

	char updated[24];
	struct tm tm_updated;
	localtime_r(&job->updated, &tm_updated);
	strftime(updated, 23, "%F %T", &tm_updated);

	const char * param[] = { jobid, updated };
	PGresult * result = PQexecPrepared(self->db_con, "select_job_since_by_id", 2, param, 0, 0, 0);

	ExecStatusType status = PQresultStatus(result);
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);

	if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		job->db_status = st_job_string_to_status(PQgetvalue(result, 0, 4));
		st_db_postgresql_get_time(result, 0, 12, &job->updated);
		st_db_postgresql_get_time(result, 0, 5, &job->start);

		PQclear(result);
		free(jobid);
		return 0;
	}
	PQclear(result);

	st_db_postgresql_prepare(self, "select_job_by_id", "SELECT * FROM job WHERE id = $1 LIMIT 1");
	result = PQexecPrepared(self->db_con, "select_job_since_by_id", 1, param, 0, 0, 0);

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) != 1)
		job->id = -1;

	PQclear(result);
	free(jobid);
	return 0;
}

// TODO: check this function
int st_db_postgresql_start_transaction(struct st_database_connection * connection, short readOnly) {
	if (!connection)
		return -1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	PGresult * result = PQexec(self->db_con, readOnly ? "BEGIN ISOLATION LEVEL SERIALIZABLE" : "BEGIN");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql (cid #%ld): error while starting a transaction (%s)", connection->id, PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

int st_db_postgresql_sync_changer(struct st_database_connection * connection, struct st_changer * changer) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	char * hostid = st_db_postgresql_get_host(connection);

	char * changerid = 0;
	if (changer->id < 0) {
		st_db_postgresql_prepare(self, "select_changer_by_model_vendor_serialnumber", "SELECT id FROM changer WHERE model = $1 AND vendor = $2 AND serialnumber = $3 LIMIT 1");

		const char * params2[] = { changer->model, changer->vendor, changer->serial_number };
		PGresult * result = PQexecPrepared(self->db_con, "select_changer_by_model_vendor_serialnumber", 3, params2, 0, 0, 0);

		if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			st_db_postgresql_get_long(result, 0, 0, &changer->id);
			st_db_postgresql_get_string_dup(result, 0, 0, &changerid);
		}

		PQclear(result);
	}

	if (changer->id < 0) {
		st_db_postgresql_prepare(self, "insert_changer", "INSERT INTO changer VALUES (DEFAULT, $1, $2, $3, $4, $5, $6, $7, $8)");

		const char * params2[] = {
			changer->device, st_changer_status_to_string(changer->status), changer->barcode ? "true" : "false",
			changer->model, changer->vendor, changer->revision, changer->serial_number, hostid
		};

		PGresult * result = PQexecPrepared(self->db_con, "insert_changer", 8, params2, 0, 0, 0);
		if (PQresultStatus(result) == PGRES_FATAL_ERROR) {
			st_db_postgresql_get_error(result);
			free(result);
			free(hostid);
			return -1;
		}
		PQclear(result);

		st_db_postgresql_prepare(self, "select_changer_by_model_vendor_host", "SELECT id FROM changer WHERE model = $1 AND vendor = $2 AND host = $3 LIMIT 1");

		const char * params3[] = { changer->model, changer->vendor, hostid };
		result = PQexecPrepared(self->db_con, "select_changer_by_model_vendor_host", 3, params3, 0, 0, 0);

		if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &changer->id);

		if (changer->id < 0)
			st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Postgresql (cid #%ld): An expected probleme occured: failed to retreive changer id after insert it", connection->id);

		PQclear(result);
	} else {
		st_db_postgresql_prepare(self, "update_changer", "UPDATE changer SET device = $1, status = $2, firmwarerev = $3 WHERE id = $4");

		const char * params2[] = { changer->device, st_changer_status_to_string(changer->status), changer->revision, changerid };
		PGresult * result = PQexecPrepared(self->db_con, "update_changer", 4, params2, 0, 0, 0);

		if (PQresultStatus(result) == PGRES_FATAL_ERROR) {
			st_db_postgresql_get_error(result);
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
		st_db_postgresql_sync_drive(connection, changer->drives + i);

	for (i = changer->nb_drives; i < changer->nb_slots; i++)
		st_db_postgresql_sync_slot(connection, changer->slots + i);

	return 0;
}

int st_db_postgresql_sync_drive(struct st_database_connection * connection, struct st_drive * drive) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	if (drive->id < 0) {
		st_db_postgresql_prepare(self, "select_drive_by_model_vendor_serialnumber", "SELECT id, operationduration, lastclean, driveformat FROM drive WHERE model = $1 AND vendor = $2 AND serialnumber = $3 LIMIT 1");

		const char * params[] = { drive->model, drive->vendor, drive->serial_number };
		PGresult * result = PQexecPrepared(self->db_con, "select_drive_by_model_vendor_serialnumber", 3, params, 0, 0, 0);

		if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
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
	}

	char * changerid = 0;
	if (drive->changer)
		asprintf(&changerid, "%ld", drive->changer->id);

	if (drive->id < 0) {
		st_db_postgresql_prepare(self, "select_driveformat_by_densitycode", "SELECT id FROM driveformat WHERE densitycode = $1 LIMIT 1");

		char * densitycode = 0;
		asprintf(&densitycode, "%u", drive->best_density_code);

		const char * params1[] = { densitycode };
		PGresult * result = PQexecPrepared(self->db_con, "select_driveformat_by_densitycode", 1, params1, 0, 0, 0);

		char * driveformat_id = 0;
		ExecStatusType status = PQresultStatus(result);
		if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_string_dup(result, 0, 0, &driveformat_id);
		PQclear(result);

		st_db_postgresql_prepare(self, "insert_drive", "INSERT INTO drive VALUES (DEFAULT, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)");

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

		const char * params2[] = {
			drive->device, drive->scsi_device, st_drive_status_to_string(drive->status), changer_num, op_duration, last_clean,
			drive->model, drive->vendor, drive->revision, drive->serial_number, changerid, driveformat_id
		};
		result = PQexecPrepared(self->db_con, "insert_drive", 12, params2, 0, 0, 0);

		if (PQresultStatus(result) == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);

		PQclear(result);
		free(op_duration);
		free(changer_num);
		if (driveformat_id)
			free(driveformat_id);
		free(densitycode);
	} else {
		st_db_postgresql_prepare(self, "select_driveformat_by_densitycode", "SELECT id FROM driveformat WHERE densitycode = $1 LIMIT 1");

		char * densitycode = 0;
		asprintf(&densitycode, "%u", drive->best_density_code);

		const char * params1[] = { densitycode };
		PGresult * result = PQexecPrepared(self->db_con, "select_driveformat_by_densitycode", 1, params1, 0, 0, 0);

		char * driveformat_id = 0;
		ExecStatusType status = PQresultStatus(result);
		if (status == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_string_dup(result, 0, 0, &driveformat_id);
		PQclear(result);

		st_db_postgresql_prepare(self, "update_drive", "UPDATE drive SET device = $1, scsidevice = $2, status = $3, operationduration = $4, lastclean = $5, firmwarerev = $6, driveformat = $7 WHERE id = $8");

		char * driveid = 0, * op_duration = 0;
		asprintf(&driveid, "%ld", drive->id);
		asprintf(&op_duration, "%.3f", drive->operation_duration);

		char last_clean[24];
		struct tm tm_last_clean;
		localtime_r(&drive->last_clean, &tm_last_clean);
		strftime(last_clean, 23, "%F %T", &tm_last_clean);

		const char * params[] = {
			drive->device, drive->scsi_device, st_drive_status_to_string(drive->status),
			op_duration, last_clean, drive->revision, driveformat_id, driveid
		};
		result = PQexecPrepared(self->db_con, "update_drive", 8, params, 0, 0, 0);

		if (PQresultStatus(result) == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);

		PQclear(result);
		free(op_duration);
		free(driveid);
		if (driveformat_id)
			free(driveformat_id);
		free(densitycode);
	}

	if (drive->changer)
		free(changerid);

	if (drive->slot)
		st_db_postgresql_sync_slot(connection, drive->slot);

	return 0;
}

int st_db_postgresql_sync_plugin_checksum(struct st_database_connection * connection, const char * plugin) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_checksum_by_name", "SELECT name FROM checksum WHERE name = $1 LIMIT 1");

	const char * params[] = { plugin };
	PGresult * result = PQexecPrepared(self->db_con, "select_checksum_by_name", 1, params, 0, 0, 0);

	int found = PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1;
	PQclear(result);

	if (found)
		return 0;

	st_db_postgresql_prepare(self, "insert_checksum", "INSERT INTO checksum VALUES (DEFAULT, $1)");
	result = PQexecPrepared(self->db_con, "insert_checksum", 1, params, 0, 0, 0);
	ExecStatusType status = PQresultStatus(result);
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);
	PQclear(result);

	return status != PGRES_COMMAND_OK;
}

int st_db_postgresql_sync_pool(struct st_database_connection * connection, struct st_pool * pool) {
	if (pool->id < 0)
		return 1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_pool_by_id", "SELECT * FROM pool WHERE id = $1 LIMIT 1");

	char * poolid = 0;
	asprintf(&poolid, "%ld", pool->id);

	const char * param[] = { poolid };
	PGresult * result = PQexecPrepared(self->db_con, "select_pool_by_id", 1, param, 0, 0, 0);

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
		st_db_postgresql_get_string(result, 0, 2, pool->name);
		st_db_postgresql_get_long(result, 0, 3, &pool->retention);
		st_db_postgresql_get_time(result, 0, 4, &pool->retention_limit);
		st_db_postgresql_get_uchar(result, 0, 5, &pool->auto_recycle);

		PQclear(result);
		return 0;
	}

	PQclear(result);
	free(poolid);
	return 1;
}

int st_db_postgresql_sync_slot(struct st_database_connection * connection, struct st_slot * slot) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	if (slot->tape && st_db_postgresql_sync_tape(connection, slot->tape))
		return 1;

	if (slot->id < 0) {
		char * changer_id = 0, * slot_index = 0;
		asprintf(&changer_id, "%ld", slot->changer->id);
		asprintf(&slot_index, "%td", slot - slot->changer->slots);

		st_db_postgresql_prepare(self, "select_slot_by_index_changer", "SELECT id FROM changerslot WHERE index = $1 AND changer = $2 LIMIT 1");

		const char * params[] = { slot_index, changer_id };
		PGresult * result = PQexecPrepared(self->db_con, "select_slot_by_index_changer", 2, params, 0, 0, 0);

		if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
			st_db_postgresql_get_long(result, 0, 0, &slot->id);

		PQclear(result);
		free(slot_index);
		free(changer_id);
	}

	if (slot->id > -1) {
		char * slot_id = 0, * tape_id = 0;
		asprintf(&slot_id, "%ld", slot->id);
		if (slot->tape && slot->tape->id > -1)
			asprintf(&tape_id, "%ld", slot->tape->id);

		st_db_postgresql_prepare(self, "update_slot", "UPDATE changerslot SET tape = $1 WHERE id = $2");

		const char * params[] = { tape_id, slot_id };
		PGresult * result = PQexecPrepared(self->db_con, "update_slot", 2, params, 0, 0, 0);

		ExecStatusType status = PQresultStatus(result);
		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);

		PQclear(result);
		free(slot_id);
		if (tape_id)
			free(tape_id);

		return status != PGRES_COMMAND_OK;
	} else {
		char * changer_id = 0, * slot_index = 0, * tape_id = 0;
		asprintf(&changer_id, "%ld", slot->changer->id);
		asprintf(&slot_index, "%td", slot - slot->changer->slots);
		if (slot->tape && slot->tape->id > -1)
			asprintf(&tape_id, "%ld", slot->tape->id);

		st_db_postgresql_prepare(self, "insert_slot", "INSERT INTO changerslot VALUES (DEFAULT, $1, $2, $3, $4)");

		char * type = "default";
		if (slot->drive)
			type = "drive";
		else if (slot->is_import_export_slot)
			type = "import / export";

		const char * params[] = { slot_index, changer_id, tape_id, type };
		PGresult * result = PQexecPrepared(self->db_con, "insert_slot", 4, params, 0, 0, 0);

		ExecStatusType status = PQresultStatus(result);
		if (status == PGRES_FATAL_ERROR)
			st_db_postgresql_get_error(result);

		PQclear(result);

		if (status == PGRES_COMMAND_OK) {
			const char * params2[] = { slot_index, changer_id };
			result = PQexecPrepared(self->db_con, "select_slot_by_index_changer", 2, params2, 0, 0, 0);
			status = PQresultStatus(result);

			if (status == PGRES_COMMAND_OK)
				st_db_postgresql_get_long(result, 0, 0, &slot->id);

			PQclear(result);
		}

		if (tape_id)
			free(tape_id);
		free(slot_index);
		free(changer_id);

		return status != PGRES_COMMAND_OK;
	}
}

int st_db_postgresql_sync_tape(struct st_database_connection * connection, struct st_tape * tape) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	if (tape->id < 0) {
		if (tape->uuid[0] != '\0') {
			st_db_postgresql_prepare(self, "select_tape_by_uuid", "SELECT * FROM tape WHERE uuid = $1 LIMIT 1");

			const char * params[] = { tape->uuid };
			PGresult * result = PQexecPrepared(self->db_con, "select_tape_by_uuid", 1, params, 0, 0, 0);

			if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
				st_db_postgresql_get_long(result, 0, 0, &tape->id);
				st_db_postgresql_get_string(result, 0, 2, tape->label);
				st_db_postgresql_get_string(result, 0, 3, tape->name);
				st_db_postgresql_get_time(result, 0, 6, &tape->first_used);
				st_db_postgresql_get_time(result, 0, 7, &tape->use_before);

				long old_value = tape->load_count;
				st_db_postgresql_get_long(result, 0, 8, &tape->load_count);
				tape->load_count += old_value;

				old_value = tape->read_count;
				st_db_postgresql_get_long(result, 0, 9, &tape->read_count);
				tape->read_count += old_value;

				old_value = tape->write_count;
				st_db_postgresql_get_long(result, 0, 10, &tape->write_count);
				tape->write_count += old_value;

				st_db_postgresql_get_ssize(result, 0, 13, &tape->block_size);
			}

			PQclear(result);
		}

		if (tape->id < 0 && tape->label[0] != '\0') {
			st_db_postgresql_prepare(self, "select_tape_by_label", "SELECT * FROM tape WHERE label = $1 LIMIT 1");

			const char * params[] = { tape->label };
			PGresult * result = PQexecPrepared(self->db_con, "select_tape_by_label", 1, params, 0, 0, 0);

			if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
				st_db_postgresql_get_long(result, 0, 0, &tape->id);
				st_db_postgresql_get_string(result, 0, 3, tape->name);
				st_db_postgresql_get_time(result, 0, 6, &tape->first_used);
				st_db_postgresql_get_time(result, 0, 7, &tape->use_before);

				long old_value = tape->load_count;
				st_db_postgresql_get_long(result, 0, 8, &tape->load_count);
				tape->load_count += old_value;

				old_value = tape->read_count;
				st_db_postgresql_get_long(result, 0, 9, &tape->read_count);
				tape->read_count += old_value;

				old_value = tape->write_count;
				st_db_postgresql_get_long(result, 0, 10, &tape->write_count);
				tape->write_count += old_value;

				st_db_postgresql_get_ssize(result, 0, 13, &tape->block_size);
			}

			PQclear(result);
		}
	}

	if (tape->uuid[0] != '\0' || tape->label[0] != '\0') {
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

			const char * params[] = {
				*tape->uuid ? tape->uuid : 0, tape->name, st_tape_status_to_string(tape->status),
				st_tape_location_to_string(tape->location),
				load, read, write, endpos, nbfiles, blocksize,
				tape->has_partition ? "true" : "false", pool, id
			};
			PGresult * result = PQexecPrepared(self->db_con, "update_tape", 13, params, 0, 0, 0);
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

			return status != PGRES_COMMAND_OK;
		} else {
			st_db_postgresql_prepare(self, "insert_tape", "INSERT INTO tape VALUES (DEFAULT, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16)");

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

			const char * params[] = {
				*tape->uuid ? tape->uuid : 0, tape->label, tape->name,
				st_tape_status_to_string(tape->status),
				st_tape_location_to_string(tape->location),
				buffer_first_used, buffer_use_before,
				load, read, write, endpos, nbfiles, blocksize,
				tape->has_partition ? "true" : "false", tapeformat, pool
			};
			PGresult * result = PQexecPrepared(self->db_con, "insert_tape", 16, params, 0, 0, 0);
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
			free(tapeformat);
			if (pool)
				free(pool);

			if (status == PGRES_FATAL_ERROR)
				return status != PGRES_COMMAND_OK;

			if (*tape->label) {
				st_db_postgresql_prepare(self, "select_tape_id_by_label", "SELECT id FROM tape WHERE label = $1 LIMIT 1");

				const char * params[] = { tape->label };
				PGresult * result = PQexecPrepared(self->db_con, "select_tape_id_by_label", 1, params, 0, 0, 0);

				if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
					st_db_postgresql_get_long(result, 0, 0, &tape->id);

				PQclear(result);
			} else {
				st_db_postgresql_prepare(self, "select_tape_id_by_name", "SELECT id FROM tape WHERE name = $1 LIMIT 1");

				const char * params[] = { tape->name };
				PGresult * result = PQexecPrepared(self->db_con, "select_tape_by_name", 1, params, 0, 0, 0);

				if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
					st_db_postgresql_get_long(result, 0, 0, &tape->id);

				PQclear(result);
			}

			return status != PGRES_COMMAND_OK;
		}
	}

	return 0;
}

int st_db_postgresql_sync_user(struct st_database_connection * connection, struct st_user * user) {
	if (!user || user->id < 0)
		return 1;

	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "select_user_by_id", "SELECT * FROM users WHERE id = $1 LIMIT 1");

	char * userid = 0;
	asprintf(&userid, "%ld", user->id);

	const char * params[] = { userid };
	PGresult * result = PQexecPrepared(self->db_con, "select_user_by_id", 1, params, 0, 0, 0);

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1) {
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

		PQclear(result);
		free(userid);
		return 0;
	} else {
		PQclear(result);
		free(userid);
		return 1;
	}
}

int st_db_postgresql_update_job(struct st_database_connection * connection, struct st_job * job) {
	struct st_db_postgresql_connetion_private * self = connection->data;
	st_db_postgresql_check(connection);

	st_db_postgresql_prepare(self, "update_job", "UPDATE job SET done = $1, status = $2, repetition = $3, update = NOW() WHERE id = $4");

	char * cdone = 0, * crepetition = 0, * jobid = 0;
	asprintf(&cdone, "%f", job->done);
	asprintf(&crepetition, "%ld", job->repetition);
	asprintf(&jobid, "%ld", job->id);

	const char * params[] = { cdone, st_job_status_to_string(job->sched_status), crepetition, jobid };
	PGresult * result = PQexecPrepared(self->db_con, "update_job", 4, params, 0, 0, 0);

	ExecStatusType status = PQresultStatus(result);
	if (status == PGRES_FATAL_ERROR)
		st_db_postgresql_get_error(result);

	PQclear(result);
	free(jobid);
	free(crepetition);
	free(cdone);

	return status != PGRES_COMMAND_OK;
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

/*int st_db_postgresql_get_int(PGresult * result, int row, int column, int * val) {
  if (column < 0)
  return -1;

  char * value = PQgetvalue(result, row, column);
  if (value && sscanf(value, "%d", val) == 1)
  return 0;
  return -1;
  }*/

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

