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
// errno
#include <errno.h>
// open
#include <fcntl.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>
// open, stat
#include <sys/stat.h>
// lseek, open, stat
#include <sys/types.h>
// statfs
#include <sys/statvfs.h>
// fchown, lseek, stat, write
#include <unistd.h>

#include <libstoriqone/format.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/io.h>

struct soj_format_writer_filesystem_private {
	char * base_dir;

	int fd;
	int last_errno;
	ssize_t block_size;
};

static enum so_format_writer_status soj_format_writer_filesystem_add_file(struct so_format_writer * fw, const struct so_format_file * file);
static enum so_format_writer_status soj_format_writer_filesystem_add_label(struct so_format_writer * fw, const char * label);
static int soj_format_writer_filesystem_close(struct so_format_writer * fw);
static ssize_t soj_format_writer_filesystem_end_of_file(struct so_format_writer * fw);
static int soj_format_writer_filesystem_file_position(struct so_format_writer * fw);
static void soj_format_writer_filesystem_free(struct so_format_writer * fw);
static ssize_t soj_format_writer_filesystem_get_available_size(struct so_format_writer * fw);
static ssize_t soj_format_writer_filesystem_get_block_size(struct so_format_writer * fw);
static struct so_value * soj_format_writer_filesystem_get_digests(struct so_format_writer * fw);
static int soj_format_writer_filesystem_last_errno(struct so_format_writer * fw);
static ssize_t soj_format_writer_filesystem_position(struct so_format_writer * fw);
static enum so_format_writer_status soj_format_writer_filesystem_restart_file(struct so_format_writer * fw, const struct so_format_file * file, ssize_t position);
static ssize_t soj_format_writer_filesystem_write(struct so_format_writer * fw, const void * buffer, ssize_t length);

static struct so_format_writer_ops soj_format_writer_filesystem_ops = {
	.add_file           = soj_format_writer_filesystem_add_file,
	.add_label          = soj_format_writer_filesystem_add_label,
	.close              = soj_format_writer_filesystem_close,
	.end_of_file        = soj_format_writer_filesystem_end_of_file,
	.file_position      = soj_format_writer_filesystem_file_position,
	.free               = soj_format_writer_filesystem_free,
	.get_available_size = soj_format_writer_filesystem_get_available_size,
	.get_block_size     = soj_format_writer_filesystem_get_block_size,
	.get_digests        = soj_format_writer_filesystem_get_digests,
	.last_errno         = soj_format_writer_filesystem_last_errno,
	.position           = soj_format_writer_filesystem_position,
	.restart_file       = soj_format_writer_filesystem_restart_file,
	.write              = soj_format_writer_filesystem_write,
};


struct so_format_writer * soj_io_filesystem_writer(const char * path) {
	struct soj_format_writer_filesystem_private * self = malloc(sizeof(struct soj_format_writer_filesystem_private));
	bzero(self, sizeof(struct soj_format_writer_filesystem_private));
	self->base_dir = strdup(path);
	self->fd = -1;

	struct so_format_writer * writer = malloc(sizeof(struct so_format_writer));
	writer->ops = &soj_format_writer_filesystem_ops;
	writer->data = self;

	return writer;
}


static enum so_format_writer_status soj_format_writer_filesystem_add_file(struct so_format_writer * fw, const struct so_format_file * file) {
	struct soj_format_writer_filesystem_private * self = fw->data;

	char * path;
	asprintf(&path, "%s/%s", self->base_dir, file->filename);

	self->fd = open(path, O_WRONLY | O_CREAT, file->mode);
	if (self->fd < 0) {
		self->last_errno = errno;
		return so_format_writer_error;
	}

	if (file->position > 0 && lseek(self->fd, SEEK_SET, file->position) == (off_t) -1) {
		self->last_errno = errno;
		return so_format_writer_error;
	}

	if (fchown(self->fd, file->uid, file->gid) != 0)
		self->last_errno = errno;

	return so_format_writer_ok;
}

static enum so_format_writer_status soj_format_writer_filesystem_add_label(struct so_format_writer * fw __attribute__((unused)), const char * label __attribute__((unused))) {
	return so_format_writer_unsupported;
}

static int soj_format_writer_filesystem_close(struct so_format_writer * fw) {
	struct soj_format_writer_filesystem_private * self = fw->data;

	if (self->fd > -1 && close(self->fd) != 0)
		self->last_errno = errno;
	self->fd = -1;

	return 0;
}

static ssize_t soj_format_writer_filesystem_end_of_file(struct so_format_writer * fw) {
	struct soj_format_writer_filesystem_private * self = fw->data;

	if (self->fd > -1 && close(self->fd) != 0)
		self->last_errno = errno;
	self->fd = -1;

	return 0;
}

static int soj_format_writer_filesystem_file_position(struct so_format_writer * fw __attribute__((unused))) {
	return 0;
}

static void soj_format_writer_filesystem_free(struct so_format_writer * fw) {
	struct soj_format_writer_filesystem_private * self = fw->data;

	if (self->fd > -1)
		soj_format_writer_filesystem_close(fw);

	free(self->base_dir);
	free(self);
	free(fw);
}

static ssize_t soj_format_writer_filesystem_get_available_size(struct so_format_writer * fw) {
	struct soj_format_writer_filesystem_private * self = fw->data;

	struct statvfs st;
	int failed = statvfs(self->base_dir, &st);
	if (failed != 0) {
		self->last_errno = errno;
		return failed;
	}

	if (self->block_size < 1)
		self->block_size = st.f_bsize;

	// TODO: check this
	return st.f_bfree;
}

static ssize_t soj_format_writer_filesystem_get_block_size(struct so_format_writer * fw) {
	struct soj_format_writer_filesystem_private * self = fw->data;

	if (self->block_size > 0)
		return self->block_size;

	struct stat st;
	int failed = stat(self->base_dir, &st);
	if (failed != 0) {
		self->last_errno = errno;
		return failed;
	}

	return self->block_size = st.st_blksize;
}

static struct so_value * soj_format_writer_filesystem_get_digests(struct so_format_writer * fw __attribute__((unused))) {
	return so_value_new_linked_list();
}

static int soj_format_writer_filesystem_last_errno(struct so_format_writer * fw) {
	struct soj_format_writer_filesystem_private * self = fw->data;
	return self->last_errno;
}

static ssize_t soj_format_writer_filesystem_position(struct so_format_writer * fw __attribute__((unused))) {
	return -1;
}

static enum so_format_writer_status soj_format_writer_filesystem_restart_file(struct so_format_writer * fw, const struct so_format_file * file, ssize_t position) {
	struct so_format_file tmp_file = *file;
	tmp_file.position = position;

	return soj_format_writer_filesystem_add_file(fw, &tmp_file);
}

static ssize_t soj_format_writer_filesystem_write(struct so_format_writer * fw, const void * buffer, ssize_t length) {
	struct soj_format_writer_filesystem_private * self = fw->data;

	if (self->fd < 0)
		return -1;

	ssize_t nb_write = write(self->fd, buffer, length);
	if (nb_write < 0)
		self->last_errno = errno;

	return nb_write;
}

