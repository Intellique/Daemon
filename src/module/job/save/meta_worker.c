/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sun, 04 Nov 2012 20:21:16 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// scandir
#include <dirent.h>
// asprintf
#include <stdio.h>
// free
#include <stdlib.h>
// lstat
#include <sys/stat.h>
// lstat
#include <sys/types.h>
// access, lstat
#include <unistd.h>

#include <libstone/util/file.h>

#include "save.h"

ssize_t st_job_save_compute_size(const char * path) {
	if (path == NULL)
		return 0;

	struct stat st;
	if (lstat(path, &st))
		return 0;

	if (S_ISREG(st.st_mode))
		return st.st_size;

	if (S_ISDIR(st.st_mode)) {
		if (access(path, R_OK | X_OK))
			return 0;

		struct dirent ** dl = 0;
		int nb_files = scandir(path, &dl, st_util_file_basic_scandir_filter, 0);

		int i;
		ssize_t total = 0;
		for (i = 0; i < nb_files; i++) {
			char * subpath = 0;
			asprintf(&subpath, "%s/%s", path, dl[i]->d_name);

			total = st_job_save_compute_size(subpath);

			free(subpath);
			free(dl[i]);
		}
		free(dl);

		return total;
	}

	return 0;
}

