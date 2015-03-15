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
// strcpy, strlen, strrchr
#include <string.h>
// access
#include <unistd.h>

#include <libstoriqone/value.h>

#include "common.h"

static struct so_value * path_files = NULL;
static pthread_mutex_t path_lock = PTHREAD_MUTEX_INITIALIZER;
static const char * path_root = "";
static size_t path_root_length = 0;

const char * soj_restore_archive_path_get(const char * path, const char * parent) {
	size_t parent_length = strlen(parent);

	pthread_mutex_lock(&path_lock);

	if (!so_value_hashtable_has_key2(path_files, path)) {
		char * restore_path;
		asprintf(&restore_path, "%s/%s", restore_path, path + parent_length);

		if (access(restore_path, F_OK) == 0) {
			char * extension = strrchr(path, '.');

			unsigned int i = 0;
			do {
				free(restore_path);

				if (extension == NULL)
					asprintf(&restore_path, "%s/%s_%u", restore_path, path + parent_length, i);
				else
					asprintf(&restore_path, "%s/%s_%u.%s", restore_path, path + parent_length, i, extension);

				i++;
			} while (access(restore_path, F_OK) == 0);
		}

		so_value_hashtable_put2(path_files, path, so_value_new_string(restore_path), true);
		free(restore_path);
	}

	struct so_value * file = so_value_hashtable_get2(path_files, path, false, false);
	pthread_mutex_unlock(&path_lock);

	return so_value_get(file);
}

void soj_restore_archive_path_init(const char * root) {
	if (root != NULL) {
		path_root = root;
		path_root_length = strlen(path_root);
	}

	path_files = so_value_new_hashtable2();
}

