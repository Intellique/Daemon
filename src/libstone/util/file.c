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

void st_util_file_convert_mode(char * buffer, mode_t mode) {
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

void st_util_file_convert_size_to_string(ssize_t size, char * str, ssize_t str_len) {
	unsigned short mult = 0;
	double tsize = size;

	while (tsize >= 1000 && mult < 4) {
		tsize /= 1024;
		mult++;
	}

	int fixed = 0;
	if (tsize < 0) {
		fixed = 3;
	} else if (tsize < 10) {
		fixed = 2;
	} else if (tsize < 100) {
		fixed = 1;
	}

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

int st_util_file_cp(const char * src, const char * dst) {
	struct stat stsrc;
	int failed = lstat(src, &stsrc);
	if (failed) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Copy files: Can't get information of '%s' because %m", src);
		return failed;
	}

	if (S_ISSOCK(stsrc.st_mode)) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "Copy files: Ignore socket file '%s'", src);
		return 0;
	}

	char * dst_file;
	if (!access(dst, F_OK)) {
		struct stat stdst;
		failed = lstat(dst, &stdst);
		if (failed) {
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Copy files: Can't get information of '%s' because %m", dst);
			return failed;
		}

		if (!S_ISDIR(stdst.st_mode)) {
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Copy files: '%s' should be a directory", dst);
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
		if (failed) {
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Copy files: Can't get information of '%s' because %m", dirname_dst);
			free(cpdst);
			return 2;
		}

		if (!S_ISDIR(stdst.st_mode)) {
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Copy files: '%s' should be a directory", dirname_dst);
			free(cpdst);
			return 2;
		}

		free(cpdst);

		dst_file = strdup(dst);
	}

	if (S_ISDIR(stsrc.st_mode)) {
		failed = st_util_file_mkdir(dst_file, stsrc.st_mode);

		struct dirent ** files = NULL;
		int nb_files = 0;
		if (!failed) {
			nb_files = scandir(src, &files, st_util_file_basic_scandir_filter, NULL);
			if (nb_files < 0)
				failed = 3;
		}

		int i;
		for (i = 0; i < nb_files; i++) {
			if (!failed) {
				char * new_src;
				asprintf(&new_src, "%s/%s", src, files[i]->d_name);

				failed = st_util_file_cp(new_src, dst_file);

				free(new_src);
			}

			free(files[i]);
		}

		free(files);
	} else if (S_ISREG(stsrc.st_mode)) {
		int fd_in = open(src, O_RDONLY);
		if (fd_in < 0) {
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Copy files: Can't open file '%s' for reading because %m", src);
			failed = 4;
		}

		int fd_out;
		if (!failed) {
			fd_out = open(dst_file, O_WRONLY | O_CREAT, stsrc.st_mode);
			if (fd_out < 0) {
				close(fd_in);
				st_log_write_all(st_log_level_error, st_log_type_daemon, "Copy files: Can't open file '%s' for writing because %m", dst_file);
				failed = 5;
			}
		}

		if (!failed) {
			ssize_t nb_read;
			char buffer[4096];
			while (nb_read = read(fd_in, buffer, 4096), nb_read > 0) {
				ssize_t nb_write = write(fd_out, buffer, nb_read);
				if (nb_write < 0) {
					st_log_write_all(st_log_level_error, st_log_type_daemon, "Copy files: Error while writing into '%s' because %m", dst_file);
					failed = 6;
					break;
				}
			}

			if (nb_read < 0) {
				st_log_write_all(st_log_level_error, st_log_type_daemon, "Copy files: Error while reading into '%s' because %m", src);
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
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Copy files: Can't read link of '%s' because %m", src);
			failed = 8;
		}

		if (!failed) {
			failed = symlink(link, dst_file);
			if (failed) {
				st_log_write_all(st_log_level_error, st_log_type_daemon, "Copy files: Failed to create symlink '%s' -> '%s' because %m", dst_file, link);
				failed = 9;
			}
		}
	} else if (S_ISFIFO(stsrc.st_mode)) {
		failed = mkfifo(dst_file, stsrc.st_mode);
		if (failed) {
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Copy files: Failed to create fifo '%s' because %m", dst_file);
			failed = 10;
		}
	} else if (S_ISBLK(stsrc.st_mode) || S_ISCHR(stsrc.st_mode)) {
		failed = mknod(dst_file, stsrc.st_mode, stsrc.st_rdev);
		if (failed) {
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Copy files: Failed to create device '%s' because %m", dst_file);
			failed = 10;
		}
	}

	free(dst_file);

	return failed;
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
	int failed = 0;
	if (ptr == NULL) {
		free(dir);
		failed = mkdir(dirname, mode);

		if (failed)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Error while create directory '%s' because %m", dirname);

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

	if (failed)
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Error while create directory '%s' because %m", dirname);

	unsigned short i;
	for (i = 0; i < nb && !failed; i++) {
		size_t length = strlen(dir);
		dir[length] = '/';

		failed = mkdir(dir, mode);

		if (failed)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Error while create directory '%s' because %m", dirname);
	}

	free(dir);

	return failed;
}

int st_util_file_mv(const char * src, const char * dst) {
	struct stat stsrc;
	int failed = lstat(src, &stsrc);
	if (failed) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Move files: Can't get information of '%s' because %m", src);
		return failed;
	}

	char * cpdst = strdup(dst);
	char * dirname_dst = dirname(cpdst);

	struct stat stdst;
	failed = lstat(dirname_dst, &stdst);
	if (failed) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Move files: Can't get information of '%s' because %m", dst);
	} else if (stsrc.st_dev != stdst.st_dev) {
		failed = st_util_file_cp(src, dirname_dst);
		if (!failed)
			failed = st_util_file_rm(src);
	} else {
		failed = rename(src, dst);
		if (failed)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Move files: failed to rename '%s' to '%s' because %m", src, dst);
	}

	free(cpdst);

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

