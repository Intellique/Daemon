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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Wed, 20 Nov 2013 13:07:10 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// scandir
#include <dirent.h>
// open
#include <fcntl.h>
// getgrgid_r
#include <grp.h>
// basename, dirname
#include <libgen.h>
// getpwuid_r
#include <pwd.h>
// asprintf, rename, snprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// memmove, strchr, strcpy, strdup, strlen, strncpy, strrchr
#include <string.h>
// lstat, mkdir, open
#include <sys/stat.h>
// getgrgid_r, getpwuid_r, lstat, mkdir, mkfifo, mknod, open
#include <sys/types.h>
// access, close, fstat, lstat, mkfifo, read, readlink, rmdir, symlink, unlink, write
#include <unistd.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>


#include <libstone/log.h>
#include <libstone/util/file.h>
#include <libstone/util/string.h>


char * st_util_file_get_serial(const char * filename) {
	char * serial = st_util_file_read_all_from(filename);
	if (serial == NULL) {
		uuid_t id;
		uuid_generate(id);

		char uuid[37];
		uuid_unparse_lower(id, uuid);

		serial = strdup(uuid);

		st_util_file_write_to(filename, serial, 36);
	}

	return serial;
}

void st_util_file_gid2name(char * name, ssize_t length, gid_t gid) {
	char * buffer = malloc(512);

	struct group gr;
	struct group * tmp_gr;

	if (!getgrgid_r(gid, &gr, buffer, 512, &tmp_gr)) {
		strncpy(name, gr.gr_name, length);
		name[length - 1] = '\0';
	} else {
		snprintf(name, length, "%d", gid);
	}

	free(buffer);
}

char * st_util_file_trunc_path(char * path, int nb_trunc_path) {
	while (nb_trunc_path > 0 && path) {
		path = strchr(path, '/');
		nb_trunc_path--;

		if (path && *path == '/')
			path++;
	}

	return path;
}

void st_util_file_uid2name(char * name, ssize_t length, uid_t uid) {
	char * buffer = malloc(512);

	struct passwd pw;
	struct passwd * tmp_pw;

	if (!getpwuid_r(uid, &pw, buffer, 512, &tmp_pw)) {
		strncpy(name, pw.pw_name, length);
		name[length - 1] = '\0';
	} else {
		snprintf(name, length, "%d", uid);
	}

	free(buffer);
}

bool st_util_file_write_to(const char * filename, const char * data, ssize_t length) {
	int fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (fd < 0)
		return false;

	ssize_t nb_write = write(fd, data, length);
	close(fd);

	return nb_write == length;
}

