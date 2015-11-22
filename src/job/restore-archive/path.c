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
// pthread_mutex_init, pthread_mutex_lock, pthread_mutex_unlock
#include <pthread.h>
// asprintf
#include <stdio.h>
// stdlib
#include <stdlib.h>
// strchr, strcpy, strlen, strrchr
#include <string.h>
// access
#include <unistd.h>

#include <libstoriqone/file.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

#include "common.h"

static void soj_restorearchive_path_exit(void) __attribute__((destructor));

static struct so_value * path_files = NULL;
static pthread_mutex_t path_lock = PTHREAD_MUTEX_INITIALIZER;
static const char * path_root = "";
static size_t path_root_length = 0;
static struct so_value * path_selected = NULL;


static void soj_restorearchive_path_exit() {
	so_value_free(path_files);
}

bool soj_restorearchive_path_filter(const char * path) {
	if (so_value_list_get_length(path_selected) == 0)
		return true;

	bool found = false;
	struct so_value_iterator * iter = so_value_list_get_iterator(path_selected);
	while (!found && so_value_iterator_has_next(iter)) {
		struct so_value * val = so_value_iterator_get_value(iter, false);
		const char * filter = so_value_string_get(val);

		size_t length = strlen(filter);
		found = strncmp(path, filter, length) == 0;
	}
	so_value_iterator_free(iter);
	return found;
}

const char * soj_restorearchive_path_get(const char * path, const char * parent, bool is_regular_file) {
	pthread_mutex_lock(&path_lock);

	if (!so_value_hashtable_has_key2(path_files, path)) {
		const char * ppath = path;
		if (path_root_length > 0) {
			ppath += strlen(parent) - 1;

			while (*ppath == '/')
				ppath--;

			while (*ppath != '/')
				ppath--;

			ppath++;
		}

		char * restore_path = NULL;
		int size = asprintf(&restore_path, "%s/%s", path_root, ppath);

		if (size < 0) {
			pthread_mutex_unlock(&path_lock);

			return NULL;
		}

		if (is_regular_file && access(restore_path, F_OK) == 0) {
			char * tmp_restore_path = so_file_rename(restore_path);
			free(restore_path);
			restore_path = tmp_restore_path;
		}

		if (restore_path != NULL) {
			so_string_delete_double_char(restore_path, '/');
			so_value_hashtable_put2(path_files, path, so_value_new_string(restore_path), true);
			free(restore_path);
		} else {
			pthread_mutex_unlock(&path_lock);
			return NULL;
		}
	}

	struct so_value * file = so_value_hashtable_get2(path_files, path, false, false);
	pthread_mutex_unlock(&path_lock);

	return so_value_string_get(file);
}

void soj_restorearchive_path_init(const char * root, struct so_value * selected_path) {
	if (root != NULL) {
		path_root = root;
		path_root_length = strlen(path_root);
	}

	path_files = so_value_new_hashtable2();

	path_selected = selected_path;
}

