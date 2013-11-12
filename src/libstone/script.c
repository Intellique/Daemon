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
*  Last modified: Tue, 12 Nov 2013 17:21:12 +0100                            *
\****************************************************************************/

#include <stddef.h>

#define _GNU_SOURCE
// glob, globfree
#include <glob.h>
// realpath
#include <limits.h>
// realpath
#include <stdlib.h>
// strcasecmp
#include <string.h>
// MAXPATHLEN
#include <sys/param.h>

#include <libstone/database.h>
#include <libstone/log.h>
#include <libstone/script.h>
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

