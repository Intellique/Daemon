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
*  Last modified: Wed, 23 Nov 2011 12:47:26 +0100                         *
\*************************************************************************/

#define _XOPEN_SOURCE 500
// free, malloc
#include <malloc.h>
// PQbackendPID, PQclear, PQerrorMessage, PQexec, PQfinish, PQfnumber, PQgetvalue, PQntuples, PQsetdbLogin, PQresultStatus, PQstatus
#include <postgresql/libpq-fe.h>
// sscanf, snprintf
#include <stdio.h>
// strcmp, strdup
#include <string.h>
// strptime
#include <time.h>
// sleep
#include <unistd.h>

#include <storiqArchiver/job.h>
#include <storiqArchiver/library.h>
#include <storiqArchiver/log.h>

#include "common.h"

static void sa_db_postgresql_check(struct sa_db_postgresql_connetion_private * self);
static int sa_db_postgresql_con_close(struct sa_database_connection * connection);
static int sa_db_postgresql_con_free(struct sa_database_connection * connection);

static int sa_db_postgresql_cancel_transaction(struct sa_database_connection * connection);
static int sa_db_postgresql_finish_transaction(struct sa_database_connection * connection);
static int sa_db_postgresql_start_transaction(struct sa_database_connection * connection, short read_only);

static int sa_db_postgresql_get_int(PGresult * result, int row, int column, int * value);
static int sa_db_postgresql_get_long(PGresult * result, int row, int column, long * value);
static int sa_db_postgresql_get_string(PGresult * result, int row, int column, char ** value);

static struct sa_database_connection_ops sa_db_postgresql_con_ops = {
	.close = sa_db_postgresql_con_close,
	.free  = sa_db_postgresql_con_free,

	.cancel_transaction = sa_db_postgresql_cancel_transaction,
	.finish_transaction = sa_db_postgresql_finish_transaction,
	.start_transaction  = sa_db_postgresql_start_transaction,
};


int sa_db_postgresql_cancel_transaction(struct sa_database_connection * connection) {
	if (!connection)
		return -1;

	struct sa_db_postgresql_connetion_private * self = connection->data;
	sa_db_postgresql_check(self);

	PGresult * result = PQexec(self->db_con, "ROLLBACK;");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		sa_log_write_all(sa_log_level_error, "Db: Postgresql: error while cancelling a transaction (%s)", PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}

void sa_db_postgresql_check(struct sa_db_postgresql_connetion_private * self) {
	if (PQstatus(self->db_con) != CONNECTION_OK) {
		int i;
		for (i = 0; i < 3; i++) {
			PQreset(self->db_con);
			if (PQstatus(self->db_con) != CONNECTION_OK) {
				sa_log_write_all(sa_log_level_error, "Db: Postgresql: Failed to reset database connection");
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

int sa_db_postgresql_finish_transaction(struct sa_database_connection * connection) {
	if (!connection)
		return -1;

	struct sa_db_postgresql_connetion_private * self = connection->data;
	sa_db_postgresql_check(self);

	PGresult * result = PQexec(self->db_con, "COMMIT;");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		sa_log_write_all(sa_log_level_error, "Db: Postgresql: error while committing a transaction (%s)", PQerrorMessage(self->db_con));

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
	sa_db_postgresql_check(self);

	PGresult * result = PQexec(self->db_con, readOnly ? "BEGIN READ ONLY;" : "BEGIN;");
	ExecStatusType status = PQresultStatus(result);

	if (status != PGRES_COMMAND_OK)
		sa_log_write_all(sa_log_level_error, "Db: Postgresql: error while starting a transaction (%s)", PQerrorMessage(self->db_con));

	PQclear(result);

	return status == PGRES_COMMAND_OK ? 0 : -1;
}


int sa_db_postgresql_get_int(PGresult * result, int row, int column, int * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%d", val) == 1)
		return 0;
	return -1;
}

int sa_db_postgresql_get_long(PGresult * result, int row, int column, long * val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value && sscanf(value, "%ld", val) == 1)
		return 0;
	return -1;
}

int sa_db_postgresql_get_string(PGresult * result, int row, int column, char ** val) {
	if (column < 0)
		return -1;

	char * value = PQgetvalue(result, row, column);
	if (value) {
		*val = strdup(value);
		return 0;
	}
	return -1;
}

