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
*  Last modified: Thu, 16 Aug 2012 10:43:12 +0200                         *
\*************************************************************************/

// scandir
#include <dirent.h>
// getgrgid_r
#include <grp.h>
// getpwuid_r
#include <pwd.h>
// snprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// memmove, strdup, strchr, strcpy, strlen, strncpy
#include <string.h>
// lstat, mkdir
#include <sys/stat.h>
// getgrgid_r, getpwuid_r, lstat, mkdir
#include <sys/types.h>
// access, lstat, rmdir, unlink
#include <unistd.h>

#include <libstone/util/file.h>
#include <libstone/util/string.h>


int st_util_file_basic_scandir_filter(const struct dirent * d) {
	if (d->d_name[0] != '.')
		return 1;

	if (d->d_name[1] == '\0')
		return 0;

	return d->d_name[1] != '.' || d->d_name[2] != '\0';
}

void st_util_file_convert_size_to_string(ssize_t size, char * str, ssize_t str_len) {
	unsigned short mult = 0;
	double tsize = size;

	while (tsize >= 1024 && mult < 4) {
		tsize /= 1024;
		mult++;
	}

	switch (mult) {
		case 0:
			snprintf(str, str_len, "%zd Bytes", size);
			break;
		case 1:
			snprintf(str, str_len, "%.1f KBytes", tsize);
			break;
		case 2:
			snprintf(str, str_len, "%.2f MBytes", tsize);
			break;
		case 3:
			snprintf(str, str_len, "%.3f GBytes", tsize);
			break;
		default:
			snprintf(str, str_len, "%.4f TBytes", tsize);
	}

	if (strchr(str, '.')) {
		char * ptrEnd = strchr(str, ' ');
		char * ptrBegin = ptrEnd - 1;
		while (*ptrBegin == '0')
			ptrBegin--;
		if (*ptrBegin == '.')
			ptrBegin--;

		if (ptrBegin + 1 < ptrEnd)
			memmove(ptrBegin + 1, ptrEnd, strlen(ptrEnd) + 1);
	}
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

int st_util_file_mkdir(const char * dirname, mode_t mode) {
	if (!access(dirname, F_OK))
		return 0;

	char * dir = strdup(dirname);
	st_util_string_delete_double_char(dir, '/');

	char * ptr = strrchr(dir, '/');
	if (!ptr) {
		free(dir);
		return mkdir(dirname, mode);
	}

	unsigned short nb = 0;
	do {
		*ptr = '\0';
		nb++;
		ptr = strrchr(dir, '/');
	} while (ptr && access(dir, F_OK));

	int failed = 0;
	if (access(dir, F_OK))
		failed = mkdir(dir, mode);

	unsigned short i;
	for (i = 0; i < nb && !failed; i++) {
		size_t length = strlen(dir);
		dir[length] = '/';

		failed = mkdir(dir, mode);
	}

	free(dir);

	return failed;
}

int st_util_file_rm(const char * path) {
	if (access(path, F_OK))
		return 0;

	struct stat st;
	if (lstat(path, &st))
		return -1;

	if (S_ISDIR(st.st_mode)) {
		struct dirent ** files;
		int nb_files = scandir(path, &files, st_util_file_basic_scandir_filter, 0);
		if (nb_files < 0)
			return -1;

		size_t path_length = strlen(path);

		int i, failed = 0;
		for (i = 0; i < nb_files; i++) {
			if (!failed) {
				size_t file_length = strlen(files[i]->d_name);
				char * file = malloc(path_length + file_length + 2);

				strcpy(file, path);
				file[path_length] = '/';
				strcpy(file + path_length + 1, files[i]->d_name);

				failed = st_util_file_rm(file);

				free(file);
			}

			free(files[i]);
		}
		free(files);

		if (failed)
			return failed;

		return rmdir(path);
	} else {
		return unlink(path);
	}
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

