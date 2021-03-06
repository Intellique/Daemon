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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// scandir
#include <dirent.h>
// fnmatch
#include <fnmatch.h>
// glob, globfree
#include <glob.h>
// gettext
#include <libintl.h>
// asprintf, sscanf
#include <stdio.h>
// free
#include <stdlib.h>
// strrchr
#include <string.h>
// close
#include <unistd.h>

#include <libstoriqone/checksum.h>
#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/process.h>
#include <libstoriqone/value.h>

#include "plugin.h"

#include "config.h"

static int sod_plugin_job_filter(const struct dirent * file);
static int sod_plugin_sync_checksum_inner(void * arg);


static int sod_plugin_job_filter(const struct dirent * file) {
	return !fnmatch("job_*", file->d_name, 0);
}

void sod_plugin_sync_checksum(struct so_database_config * config) {
	so_process_fork_and_do(sod_plugin_sync_checksum_inner, config);
}

int sod_plugin_sync_checksum_inner(void * arg) {
	struct so_database_config * config = arg;
	struct so_database_connection * connection = config->ops->connect(config);

	if (connection == NULL)
		return 1;

	glob_t gl;
	glob(MODULE_PATH "/libchecksum-*.so", 0, NULL, &gl);

	unsigned int i;
	for (i = 0; i < gl.gl_pathc; i++) {
		char * ptr = strrchr(gl.gl_pathv[i], '/') + 1;

		char plugin[64];
		sscanf(ptr, "libchecksum-%63[^.].so", plugin);

		if (connection->ops->find_plugin_checksum(connection, plugin))
			continue;

		struct so_checksum_driver * driver = so_checksum_get_driver(plugin);
		if (driver != NULL)
			connection->ops->sync_plugin_checksum(connection, driver);
	}

	globfree(&gl);

	connection->ops->close(connection);
	connection->ops->free(connection);

	return 0;
}

void sod_plugin_sync_job(struct so_database_connection * connection) {
	struct dirent ** files = NULL;
	int nb_files = scandir(DAEMON_JOB_DIR, &files, sod_plugin_job_filter, alphasort);
	int i;
	for (i = 0; i < nb_files; i++) {
		connection->ops->sync_plugin_job(connection, files[i]->d_name + 4);
		free(files[i]);
	}
	free(files);
}

void sod_plugin_sync_scripts(struct so_database_connection * connection) {
	struct dirent ** files = NULL;
	int nb_files = scandir(SCRIPT_PATH, &files, so_file_basic_scandir_filter, alphasort);
	int i;
	for (i = 0; i < nb_files; i++) {
		char * filename = NULL;
		int size = asprintf(&filename, SCRIPT_PATH "/%s", files[i]->d_name);

		if (size > 0) {
			const char * params[] = { "config" };
			struct so_process process;
			so_process_new(&process, filename, params, 1);

			int fd_read = so_process_pipe_from(&process, so_process_stdout);

			so_process_start(&process, 1);
			so_process_wait(&process, 1, true);

			struct so_value * returned = so_json_parse_fd(fd_read, 10000);

			const char * script_name = NULL, * script_desciption = NULL, * script_type = NULL;
			if (returned == NULL)
				so_log_write2(so_log_level_warning, so_log_type_daemon, gettext("Script (%s) returned an invalid json"), filename);
			else if (so_value_unpack(returned, "{sSsSsS}", "name", &script_name, "description", &script_desciption, "type", &script_type) == 3)
				connection->ops->sync_plugin_script(connection, script_name, script_desciption, filename, script_type);
			else
				so_log_write2(so_log_level_warning, so_log_type_daemon, gettext("Script (%s) returned an incompleted json"), filename);

			if (returned != NULL)
				so_value_free(returned);
			close(fd_read);
			free(filename);
		}

		free(files[i]);
	}
	free(files);
}
