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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// errno
#include <errno.h>
// dgettext
#include <libintl.h>
// snprintf
#include <stdio.h>
// mkstemp
#include <stdlib.h>
// fstat
#include <sys/stat.h>
// fstatvfs
#include <sys/statvfs.h>
// fstat
#include <sys/types.h>
// close, fstat, pread, read, unlink, write
#include <unistd.h>

#include <libstoriqone/config.h>
#include <libstoriqone/io.h>
#include <libstoriqone/log.h>
#include <libstoriqone/value.h>

#include "config.h"

struct so_io_tmp_private {
	int fd;
	int last_errno;

	ssize_t file_position;
	ssize_t file_size;
};

static int so_io_tmp_close(struct so_io_tmp_private * self);
static ssize_t so_io_tmp_get_block_size(struct so_io_tmp_private * self);

static int so_io_tmp_reader_close(struct so_stream_reader * sr);
static bool so_io_tmp_reader_end_of_file(struct so_stream_reader * sr);
static off_t so_io_tmp_reader_forward(struct so_stream_reader * sr, off_t offset);
static void so_io_tmp_reader_free(struct so_stream_reader * sr);
static ssize_t so_io_tmp_reader_get_available_size(struct so_stream_reader * sr);
static ssize_t so_io_tmp_reader_get_block_size(struct so_stream_reader * sr);
static int so_io_tmp_reader_last_errno(struct so_stream_reader * sr);
static ssize_t so_io_tmp_reader_peek(struct so_stream_reader * sr, void * buffer, ssize_t length);
static ssize_t so_io_tmp_reader_position(struct so_stream_reader * sr);
static ssize_t so_io_tmp_reader_read(struct so_stream_reader * sr, void * buffer, ssize_t length);
static int so_io_tmp_reader_rewind(struct so_stream_reader * sr);

static ssize_t so_io_tmp_writer_before_close(struct so_stream_writer * sw, void * buffer, ssize_t length);
static int so_io_tmp_writer_close(struct so_stream_writer * sw);
static int so_io_tmp_writer_create_new_file(struct so_stream_writer * sw);
static int so_io_tmp_writer_file_position(struct so_stream_writer * sw);
static void so_io_tmp_writer_free(struct so_stream_writer * sw);
static ssize_t so_io_tmp_writer_get_available_size(struct so_stream_writer * sw);
static ssize_t so_io_tmp_writer_get_block_size(struct so_stream_writer * sw);
static int so_io_tmp_writer_last_errno(struct so_stream_writer * sw);
static ssize_t so_io_tmp_writer_position(struct so_stream_writer * sw);
static struct so_stream_reader * so_io_tmp_writer_reopen(struct so_stream_writer * sw);
static ssize_t so_io_tmp_writer_write(struct so_stream_writer * sw, const void * buffer, ssize_t length);

static struct so_stream_reader_ops so_io_tmp_reader_ops = {
	.close              = so_io_tmp_reader_close,
	.end_of_file        = so_io_tmp_reader_end_of_file,
	.forward            = so_io_tmp_reader_forward,
	.free               = so_io_tmp_reader_free,
	.get_available_size = so_io_tmp_reader_get_available_size,
	.get_block_size     = so_io_tmp_reader_get_block_size,
	.last_errno         = so_io_tmp_reader_last_errno,
	.peek               = so_io_tmp_reader_peek,
	.position           = so_io_tmp_reader_position,
	.read               = so_io_tmp_reader_read,
	.rewind             = so_io_tmp_reader_rewind,
};

static struct so_stream_writer_ops so_io_tmp_writer_ops = {
	.before_close       = so_io_tmp_writer_before_close,
	.close              = so_io_tmp_writer_close,
	.create_new_file    = so_io_tmp_writer_create_new_file,
	.file_position      = so_io_tmp_writer_file_position,
	.free               = so_io_tmp_writer_free,
	.get_available_size = so_io_tmp_writer_get_available_size,
	.get_block_size     = so_io_tmp_writer_get_block_size,
	.last_errno         = so_io_tmp_writer_last_errno,
	.position           = so_io_tmp_writer_position,
	.reopen             = so_io_tmp_writer_reopen,
	.write              = so_io_tmp_writer_write,
};


struct so_stream_writer * so_io_tmp_writer() {
	const char * directory = TMP_DIR;

	struct so_value * config = so_config_get();
	so_value_unpack(config, "{s{s{sS}}}", "default values", "paths", "temporary", &directory);

	char filename[128];
	snprintf(filename, 128, "%s/StoriqOne_XXXXXX", directory);

	int fd = mkstemp(filename);
	if (fd < 0) {
		so_log_write(so_log_level_error, dgettext("libstoriqone", "so_io_tmp_writer: Can't create temporary file into directory '%s' because %m"), directory);
		return NULL;
	}

	unlink(filename);

	struct so_io_tmp_private * self = malloc(sizeof(struct so_io_tmp_private));
	self->fd = fd;
	self->last_errno = 0;

	self->file_position = 0;
	self->file_size = 0;

	struct so_stream_writer * writer = malloc(sizeof(struct so_stream_writer));
	writer->ops = &so_io_tmp_writer_ops;
	writer->data = self;

	return writer;
}


static int so_io_tmp_close(struct so_io_tmp_private * self) {
	if (self->fd < 0) {
		self->last_errno = EBADFD;
		return -1;
	}

	int failed = close(self->fd);

	if (failed == 0) {
		self->fd = -1;
		self->last_errno = 0;
	} else
		self->last_errno = errno;

	return failed;
}

static ssize_t so_io_tmp_get_block_size(struct so_io_tmp_private * self) {
	self->last_errno = 0;

	struct stat st;
	if (fstat(self->fd, &st)) {
		self->last_errno = errno;
		return -1;
	}

	return st.st_blksize;
}


static int so_io_tmp_reader_close(struct so_stream_reader * sr) {
	return so_io_tmp_close(sr->data);
}

static bool so_io_tmp_reader_end_of_file(struct so_stream_reader * sr) {
	struct so_io_tmp_private * self = sr->data;

	ssize_t cur_pos = self->file_position;
	if (cur_pos < 0)
		return true;

	struct stat st;
	if (fstat(self->fd, &st)) {
		self->last_errno = errno;
		return true;
	}

	return cur_pos == st.st_size;
}

static off_t so_io_tmp_reader_forward(struct so_stream_reader * sr, off_t offset) {
	struct so_io_tmp_private * self = sr->data;
	self->last_errno = 0;
	ssize_t new_pos = lseek(self->fd, offset, SEEK_CUR);
	if (new_pos < 0)
		self->last_errno = errno;
	return new_pos;
}

static void so_io_tmp_reader_free(struct so_stream_reader * sr) {
	struct so_io_tmp_private * self = sr->data;

	if (self->fd >= 0)
		so_io_tmp_reader_close(sr);

	free(self);
	free(sr);
}

static ssize_t so_io_tmp_reader_get_available_size(struct so_stream_reader * sr) {
	struct so_io_tmp_private * self = sr->data;
	return self->file_size - self->file_position;
}

static ssize_t so_io_tmp_reader_get_block_size(struct so_stream_reader * sr) {
	return so_io_tmp_get_block_size(sr->data);
}

static int so_io_tmp_reader_last_errno(struct so_stream_reader * sr) {
	struct so_io_tmp_private * self = sr->data;
	return self->last_errno;
}

static ssize_t so_io_tmp_reader_peek(struct so_stream_reader * sr, void * buffer, ssize_t length) {
	if (sr == NULL || buffer == NULL || length < 0)
		return -1;

	if (length == 0)
		return 0;

	struct so_io_tmp_private * self = sr->data;
	self->last_errno = 0;

	ssize_t nb_read = pread(self->fd, buffer, length, self->file_position);
	if (nb_read < 0)
		self->last_errno = errno;

	return nb_read;
}

static ssize_t so_io_tmp_reader_position(struct so_stream_reader * sr) {
	struct so_io_tmp_private * self = sr->data;
	return self->file_position;
}

static ssize_t so_io_tmp_reader_read(struct so_stream_reader * sr, void * buffer, ssize_t length) {
	if (sr == NULL || buffer == NULL || length < 0)
		return -1;

	if (length == 0)
		return 0;

	struct so_io_tmp_private * self = sr->data;
	self->last_errno = 0;

	ssize_t nb_read = read(self->fd, buffer, length);
	if (nb_read < 0)
		self->last_errno = errno;
	else if (nb_read > 0)
		self->file_position += nb_read;

	return nb_read;
}

static int so_io_tmp_reader_rewind(struct so_stream_reader * sr) {
	struct so_io_tmp_private * self = sr->data;

	off_t new_pos = lseek(self->fd, 0, SEEK_SET);

	return new_pos == (off_t) -1;
}


static ssize_t so_io_tmp_writer_before_close(struct so_stream_writer * sw, void * buffer __attribute__((unused)), ssize_t length __attribute__((unused))) {
	struct so_io_tmp_private * self = sw->data;

	if (self->fd < 0) {
		self->last_errno = EBADFD;
		return -1;
	}

	return 0;
}

static int so_io_tmp_writer_close(struct so_stream_writer * sw) {
	return so_io_tmp_close(sw->data);
}

static int so_io_tmp_writer_create_new_file(struct so_stream_writer * sw __attribute__((unused))) {
	return -1;
}

static int so_io_tmp_writer_file_position(struct so_stream_writer * sw __attribute__((unused))) {
	return 0;
}

static void so_io_tmp_writer_free(struct so_stream_writer * sw) {
	struct so_io_tmp_private * self = sw->data;

	if (self->fd >= 0)
		so_io_tmp_writer_close(sw);

	free(self);
	free(sw);
}

static ssize_t so_io_tmp_writer_get_available_size(struct so_stream_writer * sw) {
	struct so_io_tmp_private * self = sw->data;
	self->last_errno = 0;

	struct statvfs st;
	if (fstatvfs(self->fd, &st)) {
		self->last_errno = errno;
		return -1;
	}

	return st.f_bavail * st.f_bsize;
}

static ssize_t so_io_tmp_writer_get_block_size(struct so_stream_writer * sw) {
	return so_io_tmp_get_block_size(sw->data);
}

static int so_io_tmp_writer_last_errno(struct so_stream_writer * sw) {
	struct so_io_tmp_private * self = sw->data;
	return self->last_errno;
}

static ssize_t so_io_tmp_writer_position(struct so_stream_writer * sw) {
	struct so_io_tmp_private * self = sw->data;
	return self->file_size;
}

static struct so_stream_reader * so_io_tmp_writer_reopen(struct so_stream_writer * sw) {
	struct so_io_tmp_private * self = sw->data;
	struct so_io_tmp_private * new_self = malloc(sizeof(struct so_io_tmp_private));
	new_self->fd = self->fd;
	new_self->last_errno = 0;

	new_self->file_position = 0;
	new_self->file_size = self->file_size;

	self->fd = -1;

	struct so_stream_reader * reader = malloc(sizeof(struct so_stream_reader));
	reader->ops = &so_io_tmp_reader_ops;
	reader->data = new_self;

	lseek(new_self->fd, 0, SEEK_SET);

	return reader;
}

static ssize_t so_io_tmp_writer_write(struct so_stream_writer * sw, const void * buffer, ssize_t length) {
	if (sw == NULL || buffer == NULL || length < 0)
		return -1;

	if (length == 0)
		return 0;

	struct so_io_tmp_private * self = sw->data;
	ssize_t nb_write = write(self->fd, buffer, length);

	if (nb_write < 0)
		self->last_errno = errno;
	else if (nb_write > 0)
		self->file_size += nb_write;

	return nb_write;
}

