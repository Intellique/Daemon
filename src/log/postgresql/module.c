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

// free, malloc
#include <malloc.h>
// PQclear, PQexecParams, PQexecPrepared, PQfinish, PQgetvalue, PQntuples,
// PQprepare, PQreset, PQresultStatus, PQsetdbLogin, PQstatus
#include <postgresql/libpq-fe.h>
// strdup
#include <string.h>
// uname
#include <sys/utsname.h>
// sleep
#include <unistd.h>

#include <libstone/time.h>

#include "common.h"

struct st_log_postgresql_private {
	PGconn * connection;
	char * hostid;
};

static void st_log_postgresql_module_free(struct lgr_log_module * module);
static void st_log_postgresql_module_prepare_queries(PGconn * connection);
static void st_log_postgresql_module_write(struct lgr_log_module * module, struct st_value * message);

static struct lgr_log_module_ops st_log_postgresql_module_ops = {
	.free  = st_log_postgresql_module_free,
	.write = st_log_postgresql_module_write,
};

static const char * st_log_postgresql_levels[] = {
	[st_log_level_alert]      = "alert",
	[st_log_level_critical]   = "critical",
	[st_log_level_debug]      = "debug",
	[st_log_level_emergencey] = "emergency",
	[st_log_level_error]      = "error",
	[st_log_level_info]       = "info",
	[st_log_level_notice]     = "notice",
	[st_log_level_warning]    = "warning",
};

static const char * st_log_postgresql_types[] = {
	[st_log_type_changer]         = "changer",
	[st_log_type_daemon]          = "daemon",
	[st_log_type_drive]           = "drive",
	[st_log_type_job]             = "job",
	[st_log_type_logger]          = "logger",
	[st_log_type_plugin_checksum] = "plugin checksum",
	[st_log_type_plugin_db]       = "plugin database",
	[st_log_type_plugin_log]      = "plugin log",
	[st_log_type_scheduler]       = "scheduler",
	[st_log_type_ui]              = "ui",
	[st_log_type_user_message]    = "user message",
};


static void st_log_postgresql_module_free(struct lgr_log_module * module) {
	struct st_log_postgresql_private * self = module->data;

	PQfinish(self->connection);
	free(self->hostid);
	free(self);

	module->ops = NULL;
	module->data = NULL;
}

struct lgr_log_module * st_log_postgresql_new_module(struct st_value * params) {
	struct st_value * val_host = st_value_hashtable_get2(params, "host", false);
	struct st_value * val_port = st_value_hashtable_get2(params, "port", false);
	struct st_value * val_db = st_value_hashtable_get2(params, "db", false);
	struct st_value * val_user = st_value_hashtable_get2(params, "user", false);
	struct st_value * val_password = st_value_hashtable_get2(params, "password", false);
	struct st_value * level = st_value_hashtable_get2(params, "verbosity", false);

	const char * host = NULL;
	if (val_host->type == st_value_string)
		host = st_value_string_get(val_host);

	const char * port = NULL;
	if (val_port->type == st_value_string)
		port = st_value_string_get(val_port);

	const char * db = NULL;
	if (val_db->type == st_value_string)
		db = st_value_string_get(val_db);

	const char * user = NULL;
	if (val_user->type == st_value_string)
		user = st_value_string_get(val_user);

	const char * password = NULL;
	if (val_password->type == st_value_string)
		password = st_value_string_get(val_password);

	PGconn * connect = PQsetdbLogin(host, port, NULL, NULL, db, user, password);
	ConnStatusType status = PQstatus(connect);

	if (status == CONNECTION_BAD) {
		PQfinish(connect);
		return NULL;
	}

	char * hostid = NULL;
	struct utsname name;
	uname(&name);

	const char * param[] = { name.nodename };
	PGresult * result = PQexecParams(connect, "SELECT id FROM host WHERE name = $1 OR name || '.' || domaine = $1 LIMIT 1", 1, NULL, param, NULL, NULL, 0);
	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
		hostid = strdup(PQgetvalue(result, 0, 0));
	PQclear(result);

	if (hostid == NULL) {
		PQfinish(connect);
		return NULL;
	}

	PGresult * prepare = PQprepare(connect, "insert_log", "INSERT INTO log(type, level, time, message, host) VALUES ($1, $2, $3, $4, $5)", 0, NULL);
	PQclear(prepare);

	st_log_postgresql_module_prepare_queries(connect);

	struct st_log_postgresql_private * self = malloc(sizeof(struct st_log_postgresql_private));
	self->connection = connect;
	self->hostid = hostid;

	struct lgr_log_module * mod = malloc(sizeof(struct lgr_log_module));
	mod->level = st_log_string_to_level(st_value_string_get(level));
	mod->ops = &st_log_postgresql_module_ops;
	mod->data = self;

	return mod;
}

static void st_log_postgresql_module_prepare_queries(PGconn * connection) {
	PGresult * prepare = PQprepare(connection, "insert_log", "INSERT INTO log(type, level, time, message, host) VALUES ($1, $2, $3, $4, $5)", 0, NULL);
	PQclear(prepare);
}

static void st_log_postgresql_module_write(struct lgr_log_module * module, struct st_value * message) {
	struct st_log_postgresql_private * self = module->data;

	if (!st_value_hashtable_has_key2(message, "type") || !st_value_hashtable_has_key2(message, "message"))
		return;

	struct st_value * level = st_value_hashtable_get2(message, "level", false);
	if (level->type != st_value_string)
		return;

	struct st_value * vtype = st_value_hashtable_get2(message, "type", false);
	if (vtype->type != st_value_string)
		return;

	enum st_log_type type = st_log_string_to_type(st_value_string_get(vtype));
	if (type == st_log_type_unknown)
		return;

	struct st_value * vtimestamp = st_value_hashtable_get2(message, "timestamp", false);
	if (vtimestamp->type != st_value_integer)
		return;

	struct st_value * vmessage = st_value_hashtable_get2(message, "message", false);
	if (vmessage->type != st_value_string)
		return;

	char strtime[32];
	time_t timestamp = st_value_integer_get(vtimestamp);
	st_time_convert(&timestamp, "%F %T", strtime, 32);

	enum st_log_level lvl = st_log_string_to_level(st_value_string_get(level));
	enum st_log_type typ = st_log_string_to_type(st_value_string_get(vtype));

	const char * param[] = {st_log_postgresql_types[typ], st_log_postgresql_levels[lvl], strtime, st_value_string_get(vmessage), self->hostid};

	ExecStatusType qStatus = PGRES_FATAL_ERROR;
	unsigned short i;
	for (i = 0; i < 5 && qStatus != PGRES_COMMAND_OK; i++) {
		PGresult * result = PQexecPrepared(self->connection, "insert_log", 5, param, NULL, NULL, 0);
		qStatus = PQresultStatus(result);
		PQclear(result);

		ConnStatusType status = PQstatus(self->connection);
		if (qStatus != PGRES_COMMAND_OK || status != CONNECTION_OK) {
			sleep(i);
			PQreset(self->connection);
			st_log_postgresql_module_prepare_queries(self->connection);
		}
	}
}

