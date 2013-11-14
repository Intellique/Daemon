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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Thu, 14 Nov 2013 18:27:32 +0100                            *
\****************************************************************************/

#include <stddef.h>

#define _GNU_SOURCE
// glob, globfree
#include <glob.h>
// json_*
#include <jansson.h>
// realpath
#include <limits.h>
// free, realpath
#include <stdlib.h>
// strcasecmp
#include <string.h>
// MAXPATHLEN
#include <sys/param.h>

#include <libstone/database.h>
#include <libstone/io.h>
#include <libstone/log.h>
#include <libstone/script.h>
#include <libstone/util/command.h>
#include <libstone/util/debug.h>

#include "config.h"

static const struct st_script_type2 {
	char * name;
	enum st_script_type type;
} st_script_types[] = {
	{ "on error", st_script_type_on_error, },
	{ "post",     st_script_type_post, },
	{ "pre",      st_script_type_pre, },

	{ "unknown", st_script_type_unknown },
};


json_t * st_script_run(struct st_database_connection * connect, enum st_script_type type, struct st_pool * pool, json_t * data) {
	json_t * result = json_object();
	json_object_set_new(result, "should run", json_false());

	json_t * datas = json_array();
	json_object_set_new(result, "data", datas);

	if (connect == NULL || pool == NULL) {
		json_object_set_new(result, "error", json_string("connect or pool equals NULL"));
		return result;
	}

	int nb_scripts = connect->ops->get_nb_scripts(connect, type, pool);
	if (nb_scripts < 0)
		return result;

	int i, status = 0;
	for (i = 0; i < nb_scripts && status == 0; i++) {
		char * path = connect->ops->get_script(connect, i, type, pool);
		if (path == NULL) {
			status = 1;
			break;
		}

		// run command & wait result
		struct st_util_command command;
		const char * command_params[] = { st_script_type_to_string(type) };
		st_util_command_new(&command, path, command_params, 1);

		int fd_write = st_util_command_pipe_to(&command);
		int fd_read = st_util_command_pipe_from(&command, st_util_command_stdout);

		st_util_command_start(&command, 1);

		st_io_json_write_to(fd_write, data, true);
		json_t * returned = st_io_json_read_from(fd_read, true);

		st_util_command_wait(&command, 1);

		status = command.exited_code;

		st_util_command_free(&command, 1);
		free(path);

		if (returned == NULL) {
			status = 1;
			break;
		}

		json_t * shouldRun = json_object_get(returned, "should run");
		if (shouldRun == NULL) {
			json_decref(returned);
			status = 1;
			break;
		}

		json_t * data = json_object_get(returned, "data");
		if (data != NULL)
			json_array_append(datas, data);

		if (json_is_false(shouldRun))
			status = 1;

		json_decref(returned);
	}

	if (status == 0)
		json_object_set_new(result, "should run", json_true());

	return result;
}

bool st_io_json_should_run(struct json_t * data) {
	bool sr = false;

	if (data == NULL)
		return sr;

	json_t * jsr = json_object_get(data, "should run");
	if (jsr == NULL)
		return sr;

	if (json_is_true(jsr))
		sr = true;

	return sr;
}

enum st_script_type st_script_string_to_type(const char * string) {
	if (string == NULL)
		return st_script_type_unknown;

	unsigned int i;
	for (i = 0; st_script_types[i].type != st_script_type_unknown; i++)
		if (!strcasecmp(string, st_script_types[i].name))
			return st_script_types[i].type;

	return st_script_type_unknown;
}

void st_script_sync(struct st_database_connection * connection) {
	if (connection == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Try to synchronize scripts without database connection");
		st_debug_log_stack(16);
		return;
	}

	if (connection->ops->is_connection_closed(connection)) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Try to synchronize scripts with closed connection to database");
		st_debug_log_stack(16);
		return;
	}

	glob_t gl;
	glob(SCRIPT_PATH "/*", 0, NULL, &gl);

	unsigned int i;
	for (i = 0; i < gl.gl_pathc; i++) {
		char resolved_path[MAXPATHLEN];

		if (realpath(gl.gl_pathv[i], resolved_path) != NULL && connection->ops->sync_script(connection, resolved_path))
			st_log_write_all(st_log_level_error, st_log_type_checksum, "Failed to synchronize script '%s'", resolved_path);
	}

	globfree(&gl);
}

const char * st_script_type_to_string(enum st_script_type type) {
	unsigned int i;
	for (i = 0; st_script_types[i].type != st_script_type_unknown; i++)
		if (st_script_types[i].type == type)
			return st_script_types[i].name;

	return st_script_types[i].name;
}

