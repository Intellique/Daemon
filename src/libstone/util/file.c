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
*  Last modified: Mon, 04 Feb 2013 21:02:38 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// scandir
#include <dirent.h>
// open
#include <fcntl.h>
// getgrgid_r
#include <grp.h>
// getpwuid_r
#include <pwd.h>
// asprintf, snprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// memmove, strchr, strcpy, strdup, strlen, strncpy, strrchr
#include <string.h>
// lstat, mkdir, open
#include <sys/stat.h>
// getgrgid_r, getpwuid_r, lstat, mkdir, open
#include <sys/types.h>
// access, close, fstat, lstat, readlink, rmdir, unlink
#include <unistd.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>


#include <libstone/util/file.h>
#include <libstone/util/string.h>


int st_util_file_basic_scandir_filter(const struct dirent * d) {
	if (d->d_name[0] != '.')
		return 1;

	if (d->d_name[1] == '\0')
		return 0;

	return d->d_name[1] != '.' || d->d_name[2] != '\0';
}

bool st_util_file_check_link(const char * file) {
	if (access(file, F_OK))
		return false;

	char link[256];
	ssize_t link_lenght = readlink(file, link, 256);

	if (link_lenght < 0)
		return false;

	return !access(link, F_OK);
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

int st_util_file_mkdir(const char * dirname, mode_t mode) {
	if (!access(dirname, F_OK))
		return 0;

	char * dir = strdup(dirname);
	st_util_string_delete_double_char(dir, '/');
	st_util_string_rtrim(dir, '/');

	char * ptr = strrchr(dir, '/');
	if (ptr == NULL) {
		free(dir);
		return mkdir(dirname, mode);
	}

	unsigned short nb = 0;
	do {
		*ptr = '\0';
		nb++;
		ptr = strrchr(dir, '/');
	} while (ptr != NULL && access(dir, F_OK));

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

char * st_util_file_read_all_from(const char * filename) {
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;

	struct stat st;
	int failed = fstat(fd, &st);
	if (failed != 0 || st.st_size == 0) {
		close(fd);
		return NULL;
	}

	char * data = malloc(st.st_size + 1);

	ssize_t nb_read = read(fd, data, st.st_size);
	data[st.st_size] = '\0';

	close(fd);

	if (nb_read < 0) {
		free(data);
		data = NULL;
	}

	return data;
}

char * st_util_file_rename(const char * filename) {
	char * path = strdup(filename);
	char * extension = strrchr(path, '.');

	unsigned int next = 0;
	if (!access(path, F_OK)) {
		char * old_path = path;
		path = NULL;

		if (extension != NULL)
			*extension = '\0';

		do {
			free(path);

			if (extension != NULL)
				asprintf(&path, "%s_%u.%s", old_path, next, extension + 1);
			else
				asprintf(&path, "%s_%u", old_path, next);

			next++;
		} while (!access(path, F_OK));

		free(old_path);
	}

	return path;
}

int st_util_file_rm(const char * path) {
	if (access(path, F_OK))
		return 0;

	struct stat st;
	if (lstat(path, &st))
		return -1;

	if (S_ISDIR(st.st_mode)) {
		struct dirent ** files;
		int nb_files = scandir(path, &files, st_util_file_basic_scandir_filter, NULL);
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

bool st_util_file_write_to(const char * filename, const char * data, ssize_t length) {
	int fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (fd < 0)
		return false;

	ssize_t nb_write = write(fd, data, length);
	close(fd);

	return nb_write == length;
}

