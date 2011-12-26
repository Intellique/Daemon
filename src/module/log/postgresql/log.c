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
*  Last modified: Mon, 26 Dec 2011 12:02:55 +0100                         *
\*************************************************************************/

// free, malloc
#include <malloc.h>
// strdup
#include <string.h>
// gettimeofday
#include <sys/time.h>
// uname
#include <sys/utsname.h>
// localtime_r, strftime
#include <time.h>

#include "common.h"

struct st_log_postgresql_private {
	PGconn * connection;
	char * hostid;
};

static void st_log_postgresql_module_free(struct st_log_module * module);
static void st_log_postgresql_module_write(struct st_log_module * module, enum st_log_level level, enum st_log_type type, const char * message);

static struct st_log_module_ops st_log_postgresql_module_ops = {
	.free  = st_log_postgresql_module_free,
	.write = st_log_postgresql_module_write,
};

static const char * st_log_postgresql_levels[] = {
	"error", "warning", "info", "debug",
};

static const char * st_log_postgresql_types[] = {
	"changer", "checksum", "daemon", "database", "drive", "job", "plugin checksum", "plugin db",
	"plugin log", "scheduler", "ui", "user message",
};


void st_log_postgresql_module_free(struct st_log_module * module) {
	struct st_log_postgresql_private * self = module->data;

	PQfinish(self->connection);
	free(self);

	free(module->alias);
	module->ops = 0;
	module->data = 0;
}

struct st_log_module * st_log_postgresql_new(struct st_log_module * module, const char * alias, enum st_log_level level, const char * user, char * password, char * host, char * db, char * port) {
	PGconn * con = PQsetdbLogin(host, port, 0, 0, db, user, password);

	struct st_log_postgresql_private * self = malloc(sizeof(struct st_log_postgresql_private));
	self->connection = con;
	self->hostid = 0;

	module->alias = strdup(alias);
	module->level = level;
	module->data = self;
	module->ops = &st_log_postgresql_module_ops;

	struct utsname name;
	uname(&name);

	const char * param[] = { name.nodename };
	PGresult * result = PQexecParams(con, "SELECT id FROM host WHERE name = $1 LIMIT 1", 1, 0, param, 0, 0, 0);
	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
		self->hostid = strdup(PQgetvalue(result, 0, 0));
	PQclear(result);

	PGresult * prepare = PQprepare(con, "insert_log", "INSERT INTO log VALUES (DEFAULT, $1, $2, $3, $4, $5, NULL)", 0, 0);
	PQclear(prepare);

	return module;
}

void st_log_postgresql_module_write(struct st_log_module * module, enum st_log_level level, enum st_log_type type, const char * message) {
	struct st_log_postgresql_private * self = module->data;

	struct timeval curTime;
	struct tm curTime2;
	char buffer[32];

	gettimeofday(&curTime, 0);
	localtime_r(&(curTime.tv_sec), &curTime2);
	strftime(buffer, 32, "%F %T", &curTime2);

	const char * param[] = {
		st_log_postgresql_types[type], st_log_postgresql_levels[level],
		buffer, message, self->hostid,
	};

	PGresult * result = PQexecPrepared(self->connection, "insert_log", 5, param, 0, 0, 0);
	PQclear(result);
}

