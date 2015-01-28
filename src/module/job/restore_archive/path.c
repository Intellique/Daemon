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
*  Copyright (C) 2013-2015, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Thu, 06 Jun 2013 12:46:35 +0200                            *
\****************************************************************************/

// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>

#include <libstone/util/file.h>
#include <libstone/util/hashtable.h>
#include <libstone/util/string.h>
#include <libstone/util/util.h>

#include "common.h"

void st_job_restore_archive_path_free(struct st_job_restore_archive_path * restore_path) {
	st_hashtable_free(restore_path->paths);
	pthread_mutex_destroy(&restore_path->lock);
	free(restore_path);
}

char * st_job_restore_archive_path_get(struct st_job_restore_archive_path * restore_path, struct st_database_connection * connect, struct st_job * job, struct st_archive_file * file, bool has_restore_to) {
	pthread_mutex_lock(&restore_path->lock);

	struct st_hashtable_value val = st_hashtable_get(restore_path->paths, file->name);

	char * path;
	if (val.type == st_hashtable_value_null) {
		if (has_restore_to) {
			char * tmp_path = connect->ops->get_restore_path_from_file(connect, job, file);
			if (file->type != st_archive_file_type_directory) {
				path = st_util_file_rename(tmp_path);
				free(tmp_path);
			} else
				path = tmp_path;
		} else {
			if (file->type != st_archive_file_type_directory)
				path = st_util_file_rename(file->name);
			else
				path = strdup(file->name);
		}

		st_hashtable_put(restore_path->paths, strdup(file->name), st_hashtable_val_string(strdup(path)));
	} else
		path = strdup(val.value.string);

	pthread_mutex_unlock(&restore_path->lock);

	return path;
}

struct st_job_restore_archive_path * st_job_restore_archive_path_new() {
	struct st_job_restore_archive_path * restore_path = malloc(sizeof(struct st_job_restore_archive_path));
	restore_path->paths = st_hashtable_new2(st_util_string_compute_hash, st_util_basic_free);
	pthread_mutex_init(&restore_path->lock, NULL);
	return restore_path;
}

