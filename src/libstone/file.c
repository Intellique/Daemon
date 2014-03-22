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
\****************************************************************************/

#define _GNU_SOURCE
// scandir
#include <dirent.h>
// open
#include <fcntl.h>
// basename, dirname
#include <libgen.h>
// asprintf, rename, snprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// memmove, strchr, strcpy, strdup, strlen, strrchr
#include <string.h>
// lstat, mkdir, open
#include <sys/stat.h>
// lstat, mkdir, mkfifo, mknod, open
#include <sys/types.h>
// access, close, mkfifo, read, readlink, rmdir, symlink, unlink, write
#include <unistd.h>

#include <libstone/log.h>
#include <libstone/string.h>

#include "file.h"

__asm__(".symver st_file_basic_scandir_filter_v1, st_file_basic_scandir_filter@@LIBSTONE_1.2");
int st_file_basic_scandir_filter_v1(const struct dirent * d) {
	if (d->d_name[0] != '.')
		return 1;

	if (d->d_name[1] == '\0')
		return 0;

	return d->d_name[1] != '.' || d->d_name[2] != '\0';
}

__asm__(".symver st_file_convert_mode_v1, st_file_convert_mode@@LIBSTONE_1.2");
void st_file_convert_mode_v1(char * buffer, mode_t mode) {
	strcpy(buffer, "----------");

	// file type
	if (S_ISDIR(mode))
		buffer[0] = 'd';
	else if (S_ISCHR(mode))
		buffer[0] = 'c';
	else if (S_ISBLK(mode))
		buffer[0] = 'b';
	else if (S_ISFIFO(mode))
		buffer[0] = 'p';
	else if (S_ISLNK(mode))
		buffer[0] = 'l';
	else if (S_ISSOCK(mode))
		buffer[0] = 's';

	// user field
	if (mode & S_IRUSR)
		buffer[1] = 'r';
	if (mode & S_IWUSR)
		buffer[2] = 'w';
	if (mode & S_ISUID)
		buffer[3] = 'S';
	else if (mode & S_IXUSR)
		buffer[3] = 'x';

	// group field
	if (mode & S_IRGRP)
		buffer[4] = 'r';
	if (mode & S_IWGRP)
		buffer[5] = 'w';
	if (mode & S_ISGID)
		buffer[6] = 'S';
	else if (mode & S_IXGRP)
		buffer[6] = 'x';

	// other field
	if (mode & S_IROTH)
		buffer[7] = 'r';
	if (mode & S_IWOTH)
		buffer[8] = 'w';
	if (mode & S_ISVTX)
		buffer[9] = 'T';
	else if (mode & S_IXOTH)
		buffer[9] = 'x';
}

__asm__(".symver st_file_convert_size_to_string_v1, st_file_convert_size_to_string@@LIBSTONE_1.2");
void st_file_convert_size_to_string_v1(size_t size, char * str, ssize_t str_len) {
	unsigned short mult = 0;
	double tsize = size;

	while (tsize >= 8192 && mult < 4) {
		tsize /= 1024;
		mult++;
	}

	int fixed = 0;
	if (tsize < 10)
		fixed = 2;
	else if (tsize < 100)
		fixed = 1;

	switch (mult) {
		case 0:
			snprintf(str, str_len, "%zd Bytes", size);
			break;
		case 1:
			snprintf(str, str_len, "%.*f KBytes", fixed, tsize);
			break;
		case 2:
			snprintf(str, str_len, "%.*f MBytes", fixed, tsize);
			break;
		case 3:
			snprintf(str, str_len, "%.*f GBytes", fixed, tsize);
			break;
		default:
			snprintf(str, str_len, "%.*f TBytes", fixed, tsize);
	}

	if (strchr(str, '.') != NULL) {
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

__asm__(".symver st_file_cp_v1, st_file_cp@@LIBSTONE_1.2");
int st_file_cp_v1(const char * src, const char * dst) {
	struct stat stsrc;
	int failed = lstat(src, &stsrc);
	if (failed != 0) {
		st_log_write(st_log_level_error, "Copy files: Can't get information of '%s' because %m", src);
		return failed;
	}

	if (S_ISSOCK(stsrc.st_mode)) {
		st_log_write(st_log_level_info, "Copy files: Ignore socket file '%s'", src);
		return 0;
	}

	char * dst_file;
	if (!access(dst, F_OK)) {
		struct stat stdst;
		failed = lstat(dst, &stdst);
		if (failed != 0) {
			st_log_write(st_log_level_error, "Copy files: Can't get information of '%s' because %m", dst);
			return failed;
		}

		if (!S_ISDIR(stdst.st_mode)) {
			st_log_write(st_log_level_error, "Copy files: '%s' should be a directory", dst);
			return 2;
		}

		char * cpsrc = strdup(src);
		char * basename_src = basename(cpsrc);

		asprintf(&dst_file, "%s/%s", dst, basename_src);

		free(cpsrc);
		cpsrc = basename_src = NULL;
	} else {
		char * cpdst = strdup(dst);
		char * dirname_dst = dirname(cpdst);

		struct stat stdst;
		failed = lstat(dirname_dst, &stdst);
		if (failed != 0) {
			st_log_write(st_log_level_error, "Copy files: Can't get information of '%s' because %m", dirname_dst);
			free(cpdst);
			return 2;
		}

		if (!S_ISDIR(stdst.st_mode)) {
			st_log_write(st_log_level_error, "Copy files: '%s' should be a directory", dirname_dst);
			free(cpdst);
			return 2;
		}

		free(cpdst);

		dst_file = strdup(dst);
	}

	if (S_ISDIR(stsrc.st_mode)) {
		failed = st_file_mkdir_v1(dst_file, stsrc.st_mode);

		struct dirent ** files = NULL;
		int nb_files = 0;
		if (failed == 0) {
			nb_files = scandir(src, &files, st_file_basic_scandir_filter_v1, NULL);
			if (nb_files < 0)
				failed = 3;
		}

		int i;
		for (i = 0; i < nb_files; i++) {
			if (failed == 0) {
				char * new_src;
				asprintf(&new_src, "%s/%s", src, files[i]->d_name);

				failed = st_file_cp_v1(new_src, dst_file);

				free(new_src);
			}

			free(files[i]);
		}

		free(files);
	} else if (S_ISREG(stsrc.st_mode)) {
		int fd_in = open(src, O_RDONLY);
		if (fd_in < 0) {
			st_log_write(st_log_level_error, "Copy files: Can't open file '%s' for reading because %m", src);
			failed = 4;
		}

		int fd_out;
		if (failed == 0) {
			fd_out = open(dst_file, O_WRONLY | O_CREAT, stsrc.st_mode);
			if (fd_out < 0) {
				close(fd_in);
				st_log_write(st_log_level_error, "Copy files: Can't open file '%s' for writing because %m", dst_file);
				failed = 5;
			}
		}

		if (failed == 0) {
			ssize_t nb_read;
			char buffer[4096];
			while (nb_read = read(fd_in, buffer, 4096), nb_read > 0) {
				ssize_t nb_write = write(fd_out, buffer, nb_read);
				if (nb_write < 0) {
					st_log_write(st_log_level_error, "Copy files: Error while writing into '%s' because %m", dst_file);
					failed = 6;
					break;
				}
			}

			if (nb_read < 0) {
				st_log_write(st_log_level_error, "Copy files: Error while reading into '%s' because %m", src);
				failed = 7;
			}

			close(fd_in);
			close(fd_out);
		}
	} else if (S_ISLNK(stsrc.st_mode)) {
		char link[256];
		ssize_t length = readlink(src, link, 256);
		if (length > 0)
			link[length] = '\0';
		if (length < 0) {
			st_log_write(st_log_level_error, "Copy files: Can't read link of '%s' because %m", src);
			failed = 8;
		}

		if (failed == 0) {
			failed = symlink(link, dst_file);
			if (failed != 0) {
				st_log_write(st_log_level_error, "Copy files: Failed to create symlink '%s' -> '%s' because %m", dst_file, link);
				failed = 9;
			}
		}
	} else if (S_ISFIFO(stsrc.st_mode)) {
		failed = mkfifo(dst_file, stsrc.st_mode);
		if (failed != 0) {
			st_log_write(st_log_level_error, "Copy files: Failed to create fifo '%s' because %m", dst_file);
			failed = 10;
		}
	} else if (S_ISBLK(stsrc.st_mode) || S_ISCHR(stsrc.st_mode)) {
		failed = mknod(dst_file, stsrc.st_mode, stsrc.st_rdev);
		if (failed != 0) {
			st_log_write(st_log_level_error, "Copy files: Failed to create device '%s' because %m", dst_file);
			failed = 10;
		}
	}

	free(dst_file);

	return failed;
}

__asm__(".symver st_file_mkdir_v1, st_file_mkdir@@LIBSTONE_1.2");
int st_file_mkdir_v1(const char * dirname, mode_t mode) {
	if (!access(dirname, F_OK))
		return 0;

	char * dir = strdup(dirname);
	st_string_delete_double_char(dir, '/');
	st_string_rtrim(dir, '/');

	char * ptr = strrchr(dir, '/');
	int failed = 0;
	if (ptr == NULL) {
		free(dir);
		failed = mkdir(dirname, mode);

		if (failed != 0)
			st_log_write(st_log_level_error, "Error while create directory '%s' because %m", dirname);

		return failed;
	}

	unsigned short nb = 0;
	do {
		*ptr = '\0';
		nb++;
		ptr = strrchr(dir, '/');
	} while (ptr != NULL && access(dir, F_OK));

	if (access(dir, F_OK))
		failed = mkdir(dir, mode);

	if (failed != 0)
		st_log_write(st_log_level_error, "Error while create directory '%s' because %m", dirname);

	unsigned short i;
	for (i = 0; i < nb && !failed; i++) {
		size_t length = strlen(dir);
		dir[length] = '/';

		failed = mkdir(dir, mode);

		if (failed != 0)
			st_log_write(st_log_level_error, "Error while create directory '%s' because %m", dirname);
	}

	free(dir);

	return failed;
}

__asm__(".symver st_file_mv_v1, st_file_mv@@LIBSTONE_1.2");
int st_file_mv_v1(const char * src, const char * dst) {
	struct stat stsrc;
	int failed = lstat(src, &stsrc);
	if (failed != 0) {
		st_log_write(st_log_level_error, "Move files: Can't get information of '%s' because %m", src);
		return failed;
	}

	char * cpdst = strdup(dst);
	char * dirname_dst = dirname(cpdst);

	struct stat stdst;
	failed = lstat(dirname_dst, &stdst);
	if (failed != 0) {
		st_log_write(st_log_level_error, "Move files: Can't get information of '%s' because %m", dst);
	} else if (stsrc.st_dev != stdst.st_dev) {
		failed = st_file_cp_v1(src, dirname_dst);
		if (failed == 0)
			failed = st_file_rm_v1(src);
	} else {
		failed = rename(src, dst);
		if (failed != 0)
			st_log_write(st_log_level_error, "Move files: failed to rename '%s' to '%s' because %m", src, dst);
	}

	free(cpdst);

	return failed;
}

__asm__(".symver st_file_rm_v1, st_file_rm@@LIBSTONE_1.2");
int st_file_rm_v1(const char * path) {
	if (access(path, F_OK))
		return 0;

	struct stat st;
	if (lstat(path, &st))
		return -1;

	if (S_ISDIR(st.st_mode)) {
		struct dirent ** files;
		int nb_files = scandir(path, &files, st_file_basic_scandir_filter_v1, NULL);
		if (nb_files < 0)
			return -1;

		size_t path_length = strlen(path);

		int i, failed = 0;
		for (i = 0; i < nb_files; i++) {
			if (failed == 0) {
				size_t file_length = strlen(files[i]->d_name);
				char * file = malloc(path_length + file_length + 2);

				strcpy(file, path);
				file[path_length] = '/';
				strcpy(file + path_length + 1, files[i]->d_name);

				failed = st_file_rm_v1(file);

				free(file);
			}

			free(files[i]);
		}
		free(files);

		if (failed != 0)
			return failed;

		return rmdir(path);
	} else {
		return unlink(path);
	}
}

