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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// free, malloc
#include <malloc.h>
// PQclear, PQexecParams, PQexecPrepared, PQfinish, PQgetvalue, PQntuples,
// PQprepare, PQreset, PQresultStatus, PQsetdbLogin, PQstatus
#include <libpq-fe.h>
// strdup
#include <string.h>
// uname
#include <sys/utsname.h>
// sleep
#include <unistd.h>

#include <libstoriqone/time.h>

#include "common.h"

struct so_log_postgresql_private {
	PGconn * connection;
	char * hostid;
};

static void so_log_postgresql_module_free(struct solgr_log_module * module);
static void so_log_postgresql_module_prepare_queries(PGconn * connection);
static void so_log_postgresql_module_write(struct solgr_log_module * module, struct so_value * message);

static struct solgr_log_module_ops so_log_postgresql_module_ops = {
	.free  = so_log_postgresql_module_free,
	.write = so_log_postgresql_module_write,
};

static const char * so_log_postgresql_levels[] = {
	[so_log_level_alert]      = "alert",
	[so_log_level_critical]   = "critical",
	[so_log_level_debug]      = "debug",
	[so_log_level_emergencey] = "emergency",
	[so_log_level_error]      = "error",
	[so_log_level_info]       = "info",
	[so_log_level_notice]     = "notice",
	[so_log_level_warning]    = "warning",
};

static const char * so_log_postgresql_types[] = {
	[so_log_type_changer]         = "changer",
	[so_log_type_daemon]          = "daemon",
	[so_log_type_drive]           = "drive",
	[so_log_type_job]             = "job",
	[so_log_type_logger]          = "logger",
	[so_log_type_plugin_checksum] = "plugin checksum",
	[so_log_type_plugin_db]       = "plugin db",
	[so_log_type_plugin_log]      = "plugin log",
	[so_log_type_scheduler]       = "scheduler",
	[so_log_type_ui]              = "ui",
	[so_log_type_user_message]    = "user message",
};


static void so_log_postgresql_module_free(struct solgr_log_module * module) {
	struct so_log_postgresql_private * self = module->data;

	PQfinish(self->connection);
	free(self->hostid);
	free(self);

	module->ops = NULL;
	module->data = NULL;
}

struct solgr_log_module * so_log_postgresql_new_module(struct so_value * params) {
	struct so_value * val_host = so_value_hashtable_get2(params, "host", false, false);
	struct so_value * val_port = so_value_hashtable_get2(params, "port", false, false);
	struct so_value * val_db = so_value_hashtable_get2(params, "db", false, false);
	struct so_value * val_user = so_value_hashtable_get2(params, "user", false, false);
	struct so_value * val_password = so_value_hashtable_get2(params, "password", false, false);
	struct so_value * level = so_value_hashtable_get2(params, "verbosity", false, false);

	const char * host = NULL;
	if (val_host->type == so_value_string)
		host = so_value_string_get(val_host);

	const char * port = NULL;
	if (val_port->type == so_value_string)
		port = so_value_string_get(val_port);

	const char * db = NULL;
	if (val_db->type == so_value_string)
		db = so_value_string_get(val_db);

	const char * user = NULL;
	if (val_user->type == so_value_string)
		user = so_value_string_get(val_user);

	const char * password = NULL;
	if (val_password->type == so_value_string)
		password = so_value_string_get(val_password);

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

	so_log_postgresql_module_prepare_queries(connect);

	struct so_log_postgresql_private * self = malloc(sizeof(struct so_log_postgresql_private));
	self->connection = connect;
	self->hostid = hostid;

	struct solgr_log_module * mod = malloc(sizeof(struct solgr_log_module));
	mod->level = so_log_string_to_level(so_value_string_get(level), false);
	mod->ops = &so_log_postgresql_module_ops;
	mod->data = self;

	return mod;
}

static void so_log_postgresql_module_prepare_queries(PGconn * connection) {
	PGresult * prepare = PQprepare(connection, "insert_log", "INSERT INTO log(type, level, time, message, host) VALUES ($1, $2, $3, $4, $5)", 0, NULL);
	PQclear(prepare);
}

static void so_log_postgresql_module_write(struct solgr_log_module * module, struct so_value * message) {
	struct so_log_postgresql_private * self = module->data;

	if (!so_value_hashtable_has_key2(message, "type") || !so_value_hashtable_has_key2(message, "message"))
		return;

	struct so_value * level = so_value_hashtable_get2(message, "level", false, false);
	if (level->type != so_value_string)
		return;

	struct so_value * vtype = so_value_hashtable_get2(message, "type", false, false);
	if (vtype->type != so_value_string)
		return;

	enum so_log_type type = so_log_string_to_type(so_value_string_get(vtype), false);
	if (type == so_log_type_unknown)
		return;

	struct so_value * vtimestamp = so_value_hashtable_get2(message, "timestamp", false, false);
	if (vtimestamp->type != so_value_integer)
		return;

	struct so_value * vmessage = so_value_hashtable_get2(message, "message", false, false);
	if (vmessage->type != so_value_string)
		return;

	char strtime[32];
	time_t timestamp = so_value_integer_get(vtimestamp);
	so_time_convert(&timestamp, "%F %T", strtime, 32);

	enum so_log_level lvl = so_log_string_to_level(so_value_string_get(level), false);
	enum so_log_type typ = so_log_string_to_type(so_value_string_get(vtype), false);

	const char * param[] = {so_log_postgresql_types[typ], so_log_postgresql_levels[lvl], strtime, so_value_string_get(vmessage), self->hostid};

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
			so_log_postgresql_module_prepare_queries(self->connection);
		}
	}
}

