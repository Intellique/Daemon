/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2010, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Mon, 18 Oct 2010 18:44:02 +0200                       *
\***********************************************************************/

// free, malloc
#include <malloc.h>
// PQbackendPID, PQclear, PQerrorMessage, PQexec, PQfinish, PQgetvalue, PQntuples, PQsetdbLogin, PQresultStatus, PQstatus
#include <postgresql/libpq-fe.h>
// sscanf, snprintf
#include <stdio.h>

#include <storiqArchiver/log.h>

#include "connection.h"

static int db_postgresql_con_close(struct database_connection * connection);
static int db_postgresql_con_free(struct database_connection * connection);

static int db_postgresql_finishTransaction(struct database_connection * connection);
static int db_postgresql_startTransaction(struct database_connection * connection, short readOnly);

static int db_postgresql_newJobs(struct database_connection * connection, time_t since);

struct database_connection_ops db_postgresql_con_ops = {
	.close = db_postgresql_con_close,
	.free = db_postgresql_con_free,

	.finishTransaction = db_postgresql_finishTransaction,
	.startTransaction = db_postgresql_startTransaction,

	.newJobs = db_postgresql_newJobs,
};


int db_postgresql_con_close(struct database_connection * connection) {
	if (!connection)
		return -1;

	struct db_postgresql_connetion_private * self = connection->data;

	if (!self->db_con)
		return 0;

	PQfinish(self->db_con);

	self->db_con = 0;

	return 0;
}

int db_postgresql_con_free(struct database_connection * connection) {
	if (!connection)
		return -1;

	struct db_postgresql_connetion_private * self = connection->data;

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

int db_postgresql_finishTransaction(struct database_connection * connection) {
	if (!connection)
		return -1;

	struct db_postgresql_connetion_private * self = connection->data;

	PGresult * result = PQexec(self->db_con, "COMMIT;");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		log_writeAll(Log_level_error, "Db: Postgresql: error while committing transaction (%s)", PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

int db_postgresql_initConnection(struct database_connection * connection, struct db_postgresql_private * driver_private) {
	if (!connection || !connection->driver)
		return -1;

	PGconn * con = PQsetdbLogin(driver_private->host, driver_private->port, 0, 0, driver_private->db, driver_private->user, driver_private->password);
	ConnStatusType status = PQstatus(con);

	if (status == CONNECTION_BAD) {
		PQfinish(con);
		return -2;
	}

	connection->id = PQbackendPID(con);
	connection->ops = &db_postgresql_con_ops;

	struct db_postgresql_connetion_private * self = malloc(sizeof(struct db_postgresql_connetion_private));
	self->db_con = con;

	connection->data = self;

	return 0;
}

int db_postgresql_newJobs(struct database_connection * connection, time_t since) {
	if (!connection)
		return -1;

	char * query = malloc(128);
	snprintf(query, 128, "SELECT COUNT(job_id) FROM jobs WHERE job_modified > (TIMESTAMP 'epoch' + INTERVAL '%ld second');", since);

	struct db_postgresql_connetion_private * self = connection->data;
	PGresult * result = PQexec(self->db_con, query);

	int returned = 0;
	if (PQresultStatus(result) == PGRES_TUPLES_OK) {
		if (PQntuples(result) == 1) {
			char * cvalue = PQgetvalue(result, 0, 0);
			int ivalue = 0;
			if (sscanf(cvalue, "%d", &ivalue) == 1)
				returned = ivalue;
			else
				returned = -1;
		} else
			returned = -1;
	} else {
		log_writeAll(Log_level_error, "Db: Postgresql: error while computing new jobs (%s)", PQerrorMessage(self->db_con));
		returned = -1;
	}

	PQclear(result);
	free(query);

	return returned;
}

int db_postgresql_startTransaction(struct database_connection * connection, short readOnly) {
	if (!connection)
		return -1;

	struct db_postgresql_connetion_private * self = connection->data;

	PGresult * result = PQexec(self->db_con, readOnly ? "BEGIN READ ONLY;" : "BEGIN;");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		log_writeAll(Log_level_error, "Db: Postgresql: error while starting transaction (%s)", PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

