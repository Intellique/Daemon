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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Wed, 13 Feb 2013 16:31:06 +0100                         *
\*************************************************************************/

// asprintf, versionsort
#define _GNU_SOURCE
// scandir
#include <dirent.h>
// errno
#include <errno.h>
// fstatat
#include <fcntl.h>
// getgrgid_r
#include <grp.h>
// getpwuid_r
#include <pwd.h>
// free, malloc, realloc
#include <stdlib.h>
// snprintf
#include <stdio.h>
// memset, strcpy, strlen, strncpy
#include <string.h>
// bzero
#include <strings.h>
// lstat, stat
#include <sys/stat.h>
// getgrgid_r, getpwuid_r, fstatat, lstat, stat
#include <sys/types.h>
// time
#include <time.h>
// access, readlink, stat
#include <unistd.h>

#include <libstone/io.h>
#include <libstone/util/file.h>

#include "common.h"

struct st_format_tar_writer_private {
	struct st_stream_writer * io;
	off_t position;
	ssize_t size;
	int last_errno;

	// multi volume
	void * remain_header;
	ssize_t remain_size;
};

static enum st_format_writer_status st_format_tar_writer_add(struct st_format_writer * sfw, struct st_format_file * file);
static enum st_format_writer_status st_format_tar_writer_add_file(struct st_format_writer * sfw, const char * file);
static enum st_format_writer_status st_format_tar_writer_add_file_at(struct st_format_writer * sf, int dir_fd, const char * file);
static enum st_format_writer_status st_format_tar_writer_add_file_private(struct st_format_writer * sfw, const char * file, struct stat * sfile);
static enum st_format_writer_status st_format_tar_writer_add_label(struct st_format_writer * sfw, const char * label);
static int st_format_tar_writer_close(struct st_format_writer * sfw);
static void st_format_tar_writer_compute_checksum(const void * header, char * checksum);
static void st_format_tar_writer_compute_link(struct st_format_tar * header, char * link, const char * filename, ssize_t filename_length, char flag, struct stat * sfile, struct st_format_file * file);
static void st_format_tar_writer_compute_size(char * csize, long long size);
static ssize_t st_format_tar_writer_compute_size_of_file(struct st_format_writer * sf, const char * file, bool recursive);
static ssize_t st_format_tar_writer_end_of_file(struct st_format_writer * sfw);
static void st_format_tar_writer_free(struct st_format_writer * sfw);
static ssize_t st_format_tar_writer_get_available_size(struct st_format_writer * sfw);
static ssize_t st_format_tar_writer_get_block_size(struct st_format_writer * f);
static void st_format_tar_writer_gid2name(char * name, ssize_t length, gid_t uid);
static int st_format_tar_writer_last_errno(struct st_format_writer * f);
static void st_format_tar_writer_new_volume(struct st_format_writer * f, struct st_stream_writer * writer);
static ssize_t st_format_tar_writer_position(struct st_format_writer * f);
static int st_format_tar_writer_restart_file(struct st_format_writer * f, const char * filename, ssize_t position);
static const char * st_format_tar_writer_skip_leading_slash(const char * str);
static void st_format_tar_writer_uid2name(char * name, ssize_t length, uid_t uid);
static ssize_t st_format_tar_writer_write(struct st_format_writer * f, const void * data, ssize_t length);
static enum st_format_writer_status st_format_tar_writer_write_header(struct st_format_tar_writer_private * f, void * data, ssize_t length);

static struct st_format_writer_ops st_format_tar_writer_ops = {
	.add                  = st_format_tar_writer_add,
	.add_file             = st_format_tar_writer_add_file,
	.add_file_at          = st_format_tar_writer_add_file_at,
	.add_label            = st_format_tar_writer_add_label,
	.close                = st_format_tar_writer_close,
	.compute_size_of_file = st_format_tar_writer_compute_size_of_file,
	.end_of_file          = st_format_tar_writer_end_of_file,
	.free                 = st_format_tar_writer_free,
	.get_available_size   = st_format_tar_writer_get_available_size,
	.get_block_size       = st_format_tar_writer_get_block_size,
	.last_errno           = st_format_tar_writer_last_errno,
	.new_volume           = st_format_tar_writer_new_volume,
	.position             = st_format_tar_writer_position,
	.restart_file         = st_format_tar_writer_restart_file,
	.write                = st_format_tar_writer_write,
};


ssize_t st_format_tar_get_size(const char * path, bool recursive) {
	struct stat sfile;
	if (lstat(path, &sfile))
		return -1;

	const char * path2 = st_format_tar_writer_skip_leading_slash(path);
	int path_length = strlen(path2);

	ssize_t file_size = 512;
	if (S_ISLNK(sfile.st_mode)) {
		char link[257];
		ssize_t link_length = readlink(path, link, 256);
		link[link_length] = '\0';

		if (path_length > 100 && link_length > 100) {
			file_size += 2048 + path_length - path_length % 512 + link_length - link_length % 512;
		} else if (path_length > 100) {
			file_size += 1024 + path_length - path_length % 512;
		} else if (link_length > 100) {
			file_size += 1024 + link_length - link_length % 512;
		}
	} else if (path_length > 100) {
		file_size += 512 + path_length - path_length % 512;
	}

	if (S_ISREG(sfile.st_mode))
		file_size += 512 + sfile.st_size - sfile.st_size % 512;
	else if (recursive && S_ISDIR(sfile.st_mode)) {
		struct dirent ** dl = NULL;
		int nb_paths = scandir(path, &dl, st_util_file_basic_scandir_filter, versionsort);
		int i;
		for (i = 0; i < nb_paths; i++) {
			char * subpath = NULL;
			asprintf(&subpath, "%s/%s", path, dl[i]->d_name);

			file_size += st_format_tar_get_size(subpath, recursive);

			free(subpath);
			free(dl[i]);
		}

		free(dl);
	}

	return file_size;
}

struct st_format_writer * st_format_tar_get_writer(struct st_stream_writer * writer) {
	if (writer == NULL)
		return NULL;

	struct st_format_tar_writer_private * data = malloc(sizeof(struct st_format_tar_writer_private));
	data->io = writer;
	data->position = 0;
	data->size = 0;
	data->last_errno = 0;
	data->remain_header = 0;
	data->remain_size = 0;

	struct st_format_writer * self = malloc(sizeof(struct st_format_writer));
	self->ops  = &st_format_tar_writer_ops;
	self->data = data;

	return self;
}

static enum st_format_writer_status st_format_tar_writer_add(struct st_format_writer * f, struct st_format_file * file) {
	ssize_t block_size = 512;
	struct st_format_tar * header = malloc(block_size);
	struct st_format_tar * current_header = header;

	int filename_length = strlen(file->filename);
	if (S_ISLNK(file->mode)) {
		int link_length = strlen(file->link);
		if (filename_length > 100 && link_length > 100) {
			block_size += 2048 + filename_length - filename_length % 512 + link_length - link_length % 512;
			current_header = header = realloc(header, block_size);

			bzero(current_header, block_size - 512);
			st_format_tar_writer_compute_link(current_header, (char *) (current_header + 1), file->link, link_length, 'K', NULL, file);
			st_format_tar_writer_compute_link(current_header + 2, (char *) (current_header + 3), file->filename, filename_length, 'L', NULL, file);

			current_header += 4;
		} else if (filename_length > 100) {
			block_size += 1024 + filename_length - filename_length % 512;
			current_header = header = realloc(header, block_size);

			bzero(current_header, block_size - 512);
			st_format_tar_writer_compute_link(current_header, (char *) (current_header + 1), file->filename, filename_length, 'L', NULL, file);

			current_header += 2;
		} else if (link_length > 100) {
			block_size += 1024 + link_length - link_length % 512;
			current_header = header = realloc(header, block_size);
			bzero(current_header, block_size - 512);
			st_format_tar_writer_compute_link(current_header, (char *) (current_header + 1), file->link, link_length, 'K', NULL, file);

			current_header += 2;
		}
	}

	struct st_format_tar_writer_private * format = f->data;
	bzero(current_header, 512);
	strncpy(current_header->filename, file->filename, 100);
	snprintf(current_header->filemode, 8, "%07o", file->mode & 07777);
	snprintf(current_header->uid, 8, "%07o", file->uid);
	snprintf(current_header->gid, 8, "%07o", file->gid);
	snprintf(current_header->mtime, 12, "%0*o", 11, (unsigned int) file->mtime);

	if (S_ISREG(file->mode)) {
		st_format_tar_writer_compute_size(current_header->size, file->size);
		current_header->flag = '0';
		format->size = file->size;
	} else if (S_ISLNK(file->mode)) {
		current_header->flag = '2';
		format->size = 0;
	} else if (S_ISCHR(file->mode)) {
		current_header->flag = '3';
		snprintf(current_header->devmajor, 8, "%07o", (unsigned int) (file->rdev >> 8));
		snprintf(current_header->devminor, 8, "%07o", (unsigned int) (file->rdev & 0xFF));
		format->size = 0;
	} else if (S_ISBLK(file->mode)) {
		current_header->flag = '4';
		snprintf(current_header->devmajor, 8, "%07o", (unsigned int) (file->rdev >> 8));
		snprintf(current_header->devminor, 8, "%07o", (unsigned int) (file->rdev & 0xFF));
		format->size = 0;
	} else if (S_ISDIR(file->mode)) {
		current_header->flag = '5';
		format->size = 0;
	} else if (S_ISFIFO(file->mode)) {
		current_header->flag = '6';
		format->size = 0;
	}

	strcpy(current_header->magic, "ustar  ");
	st_format_tar_writer_uid2name(current_header->uname, 32, file->uid);
	st_format_tar_writer_gid2name(current_header->gname, 32, file->gid);

	st_format_tar_writer_compute_checksum(current_header, current_header->checksum);

	format->position = 0;

	return st_format_tar_writer_write_header(format, header, block_size);
}

static enum st_format_writer_status st_format_tar_writer_add_file(struct st_format_writer * sf, const char * file) {
	struct stat sfile;
	if (lstat(file, &sfile))
		return st_format_writer_error;

	return st_format_tar_writer_add_file_private(sf, file, &sfile);
}

static enum st_format_writer_status st_format_tar_writer_add_file_at(struct st_format_writer * sf, int dir_fd, const char * file) {
	struct stat sfile;
	if (fstatat(dir_fd, file, &sfile, AT_SYMLINK_NOFOLLOW))
		return st_format_writer_error;

	return st_format_tar_writer_add_file_private(sf, file, &sfile);
}

static enum st_format_writer_status st_format_tar_writer_add_file_private(struct st_format_writer * sfw, const char * file, struct stat * sfile) {
	ssize_t block_size = 512;
	struct st_format_tar * header = malloc(block_size);
	struct st_format_tar * current_header = header;

	const char * filename2 = st_format_tar_writer_skip_leading_slash(file);
	int filename_length = strlen(filename2);
	if (S_ISLNK(sfile->st_mode)) {
		char link[257];
		ssize_t link_length = readlink(file, link, 256);
		link[link_length] = '\0';

		if (filename_length > 100 && link_length > 100) {
			block_size += 2048 + filename_length - filename_length % 512 + link_length - link_length % 512;
			current_header = header = realloc(header, block_size);

			bzero(current_header, block_size - 512);
			st_format_tar_writer_compute_link(current_header, (char *) (current_header + 1), link, link_length, 'K', sfile, NULL);
			st_format_tar_writer_compute_link(current_header + 2, (char *) (current_header + 3), filename2, filename_length, 'L', sfile, NULL);

			current_header += 4;
		} else if (filename_length > 100) {
			block_size += 1024 + filename_length - filename_length % 512;
			current_header = header = realloc(header, block_size);

			bzero(current_header, block_size - 512);
			st_format_tar_writer_compute_link(current_header, (char *) (current_header + 1), filename2, filename_length, 'L', sfile, NULL);

			current_header += 2;
		} else if (link_length > 100) {
			block_size += 1024 + link_length - link_length % 512;
			current_header = header = realloc(header, block_size);

			bzero(current_header, block_size - 512);
			st_format_tar_writer_compute_link(current_header, (char *) (current_header + 1), link, link_length, 'K', sfile, NULL);

			current_header += 2;
		}
	} else if (filename_length > 100) {
		block_size += 1024 + filename_length - filename_length % 512;
		current_header = header = realloc(header, block_size);

		bzero(current_header, 1024);
		st_format_tar_writer_compute_link(current_header, (char *) (current_header + 1), filename2, filename_length, 'L', sfile, NULL);

		current_header += 2;
	}

	struct st_format_tar_writer_private * format = sfw->data;
	bzero(current_header, 512);
	strncpy(current_header->filename, filename2, 100);
	snprintf(current_header->filemode, 8, "%07o", sfile->st_mode & 07777);
	snprintf(current_header->uid, 8, "%07o", sfile->st_uid);
	snprintf(current_header->gid, 8, "%07o", sfile->st_gid);
	snprintf(current_header->mtime, 12, "%0*o", 11, (unsigned int) sfile->st_mtime);

	if (S_ISREG(sfile->st_mode)) {
		st_format_tar_writer_compute_size(current_header->size, sfile->st_size);
		current_header->flag = '0';
		format->size = sfile->st_size;
	} else if (S_ISLNK(sfile->st_mode)) {
		current_header->flag = '2';
		readlink(file, current_header->linkname, 100);
		format->size = 0;
	} else if (S_ISCHR(sfile->st_mode)) {
		current_header->flag = '3';
		snprintf(current_header->devmajor, 8, "%07o", (unsigned int) (sfile->st_rdev >> 8));
		snprintf(current_header->devminor, 8, "%07o", (unsigned int) (sfile->st_rdev & 0xFF));
		format->size = 0;
	} else if (S_ISBLK(sfile->st_mode)) {
		current_header->flag = '4';
		snprintf(current_header->devmajor, 8, "%07o", (unsigned int) (sfile->st_rdev >> 8));
		snprintf(current_header->devminor, 8, "%07o", (unsigned int) (sfile->st_rdev & 0xFF));
		format->size = 0;
	} else if (S_ISDIR(sfile->st_mode)) {
		current_header->flag = '5';
		format->size = 0;
	} else if (S_ISFIFO(sfile->st_mode)) {
		current_header->flag = '6';
		format->size = 0;
	}

	strcpy(current_header->magic, "ustar  ");
	st_format_tar_writer_uid2name(current_header->uname, 32, sfile->st_uid);
	st_format_tar_writer_gid2name(current_header->gname, 32, sfile->st_gid);

	st_format_tar_writer_compute_checksum(current_header, current_header->checksum);

	format->position = 0;

	return st_format_tar_writer_write_header(format, header, block_size);
}

static enum st_format_writer_status st_format_tar_writer_add_label(struct st_format_writer * f, const char * label) {
	if (label == NULL)
		return st_format_writer_error;

	struct st_format_tar * header = malloc(512);
	bzero(header, 512);
	strncpy(header->filename, label, 100);
	snprintf(header->mtime, 12, "%0*o", 11, (unsigned int) time(0));
	header->flag = 'V';

	st_format_tar_writer_compute_checksum(header, header->checksum);

	struct st_format_tar_writer_private * format = f->data;
	format->position = 0;
	format->size = 0;

	return st_format_tar_writer_write_header(format, header, 512);
}

static int st_format_tar_writer_close(struct st_format_writer * f) {
	struct st_format_tar_writer_private * format = f->data;
	return format->io->ops->close(format->io);
}

static void st_format_tar_writer_compute_checksum(const void * header, char * checksum) {
	const unsigned char * ptr = header;

	memset(checksum, ' ', 8);

	unsigned int i, sum = 0;
	for (i = 0; i < 512; i++)
		sum += ptr[i];

	snprintf(checksum, 7, "%06o", sum);
}

static void st_format_tar_writer_compute_link(struct st_format_tar * header, char * link, const char * filename, ssize_t filename_length, char flag, struct stat * sfile, struct st_format_file * file) {
	strcpy(header->filename, "././@LongLink");
	memset(header->filemode, '0', 7);
	memset(header->uid, '0', 7);
	memset(header->gid, '0', 7);
	st_format_tar_writer_compute_size(header->size, filename_length);
	memset(header->mtime, '0', 11);
	header->flag = flag;
	strcpy(header->magic, "ustar  ");
	if (sfile != NULL) {
		st_format_tar_writer_uid2name(header->uname, 32, sfile->st_uid);
		st_format_tar_writer_gid2name(header->gname, 32, sfile->st_gid);
	} else {
		st_format_tar_writer_uid2name(header->uname, 32, file->uid);
		st_format_tar_writer_gid2name(header->gname, 32, file->gid);
	}

	st_format_tar_writer_compute_checksum(header, header->checksum);

	strcpy(link, filename);
}

static void st_format_tar_writer_compute_size(char * csize, long long size) {
	if (size > 077777777777) {
		*csize = (char) 0x80;
		unsigned int i;
		for (i = 11; i > 0; i--) {
			csize[i] = size & 0xFF;
			size >>= 8;
		}
	} else {
		snprintf(csize, 12, "%0*llo", 11, size);
	}
}

static ssize_t st_format_tar_writer_compute_size_of_file(struct st_format_writer * sf __attribute__((unused)), const char * file, bool recursive) {
	return st_format_tar_get_size(file, recursive);
}

static ssize_t st_format_tar_writer_end_of_file(struct st_format_writer * sfw) {
	struct st_format_tar_writer_private * self = sfw->data;

	unsigned short mod = self->position % 512;
	if (mod > 0) {
		size_t length = 512 - mod;
		char * buffer = malloc(length);
		bzero(buffer, length);

		ssize_t nb_write = self->io->ops->write(self->io, buffer, length);
		free(buffer);

		return nb_write;
	}

	return 0;
}

static void st_format_tar_writer_free(struct st_format_writer * f) {
	struct st_format_tar_writer_private * self = f->data;

	if (self->io) {
		self->io->ops->close(self->io);
		self->io->ops->free(self->io);
	}

	free(f->data);
	free(f);
}

static ssize_t st_format_tar_writer_get_available_size(struct st_format_writer * f) {
	struct st_format_tar_writer_private * format = f->data;
	return format->io->ops->get_available_size(format->io);
}

static ssize_t st_format_tar_writer_get_block_size(struct st_format_writer * f) {
	struct st_format_tar_writer_private * format = f->data;
	return format->io->ops->get_block_size(format->io);
}

static void st_format_tar_writer_gid2name(char * name, ssize_t length, gid_t uid) {
	char * buffer = malloc(512);

	struct group gr;
	struct group * tmp_gr;

	if (!getgrgid_r(uid, &gr, buffer, 512, &tmp_gr)) {
		strncpy(name, gr.gr_name, length);
		name[length - 1] = '\0';
	} else {
		snprintf(name, length, "%d", uid);
	}

	free(buffer);
}

static int st_format_tar_writer_last_errno(struct st_format_writer * f) {
	struct st_format_tar_writer_private * format = f->data;
	if (format->last_errno)
		return format->last_errno;
	return format->io->ops->last_errno(format->io);
}

static void st_format_tar_writer_new_volume(struct st_format_writer * f, struct st_stream_writer * writer) {
	struct st_format_tar_writer_private * format = f->data;

	if (format->io != NULL)
		format->io->ops->free(format->io);
	format->io = writer;

	if (format->remain_size > 0) {
		writer->ops->write(writer, format->remain_header, format->remain_size);

		free(format->remain_header);
		format->remain_header = 0;
		format->remain_size = 0;
	}
}

static ssize_t st_format_tar_writer_position(struct st_format_writer * f) {
	struct st_format_tar_writer_private * format = f->data;
	return format->io->ops->position(format->io);
}

static int st_format_tar_writer_restart_file(struct st_format_writer * f, const char * filename, ssize_t position) {
	struct stat sfile;
	if (lstat(filename, &sfile))
		return 1;

	ssize_t block_size = 512;
	struct st_format_tar * header = malloc(block_size);
	struct st_format_tar * current_header = header;

	struct st_format_tar_writer_private * format = f->data;
	bzero(current_header, 512);
	strncpy(current_header->filename, st_format_tar_writer_skip_leading_slash(filename), 100);
	st_format_tar_writer_compute_size(current_header->size, sfile.st_size - position);
	current_header->flag = 'M';
	st_format_tar_writer_uid2name(header->uname, 32, sfile.st_uid);
	st_format_tar_writer_gid2name(header->gname, 32, sfile.st_gid);
	st_format_tar_writer_compute_size(current_header->position, position);

	st_format_tar_writer_compute_checksum(current_header, current_header->checksum);

	format->position = 0;
	format->size = position;

	if (block_size > format->io->ops->get_available_size(format->io)) {
		format->last_errno = ENOSPC;
		return -1;
	}

	ssize_t nbWrite = format->io->ops->write(format->io, header, block_size);

	free(header);

	return block_size != nbWrite ? -1 : 0;
}

static const char * st_format_tar_writer_skip_leading_slash(const char * str) {
	if (!str)
		return 0;

	const char * ptr;
	size_t i = 0, length = strlen(str);
	for (ptr = str; *ptr == '/' && i < length; i++, ptr++);

	return ptr;
}

static void st_format_tar_writer_uid2name(char * name, ssize_t length, uid_t uid) {
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

static ssize_t st_format_tar_writer_write(struct st_format_writer * f, const void * data, ssize_t length) {
	struct st_format_tar_writer_private * format = f->data;

	if (format->position >= format->size)
		return 0;

	if (format->position + length > format->size)
		length = format->size - format->position;

	ssize_t nb_write = format->io->ops->write(format->io, data, length);

	if (nb_write > 0)
		format->position += nb_write;

	return nb_write;
}

enum st_format_writer_status st_format_tar_writer_write_header(struct st_format_tar_writer_private * f, void * data, ssize_t length) {
	ssize_t available = f->io->ops->get_available_size(f->io);

	ssize_t nb_write = 0;

	if (available > 0)
		nb_write = f->io->ops->write(f->io, data, length);

	int last_errno = 0;
	if (nb_write < 0) {
		last_errno = f->io->ops->last_errno(f->io);

		if (last_errno != ENOSPC)
			return st_format_writer_error;

		nb_write = 0;
	}

	if (nb_write == 0) {
		f->remain_size = length;
		f->remain_header = data;

		f->io->ops->close(f->io);

		return st_format_writer_end_of_volume;
	} else if (length > nb_write) {
		f->remain_size = length - nb_write;
		f->remain_header = malloc(f->remain_size);
		memcpy(f->remain_header, data, f->remain_size);
		free(data);

		f->io->ops->close(f->io);

		return st_format_writer_end_of_volume;
	}

	free(data);

	return st_format_writer_ok;
}

