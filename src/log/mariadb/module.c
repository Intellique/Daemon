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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// free, malloc
#include <malloc.h>
// mysql_*
#include <mysql/mysql.h>
// sscanf
#include <stdio.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>
// uname
#include <sys/utsname.h>
// sleep
#include <unistd.h>

#include "common.h"

struct st_log_mariadb_private {
	MYSQL handler;
	int host_id;
	MYSQL_STMT * insert_log;
};

static void st_log_mariadb_module_free(struct lgr_log_module * module);
static void st_log_mariadb_module_prepare_queries(struct st_log_mariadb_private * self);
static void st_log_mariadb_module_write(struct lgr_log_module * module, struct st_value * message);

static struct lgr_log_module_ops st_log_mariadb_module_ops = {
	.free  = st_log_mariadb_module_free,
	.write = st_log_mariadb_module_write,
};

static char * st_log_mariadb_levels[] = {
	[st_log_level_alert]      = "alert",
	[st_log_level_critical]   = "critical",
	[st_log_level_debug]      = "debug",
	[st_log_level_emergencey] = "emergency",
	[st_log_level_error]      = "error",
	[st_log_level_info]       = "info",
	[st_log_level_notice]     = "notice",
	[st_log_level_warning]    = "warning",
};

static char * st_log_mariadb_types[] = {
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


static void st_log_mariadb_module_free(struct lgr_log_module * module) {
	struct st_log_mariadb_private * self = module->data;
	mysql_stmt_close(self->insert_log);
	mysql_close(&self->handler);
	free(self);

	module->ops = NULL;
	module->data = NULL;
}

struct lgr_log_module * st_log_mariadb_new_module(struct st_value * params) {
	struct st_value * val_host = st_value_hashtable_get2(params, "host", false, false);
	struct st_value * val_port = st_value_hashtable_get2(params, "port", false, false);
	struct st_value * val_db = st_value_hashtable_get2(params, "db", false, false);
	struct st_value * val_user = st_value_hashtable_get2(params, "user", false, false);
	struct st_value * val_password = st_value_hashtable_get2(params, "password", false, false);
	struct st_value * level = st_value_hashtable_get2(params, "verbosity", false, false);

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

	unsigned int pport = 0;
	if (port != NULL)
		sscanf(port, "%u", &pport);

	struct st_log_mariadb_private * self = malloc(sizeof(struct st_log_mariadb_private));
	bzero(self, sizeof(*self));
	mysql_init(&self->handler);
	self->host_id = 0;

	void * ok = mysql_real_connect(&self->handler, host, user, password, db, pport, NULL, 0);
	if (ok == NULL) {
		free(self);
		return NULL;
	}

	my_bool auto_connect = 1;
	mysql_options(&self->handler, MYSQL_OPT_RECONNECT, &auto_connect);

	struct utsname name;
	uname(&name);

	MYSQL_STMT * get_host_id = mysql_stmt_init(&self->handler);
	mysql_stmt_prepare(get_host_id, "SELECT id FROM Host WHERE name = ? OR CONCAT(name, '.', domaine) = ? LIMIT 1", 68);

	MYSQL_BIND param[] = {
		{
			.buffer_type   = MYSQL_TYPE_STRING,
			.buffer        = name.nodename,
			.buffer_length = strlen(name.nodename),
			.is_null       = false,
		}, {
			.buffer_type   = MYSQL_TYPE_STRING,
			.buffer        = name.nodename,
			.buffer_length = strlen(name.nodename),
			.is_null       = false,
		}
	};
	mysql_stmt_bind_param(get_host_id, param);

	mysql_stmt_execute(get_host_id);

	MYSQL_BIND result;
	bzero(&result, sizeof(result));

	result.buffer_type = MYSQL_TYPE_LONG;
	result.buffer = (char *) &self->host_id;
	result.is_null = false;

	mysql_stmt_bind_result(get_host_id, &result);
	mysql_stmt_fetch(get_host_id);
	mysql_stmt_close(get_host_id);

	if (self->host_id < 1) {
		mysql_close(&self->handler);
		free(self);
		return NULL;
	}

	st_log_mariadb_module_prepare_queries(self);

	struct lgr_log_module * mod = malloc(sizeof(struct lgr_log_module));
	mod->level = st_log_string_to_level(st_value_string_get(level));
	mod->ops = &st_log_mariadb_module_ops;
	mod->data = self;

	return mod;
}

static void st_log_mariadb_module_prepare_queries(struct st_log_mariadb_private * self) {
	self->insert_log = mysql_stmt_init(&self->handler);
	mysql_stmt_prepare(self->insert_log, "INSERT INTO Log(type, level, time, message, host) VALUES (?, ?, FROM_UNIXTIME(?), ?, ?)", 87);
}

static void st_log_mariadb_module_write(struct lgr_log_module * module, struct st_value * message) {
	struct st_log_mariadb_private * self = module->data;

	if (!st_value_hashtable_has_key2(message, "type") || !st_value_hashtable_has_key2(message, "message"))
		return;

	struct st_value * level = st_value_hashtable_get2(message, "level", false, false);
	if (level->type != st_value_string)
		return;

	struct st_value * vtype = st_value_hashtable_get2(message, "type", false, false);
	if (vtype->type != st_value_string)
		return;

	enum st_log_type type = st_log_string_to_type(st_value_string_get(vtype));
	if (type == st_log_type_unknown)
		return;

	struct st_value * vtimestamp = st_value_hashtable_get2(message, "timestamp", false, false);
	if (vtimestamp->type != st_value_integer)
		return;

	struct st_value * vmessage = st_value_hashtable_get2(message, "message", false, false);
	if (vmessage->type != st_value_string)
		return;

	enum st_log_level lvl = st_log_string_to_level(st_value_string_get(level));
	enum st_log_type typ = st_log_string_to_type(st_value_string_get(vtype));

	MYSQL_BIND params[5];
	bzero(params, sizeof(params));

	params[0].buffer_type = MYSQL_TYPE_STRING;
	params[0].buffer = st_log_mariadb_types[typ];
	params[0].buffer_length = strlen(st_log_mariadb_types[typ]);

	params[1].buffer_type = MYSQL_TYPE_STRING;
	params[1].buffer = st_log_mariadb_levels[lvl];
	params[1].buffer_length = strlen(st_log_mariadb_levels[lvl]);

	long long int timestamp = st_value_integer_get(vtimestamp);
	params[2].buffer_type = MYSQL_TYPE_LONGLONG;
	params[2].buffer = (char *) &timestamp;
	params[2].buffer_length = sizeof(long long int);

	params[3].buffer_type = MYSQL_TYPE_STRING;
	params[3].buffer = (char *) st_value_string_get(vmessage);
	params[3].buffer_length = strlen(st_value_string_get(vmessage));

	params[4].buffer_type = MYSQL_TYPE_LONG;
	params[4].buffer = (char *) &self->host_id;
	params[4].buffer_length = sizeof(self->host_id);

	int failed = 1;
	unsigned short i;
	for (i = 0; i < 5 && failed != 0; i++) {
		mysql_stmt_bind_param(self->insert_log, params);
		failed = mysql_stmt_execute(self->insert_log);
		mysql_stmt_reset(self->insert_log);

		if (failed != 0) {
			mysql_stmt_close(self->insert_log);
			self->insert_log = NULL;

			sleep(i);
			mysql_ping(&self->handler);

			st_log_mariadb_module_prepare_queries(self);
		}
	}
}

