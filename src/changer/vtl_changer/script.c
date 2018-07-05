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
// faccessat, fstatat
#include <fcntl.h>
// dgettext
#include <libintl.h>
// scandir, versionsort
#include <dirent.h>
// asprintf
#include <stdio.h>
// free
#include <stdlib.h>
// fstatat
#include <sys/stat.h>
// fstatat
#include <sys/types.h>
// faccessat, fstatat
#include <unistd.h>

#include <libstoriqone/changer.h>
#include <libstoriqone/file.h>
#include <libstoriqone/log.h>
#include <libstoriqone/process.h>

#include "script.h"

static int sochgr_vtl_script_filter(const struct dirent * d);
static int sochgr_vtl_script_run(struct so_changer * changer, const char * path, const char * script, int i_script, const char * params[], unsigned int nb_params);

static int sochgr_vtl_script_directory_fd = -1;


static int sochgr_vtl_script_filter(const struct dirent * d) {
	if (so_file_basic_scandir_filter(d) == 0)
		return 0;

	if (faccessat(sochgr_vtl_script_directory_fd, d->d_name, F_OK | R_OK | X_OK, 0) != 0)
		return 0;

	struct stat info;
	int failed = fstatat(sochgr_vtl_script_directory_fd, d->d_name, &info, 0);
	if (failed != 0)
		return 0;

	return S_ISREG(info.st_mode);
}

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

int sochgr_vtl_script_post_offline(struct so_changer * changer, const char * root_directory) {
	char * path = NULL;
	ssize_t length = asprintf(&path, "%s/script/post-offline", root_directory);
	if (length < 0)
		return -1;

	sochgr_vtl_script_directory_fd = open(path, O_DIRECTORY);
	if (sochgr_vtl_script_directory_fd < 0) {
		free(path);
		return -1;
	}

	struct dirent ** files = NULL;
	int nb_files = scandir(path, &files, sochgr_vtl_script_filter, versionsort);

	close(sochgr_vtl_script_directory_fd);
	sochgr_vtl_script_directory_fd = -1;

	if (nb_files < 0)
		return nb_files;
	if (nb_files == 0) {
		free(files);

		so_log_write(so_log_level_debug,
			dgettext("storiqone-changer-vtl", "[%s | %s]: there is no scripts to be run"),
			changer->vendor, changer->model);

		return 0;
	}

	so_log_write(so_log_level_debug,
		dgettext("storiqone-changer-vtl", "[%s | %s]: starting post-script"),
		changer->vendor, changer->model);

	int i, failed = 0;
	for (i = 0; i < nb_files; i++) {
		if (failed == 0) {
			const char * params[] = { "post", "offline", changer->serial_number };
			failed = sochgr_vtl_script_run(changer, path, files[i]->d_name, i, params, 3);
		}
		free(files[i]);
	}
	free(files);

	so_log_write(so_log_level_debug,
		dgettext("storiqone-changer-vtl", "[%s | %s]: no more scripts to be run"),
		changer->vendor, changer->model);

	free(path);
	return failed;
}

int sochgr_vtl_script_post_online(struct so_changer * changer, const char * root_directory) {
	char * path = NULL;
	ssize_t length = asprintf(&path, "%s/script/post-online", root_directory);
	if (length < 0)
		return -1;

	sochgr_vtl_script_directory_fd = open(path, O_DIRECTORY);
	if (sochgr_vtl_script_directory_fd < 0) {
		free(path);
		return -1;
	}

	struct dirent ** files = NULL;
	int nb_files = scandir(path, &files, sochgr_vtl_script_filter, versionsort);

	close(sochgr_vtl_script_directory_fd);
	sochgr_vtl_script_directory_fd = -1;

	if (nb_files < 0)
		return nb_files;
	if (nb_files == 0) {
		free(files);

		so_log_write(so_log_level_debug,
			dgettext("storiqone-changer-vtl", "[%s | %s]: there is no scripts to be run"),
			changer->vendor, changer->model);

		return 0;
	}

	so_log_write(so_log_level_debug,
		dgettext("storiqone-changer-vtl", "[%s | %s]: starting post-script"),
		changer->vendor, changer->model);

	int i, failed = 0;
	for (i = 0; i < nb_files; i++) {
		if (failed == 0) {
			const char * params[] = { "post", "online", changer->serial_number };
			failed = sochgr_vtl_script_run(changer, path, files[i]->d_name, i, params, 3);
		}
		free(files[i]);
	}
	free(files);

	so_log_write(so_log_level_debug,
		dgettext("storiqone-changer-vtl", "[%s | %s]: no more scripts to be run"),
		changer->vendor, changer->model);

	free(path);
	return failed;
}

int sochgr_vtl_script_pre_offline(struct so_changer * changer, const char * root_directory) {
	char * path = NULL;
	ssize_t length = asprintf(&path, "%s/script/pre-offline", root_directory);
	if (length < 0)
		return -1;

	sochgr_vtl_script_directory_fd = open(path, O_DIRECTORY);
	if (sochgr_vtl_script_directory_fd < 0) {
		free(path);
		return -1;
	}

	struct dirent ** files = NULL;
	int nb_files = scandir(path, &files, sochgr_vtl_script_filter, versionsort);

	close(sochgr_vtl_script_directory_fd);
	sochgr_vtl_script_directory_fd = -1;

	if (nb_files < 0)
		return nb_files;
	if (nb_files == 0) {
		free(files);

		so_log_write(so_log_level_debug,
			dgettext("storiqone-changer-vtl", "[%s | %s]: there is no scripts to be run"),
			changer->vendor, changer->model);

		return 0;
	}

	so_log_write(so_log_level_debug,
		dgettext("storiqone-changer-vtl", "[%s | %s]: starting pre-script"),
		changer->vendor, changer->model);

	int i, failed = 0;
	for (i = 0; i < nb_files; i++) {
		if (failed == 0) {
			const char * params[] = { "pre", "offline", changer->serial_number };
			failed = sochgr_vtl_script_run(changer, path, files[i]->d_name, i, params, 3);
		}
		free(files[i]);
	}
	free(files);

	so_log_write(so_log_level_debug,
		dgettext("storiqone-changer-vtl", "[%s | %s]: no more scripts to be run"),
		changer->vendor, changer->model);

	free(path);
	return failed;
}

int sochgr_vtl_script_pre_online(struct so_changer * changer, const char * root_directory) {
	char * path = NULL;
	ssize_t length = asprintf(&path, "%s/script/pre-online", root_directory);
	if (length < 0)
		return -1;

	sochgr_vtl_script_directory_fd = open(path, O_DIRECTORY);
	if (sochgr_vtl_script_directory_fd < 0) {
		free(path);
		return -1;
	}

	struct dirent ** files = NULL;
	int nb_files = scandir(path, &files, sochgr_vtl_script_filter, versionsort);

	close(sochgr_vtl_script_directory_fd);
	sochgr_vtl_script_directory_fd = -1;

	if (nb_files < 0)
		return nb_files;
	if (nb_files == 0) {
		free(files);

		so_log_write(so_log_level_debug,
			dgettext("storiqone-changer-vtl", "[%s | %s]: there is no scripts to be run"),
			changer->vendor, changer->model);

		return 0;
	}

	so_log_write(so_log_level_debug,
		dgettext("storiqone-changer-vtl", "[%s | %s]: starting pre-script"),
		changer->vendor, changer->model);

	int i, failed = 0;
	for (i = 0; i < nb_files; i++) {
		if (failed == 0) {
			const char * params[] = { "pre", "online", changer->serial_number };
			failed = sochgr_vtl_script_run(changer, path, files[i]->d_name, i, params, 3);
		}
		free(files[i]);
	}
	free(files);

	so_log_write(so_log_level_debug,
		dgettext("storiqone-changer-vtl", "[%s | %s]: no more scripts to be run"),
		changer->vendor, changer->model);

	free(path);
	return failed;
}

static int sochgr_vtl_script_run(struct so_changer * changer, const char * path, const char * script_name, int i_script, const char * params[], unsigned int nb_params) {
	char * script_path = NULL;
	int length = asprintf(&script_path, "%s/%s", path, script_name);
	if (length < 0) {
		so_log_write(so_log_level_error,
			dgettext("storiqone-changer-vtl", "[%s | %s]: memory allocation failure"),
			changer->vendor, changer->model);

		return 1;
	}

	struct so_process script;
	so_process_new(&script, script_path, params, nb_params);
	so_process_set_null(&script, so_process_stdin);
	so_process_set_null(&script, so_process_stdout);
	so_process_set_null(&script, so_process_stderr);

	so_log_write(so_log_level_debug,
		dgettext("storiqone-changer-vtl", "[%s | %s]: Starting #%d script '%s'"),
		changer->vendor, changer->model, i_script, script_path);

	so_process_start(&script, 1);
	so_process_wait(&script, 1, true);

	so_log_write(so_log_level_debug,
		dgettext("storiqone-changer-vtl", "[%s | %s]: script #%d '%s' has finished with exit code: %d"),
		changer->vendor, changer->model, i_script, script_path, script.exited_code);

	int failed = script.exited_code;

	so_process_free(&script, 1);
	free(script_path);

	return failed;
}
