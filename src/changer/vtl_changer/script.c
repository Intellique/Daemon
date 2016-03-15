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

#define _GNU_SOURCE
// scandir, versionsort
#include <dirent.h>
// asprintf
#include <stdio.h>
// free
#include <stdlib.h>

#include <libstoriqone/file.h>
#include <libstoriqone/process.h>

#include "script.h"

static int sochgr_vtl_script_run(const char * path, const char * params[], unsigned int nb_params);


int sochgr_vtl_script_init(const char * root_directory) {
	char * path = NULL;
	ssize_t length = asprintf(&path, "%s/script/pre-online", root_directory);
	if (length < 0)
		return -1;

	int failed = so_file_mkdir(path, 0755);
	free(path);

	if (failed != 0)
		return -1;

	length = asprintf(&path, "%s/script/post-online", root_directory);
	if (length < 0)
		return -1;

	failed = so_file_mkdir(path, 0755);
	free(path);

	if (failed != 0)
		return -1;

	length = asprintf(&path, "%s/script/pre-offline", root_directory);
	if (length < 0)
		return -1;

	failed = so_file_mkdir(path, 0755);
	free(path);

	if (failed != 0)
		return -1;

	length = asprintf(&path, "%s/script/post-offline", root_directory);
	if (length < 0)
		return -1;

	failed = so_file_mkdir(path, 0755);
	free(path);

	if (failed != 0)
		return -1;

	return 0;
}

int sochgr_vtl_script_post_offline(const char * root_directory) {
	char * path = NULL;
	ssize_t length = asprintf(&path, "%s/script/post-offline", root_directory);
	if (length < 0)
		return -1;

	static const char * params[] = { "post", "offline" };
	int failed = sochgr_vtl_script_run(path, params, 2);

	free(path);
	return failed;
}

int sochgr_vtl_script_post_online(const char * root_directory) {
	char * path = NULL;
	ssize_t length = asprintf(&path, "%s/script/post-online", root_directory);
	if (length < 0)
		return -1;

	static const char * params[] = { "post", "online" };
	int failed = sochgr_vtl_script_run(path, params, 2);

	free(path);
	return failed;
}

int sochgr_vtl_script_pre_offline(const char * root_directory) {
	char * path = NULL;
	ssize_t length = asprintf(&path, "%s/script/pre-offline", root_directory);
	if (length < 0)
		return -1;

	static const char * params[] = { "pre", "offline" };
	int failed = sochgr_vtl_script_run(path, params, 2);

	free(path);
	return failed;
}

int sochgr_vtl_script_pre_online(const char * root_directory) {
	char * path = NULL;
	ssize_t length = asprintf(&path, "%s/script/pre-online", root_directory);
	if (length < 0)
		return -1;

	static const char * params[] = { "pre", "online" };
	int failed = sochgr_vtl_script_run(path, params, 2);

	free(path);
	return failed;
}

static int sochgr_vtl_script_run(const char * path, const char * params[], unsigned int nb_params) {
	struct dirent ** files = NULL;
	int nb_files = scandir(path, &files, so_file_basic_scandir_filter, versionsort);
	if (nb_files < 0)
		return nb_files;

	int i, failed = 0;
	for (i = 0; i < nb_files; i++) {
		if (failed == 0) {
			char * script_path = NULL;
			int length = asprintf(&script_path, "%s/%s", path, files[i]->d_name);
			if (length < 0) {
				failed = 1;
				free(files[i]);
				continue;
			}

			struct so_process script;
			so_process_new(&script, script_path, params, nb_params);
			so_process_set_null(&script, so_process_stdin);
			so_process_set_null(&script, so_process_stdout);
			so_process_set_null(&script, so_process_stderr);

			free(script_path);

			so_process_start(&script, 1);
			so_process_wait(&script, 1, true);

			failed = script.exited_code;

			so_process_free(&script, 1);
		}
		free(files[i]);
	}
	free(files);

	return failed;
}

