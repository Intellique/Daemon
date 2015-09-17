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
// scandir
#include <dirent.h>
// fcntl, open
#include <fcntl.h>
// getgrgid_r
#include <grp.h>
// basename, dirname
#include <libgen.h>
// dgettext
#include <libintl.h>
// localeconv
#include <locale.h>
// getpwuid_r
#include <pwd.h>
// asprintf, rename, snprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// memmove, strchr, strcpy, strdup, strlen, strrchr, strstr
#include <string.h>
// fstat, lstat, mkdir, open
#include <sys/stat.h>
// fstat, lstat, mkdir, mkfifo, mknod, open
#include <sys/types.h>
// access, close, fcntl, fstat, lstat, mkfifo, read, readlink, rmdir, symlink, unlink, write
#include <unistd.h>

#include <libstoriqone/file.h>
#include <libstoriqone/log.h>
#include <libstoriqone/string.h>

int so_file_basic_scandir_filter(const struct dirent * d) {
	if (d->d_name[0] != '.')
		return 1;

	if (d->d_name[1] == '\0')
		return 0;

	return d->d_name[1] != '.' || d->d_name[2] != '\0';
}

bool so_file_check_link(const char * file) {
	if (access(file, F_OK))
		return false;

	char link[256];
	ssize_t link_length = readlink(file, link, 256);

	if (link_length < 0)
		return false;

	return !access(link, F_OK);
}

bool so_file_close_fd_on_exec(int fd, bool close) {
	int flag = fcntl(fd, F_GETFD);

	if (close)
		flag |= FD_CLOEXEC;
	else
		flag &= !FD_CLOEXEC;

	return fcntl(fd, F_SETFD, flag) == 0;
}

ssize_t so_file_compute_size(const char * file, bool recursive) {
	struct stat st;
	int failed = stat(file, &st);
	if (failed != 0)
		return -1;

	if (S_ISREG(st.st_mode))
		return st.st_size;
	else if (recursive && S_ISDIR(st.st_mode)) {
		struct dirent ** files;
		int nb_files = scandir(file, &files, so_file_basic_scandir_filter, NULL);
		if (nb_files < 0)
			return -1;

		size_t file_length = strlen(file);
		ssize_t total = 0;

		int i;
		bool failed = false;
		for (i = 0; i < nb_files; i++) {
			if (!failed) {
				size_t sub_file_length = strlen(files[i]->d_name);
				char * sub_file = malloc(file_length + sub_file_length + 2);

				strcpy(sub_file, file);
				sub_file[file_length] = '/';
				strcpy(sub_file + file_length + 1, files[i]->d_name);

				ssize_t sub_total = so_file_compute_size(sub_file, true);
				if (sub_total < 0)
					failed = true;
				else
					total += sub_total;

				free(sub_file);
			}

			free(files[i]);
		}
		free(files);

		if (failed)
			return -1;

		return total;
	}

	return 0;
}

void so_file_convert_mode(char * buffer, mode_t mode) {
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

void so_file_convert_size_to_string(size_t size, char * str, ssize_t str_len) {
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
			snprintf(str, str_len, dgettext("libstoriqone", "%zd B"), size);
			break;
		case 1:
			snprintf(str, str_len, dgettext("libstoriqone", "%.*f KB"), fixed, tsize);
			break;
		case 2:
			snprintf(str, str_len, dgettext("libstoriqone", "%.*f MB"), fixed, tsize);
			break;
		case 3:
			snprintf(str, str_len, dgettext("libstoriqone", "%.*f GB"), fixed, tsize);
			break;
		default:
			snprintf(str, str_len, dgettext("libstoriqone", "%.*f TB"), fixed, tsize);
	}

	struct lconv * locale_info = localeconv();
	if (strstr(str, locale_info->decimal_point) != NULL) {
		char * ptrEnd = strchr(str, ' ');
		char * ptrBegin = ptrEnd - 1;
		while (*ptrBegin == '0')
			ptrBegin--;

		char * ptrTmp = ptrBegin;
		while (so_string_valid_utf8_char(ptrTmp) == 0)
			ptrTmp--;

		if (strncmp(ptrTmp, locale_info->decimal_point, strlen(locale_info->decimal_point)) == 0)
			ptrBegin = ptrTmp - 1;

		if (ptrBegin + 1 < ptrEnd)
			memmove(ptrBegin + 1, ptrEnd, strlen(ptrEnd) + 1);
	}
}

int so_file_cp(const char * src, const char * dst) {
	struct stat stsrc;
	int failed = lstat(src, &stsrc);
	if (failed != 0) {
		so_log_write(so_log_level_error,
			dgettext("libstoriqone", "so_file_cp: can't get information of '%s' because %m"),
			src);
		return failed;
	}

	if (S_ISSOCK(stsrc.st_mode)) {
		so_log_write(so_log_level_info,
			dgettext("libstoriqone", "so_file_cp: ignoring socket file '%s'"),
			src);
		return 0;
	}

	char * dst_file = NULL;
	if (!access(dst, F_OK)) {
		struct stat stdst;
		failed = lstat(dst, &stdst);
		if (failed != 0) {
			so_log_write(so_log_level_error,
				dgettext("libstoriqone", "so_file_cp: can't get information of '%s' because %m"),
				dst);
			return failed;
		}

		if (!S_ISDIR(stdst.st_mode)) {
			so_log_write(so_log_level_error,
				dgettext("libstoriqone", "so_file_cp: '%s' should be a directory"),
				dst);
			return 2;
		}

		char * cpsrc = strdup(src);
		char * basename_src = basename(cpsrc);

		int size = asprintf(&dst_file, "%s/%s", dst, basename_src);

		free(cpsrc);
		cpsrc = basename_src = NULL;

		if (size < 0)
			return 2;
	} else {
		char * cpdst = strdup(dst);
		char * dirname_dst = dirname(cpdst);

		struct stat stdst;
		failed = lstat(dirname_dst, &stdst);
		if (failed != 0) {
			so_log_write(so_log_level_error,
				dgettext("libstoriqone", "so_file_cp: can't get information of '%s' because %m"),
				dirname_dst);
			free(cpdst);
			return 2;
		}

		if (!S_ISDIR(stdst.st_mode)) {
			so_log_write(so_log_level_error,
				dgettext("libstoriqone", "so_file_cp: '%s' should be a directory"),
				dirname_dst);
			free(cpdst);
			return 2;
		}

		free(cpdst);

		dst_file = strdup(dst);
	}

	if (S_ISDIR(stsrc.st_mode)) {
		failed = so_file_mkdir(dst_file, stsrc.st_mode);

		struct dirent ** files = NULL;
		int nb_files = 0;
		if (failed == 0) {
			nb_files = scandir(src, &files, so_file_basic_scandir_filter, versionsort);
			if (nb_files < 0) {
				so_log_write(so_log_level_error,
					dgettext("libstoriqone", "so_file_cp: error while scanning directory '%s' because %m"),
					src);
				failed = 3;
			}
		}

		int i;
		for (i = 0; i < nb_files; i++) {
			if (failed == 0) {
				char * new_src = NULL;
				int size = asprintf(&new_src, "%s/%s", src, files[i]->d_name);

				if (size > 0)
					failed = so_file_cp(new_src, dst_file);
				else
					failed = 1;

				free(new_src);
			}

			free(files[i]);
		}

		free(files);
	} else if (S_ISREG(stsrc.st_mode)) {
		int fd_in = open(src, O_RDONLY);
		if (fd_in < 0) {
			so_log_write(so_log_level_error,
				dgettext("libstoriqone", "so_file_cp: can't open file '%s' for reading because %m"),
				src);
			failed = 4;
		}

		int fd_out;
		if (failed == 0) {
			fd_out = open(dst_file, O_WRONLY | O_CREAT, stsrc.st_mode);
			if (fd_out < 0) {
				close(fd_in);
				so_log_write(so_log_level_error,
					dgettext("libstoriqone", "so_file_cp: can't open file '%s' for writing because %m"),
					dst_file);
				failed = 5;
			}
		}

		if (failed == 0) {
			ssize_t nb_read;
			char buffer[4096];
			while (nb_read = read(fd_in, buffer, 4096), nb_read > 0) {
				ssize_t nb_write = write(fd_out, buffer, nb_read);
				if (nb_write < 0) {
					so_log_write(so_log_level_error,
						dgettext("libstoriqone", "so_file_cp: error while writing into '%s' because %m"),
						dst_file);
					failed = 6;
					break;
				}
			}

			if (nb_read < 0) {
				so_log_write(so_log_level_error,
					dgettext("libstoriqone", "so_file_cp: error while reading from '%s' because %m"),
					src);
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
			so_log_write(so_log_level_error,
				dgettext("libstoriqone", "so_file_cp: can't read link of '%s' because %m"),
				src);
			failed = 8;
		}

		if (failed == 0) {
			failed = symlink(link, dst_file);
			if (failed != 0) {
				so_log_write(so_log_level_error,
					dgettext("libstoriqone", "so_file_cp: failed to create symlink '%s' -> '%s' because %m"),
					dst_file, link);
				failed = 9;
			}
		}
	} else if (S_ISFIFO(stsrc.st_mode)) {
		failed = mkfifo(dst_file, stsrc.st_mode);
		if (failed != 0) {
			so_log_write(so_log_level_error,
				dgettext("libstoriqone", "so_file_cp: failed to create fifo '%s' because %m"),
				dst_file);
			failed = 10;
		}
	} else if (S_ISBLK(stsrc.st_mode) || S_ISCHR(stsrc.st_mode)) {
		failed = mknod(dst_file, stsrc.st_mode, stsrc.st_rdev);
		if (failed != 0) {
			so_log_write(so_log_level_error,
				dgettext("libstoriqone", "so_file_cp: failed to create device '%s' because %m"),
				dst_file);
			failed = 10;
		}
	}

	free(dst_file);

	return failed;
}

char * so_file_gid2name(gid_t gid) {
	char buffer[128];

	struct group gr;
	struct group * tmp_gr;

	if (!getgrgid_r(gid, &gr, buffer, 512, &tmp_gr))
		return strdup(buffer);
	else {
		char * name = NULL;
		int size = asprintf(&name, "%d", gid);
		if (size > 0)
			return name;
		else
			return NULL;
	}
}

int so_file_mkdir(const char * dirname, mode_t mode) {
	if (!access(dirname, F_OK))
		return 0;

	char * dir = strdup(dirname);
	so_string_delete_double_char(dir, '/');
	so_string_rtrim(dir, '/');

	char * ptr = strrchr(dir, '/');
	int failed = 0;
	if (ptr == NULL) {
		free(dir);
		failed = mkdir(dirname, mode);

		if (failed != 0)
			so_log_write(so_log_level_error,
				dgettext("libstoriqone", "so_file_mkdir: error while creating directory '%s' because %m"),
				dirname);

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
		so_log_write(so_log_level_error,
			dgettext("libstoriqone", "so_file_mkdir: error while creating directory '%s' because %m"),
			dirname);

	unsigned short i;
	for (i = 0; i < nb && !failed; i++) {
		size_t length = strlen(dir);
		dir[length] = '/';

		failed = mkdir(dir, mode);

		if (failed != 0)
			so_log_write(so_log_level_error,
				dgettext("libstoriqone", "so_file_mkdir: error while creating directory '%s' because %m"),
				dirname);
	}

	free(dir);

	return failed;
}

int so_file_mv(const char * src, const char * dst) {
	struct stat stsrc;
	int failed = lstat(src, &stsrc);
	if (failed != 0) {
		so_log_write(so_log_level_error,
			dgettext("libstoriqone", "so_file_mv: can't get information of '%s' because %m"),
			src);
		return failed;
	}

	char * cpdst = strdup(dst);
	char * dirname_dst = dirname(cpdst);

	struct stat stdst;
	failed = lstat(dirname_dst, &stdst);
	if (failed != 0) {
		so_log_write(so_log_level_error,
			dgettext("libstoriqone", "so_file_mv: can't get information of '%s' because %m"),
			dst);
	} else if (stsrc.st_dev != stdst.st_dev) {
		failed = so_file_cp(src, dirname_dst);
		if (failed == 0)
			failed = so_file_rm(src);
	} else {
		failed = rename(src, dst);
		if (failed != 0)
			so_log_write(so_log_level_error,
				dgettext("libstoriqone", "so_file_mv: failed to rename '%s' to '%s' because %m"),
				src, dst);
	}

	free(cpdst);

	return failed;
}

char * so_file_read_all_from(const char * filename) {
	int fd = open(filename, O_RDONLY);
	if (fd < 0) {
		so_log_write(so_log_level_error,
			dgettext("libstoriqone", "so_file_read_all_from: failed to open '%s' because %m"),
			filename);
		return NULL;
	}

	struct stat st;
	int failed = fstat(fd, &st);
	if (failed != 0 || st.st_size == 0) {
		close(fd);
		return NULL;
	}

	char * data = malloc(st.st_size + 1);

	ssize_t nb_read = read(fd, data, st.st_size);
	if (nb_read < 0) {
		so_log_write(so_log_level_error,
			dgettext("libstoriqone", "so_file_read_all_from: error while reading from file '%s' because %m"),
			filename);
		return NULL;
	}

	data[nb_read] = '\0';

	close(fd);

	if (nb_read < 0) {
		free(data);
		data = NULL;
		so_log_write(so_log_level_error,
			dgettext("libstoriqone", "so_file_read_all_from: failed while reading from '%s' because %m"),
			filename);
	}

	return data;
}

char * so_file_rename(const char * filename) {
	char * path = strdup(filename);

	if (access(path, F_OK) == 0) {
		char * extension = strrchr(path, '.');

		char * old_path = path;
		path = NULL;

		if (extension != NULL)
			*extension = '\0';

		unsigned int next = 0;
		do {
			free(path);
			path = NULL;

			int size;
			if (extension != NULL)
				size = asprintf(&path, "%s_%u.%s", old_path, next, extension + 1);
			else
				size = asprintf(&path, "%s_%u", old_path, next);

			if (size < 0) {
				free(path);
				return NULL;
			}

			next++;
		} while (access(path, F_OK) == 0);

		free(old_path);
	}

	return path;
}

int so_file_rm(const char * path) {
	if (access(path, F_OK))
		return 0;

	struct stat st;
	if (lstat(path, &st)) {
		so_log_write(so_log_level_error,
			dgettext("libstoriqone", "so_file_rm: failed to get information of '%s' because %m"),
			path);
		return -1;
	}

	if (S_ISDIR(st.st_mode)) {
		struct dirent ** files;
		int nb_files = scandir(path, &files, so_file_basic_scandir_filter, NULL);
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

				failed = so_file_rm(file);

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

char * so_file_uid2name(uid_t uid) {
	char buffer[128];

	struct passwd pw;
	struct passwd * tmp_pw;

	if (!getpwuid_r(uid, &pw, buffer, 512, &tmp_pw))
		return strdup(buffer);
	else {
		char * name = NULL;
		int size = asprintf(&name, "%d", uid);
		if (size > 0)
			return name;
		else
			return NULL;
	}
}

