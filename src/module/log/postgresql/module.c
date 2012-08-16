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
*  Last modified: Fri, 17 Aug 2012 00:11:50 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// free, malloc
#include <malloc.h>

#include <postgresql/libpq-fe.h>
// asprintf
#include <stdio.h>
// strdup
#include <string.h>
// gettimeofday
#include <sys/time.h>
// uname
#include <sys/utsname.h>
// localtime_r, strftime
#include <time.h>

#include <libstone/user.h>

#include "common.h"

struct st_log_postgresql_private {
	PGconn * connection;
	char * hostid;
};

static void st_log_postgresql_module_free(struct st_log_module * module);
static void st_log_postgresql_module_write(struct st_log_module * module, const struct st_log_message * message);

static struct st_log_module_ops st_log_postgresql_module_ops = {
	.free  = st_log_postgresql_module_free,
	.write = st_log_postgresql_module_write,
};

static const char * st_log_postgresql_levels[] = {
	[st_log_level_error]   = "error",
	[st_log_level_warning] = "warning",
	[st_log_level_info]    = "info",
	[st_log_level_debug]   = "debug",
};

static const char * st_log_postgresql_types[] = {
	[st_log_type_changer]         = "changer",
	[st_log_type_checksum]        = "checksum",
	[st_log_type_daemon]          = "daemon",
	[st_log_type_database]        = "database",
	[st_log_type_drive]           = "drive",
	[st_log_type_job]             = "job",
	[st_log_type_plugin_checksum] = "plugin checksum",
	[st_log_type_plugin_db]       = "plugin database",
	[st_log_type_plugin_log]      = "plugin log",
	[st_log_type_scheduler]       = "scheduler",
	[st_log_type_ui]              = "ui",
	[st_log_type_user_message]    = "user message",
};


static void st_log_postgresql_module_free(struct st_log_module * module) {
	struct st_log_postgresql_private * self = module->data;

	PQfinish(self->connection);
	free(self);

	free(module->alias);
	module->ops = NULL;
	module->data = NULL;
}

int st_log_postgresql_new(struct st_log_module * module, enum st_log_level level, const struct st_hashtable * params) {
	const char * alias = st_hashtable_value(params, "alias");
	const char * host = st_hashtable_value(params, "host");
	const char * port = st_hashtable_value(params, "port");
	const char * db = st_hashtable_value(params, "db");
	const char * user = st_hashtable_value(params, "user");
	const char * password = st_hashtable_value(params, "password");

	PGconn * connect = PQsetdbLogin(host, port, NULL, NULL, db, user, password);

	char * hostid = NULL;
	struct utsname name;
	uname(&name);

	const char * param[] = { name.nodename };
	PGresult * result = PQexecParams(connect, "SELECT id FROM host WHERE name = $1 OR name || '.' || domaine = $1 LIMIT 1", 1, NULL, param, NULL, NULL, 0);
	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
		hostid = strdup(PQgetvalue(result, 0, 0));
	PQclear(result);

	if (!hostid) {
		char * domaine = strchr(name.nodename, '.');
		if (domaine) {
			*domaine = '\0';
			domaine++;
		}

		const char * param2[] = { name.nodename, domaine };
		result = PQexecParams(connect, "INSERT INTO host(name, domaine) VALUES ($1, $2) RETURNING id", 2, NULL, param2, NULL, NULL, 0);
		ExecStatusType status = PQresultStatus(result);

		if (domaine) {
			domaine--;
			*domaine = '.';
		}

		if (status == PGRES_FATAL_ERROR) {
			st_log_write_all(st_log_level_error, st_log_type_plugin_log, "Error, while adding host (%s) to database", name.nodename);
			st_log_write_all(st_log_level_error, st_log_type_plugin_log, "Postgresql: error => %s", PQresultErrorField(result, PG_DIAG_MESSAGE_PRIMARY));

			PQclear(result);
			PQfinish(connect);

			return 1;
		} else if (status == PGRES_TUPLES_OK && PQntuples(result) == 1) {
			hostid = strdup(PQgetvalue(result, 0, 0));
		}

		PQclear(result);
	}

	PGresult * prepare = PQprepare(connect, "insert_log", "INSERT INTO log(type, level, time, message, host, login) VALUES ($1, $2, $3, $4, $5, $6)", 0, NULL);
	PQclear(prepare);

	prepare = PQprepare(connect, "select_user_by_login", "SELECT id FROM users WHERE login = $1 LIMIT 1", 0, NULL);
	PQclear(prepare);

	struct st_log_postgresql_private * self = malloc(sizeof(struct st_log_postgresql_private));
	self->connection = connect;
	self->hostid = hostid;

	module->alias = strdup(alias);
	module->level = level;
	module->data = self;
	module->ops = &st_log_postgresql_module_ops;

	return 0;
}

static void st_log_postgresql_module_write(struct st_log_module * module, const struct st_log_message * message) {
	struct st_log_postgresql_private * self = module->data;

	struct tm curTime2;
	char buffer[32];

	localtime_r(&message->timestamp, &curTime2);
	strftime(buffer, 32, "%F %T", &curTime2);

	char * userid = NULL;
	if (message->user) {
		const char * param[] = { message->user->login };
		PGresult * result = PQexecPrepared(self->connection, "select_user_by_login", 1, param, NULL, NULL, 0);

		if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
			userid = strdup(PQgetvalue(result, 0, 0));

		PQclear(result);
	}

	const char * param[] = {
		st_log_postgresql_types[message->type], st_log_postgresql_levels[message->level],
		buffer, message->message, self->hostid, userid,
	};

	PGresult * result = PQexecPrepared(self->connection, "insert_log", 6, param, NULL, NULL, 0);
	PQclear(result);

	free(userid);
}

