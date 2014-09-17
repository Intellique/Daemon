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

// errno
#include <errno.h>
// mkstemp
#include <stdlib.h>
// fstat
#include <sys/stat.h>
// fstatvfs
#include <sys/statvfs.h>
// fstat, lseek
#include <sys/types.h>
// close, fstat, lseek, unlink
#include <unistd.h>

#include "../io.h"

#include "config.h"

struct st_io_tmp_private {
	int fd;
	int last_errno;
};

static int st_io_tmp_close(struct st_io_tmp_private * self);
static ssize_t st_io_tmp_get_block_size(struct st_io_tmp_private * self);
static ssize_t st_io_tmp_position(struct st_io_tmp_private * self);

static int st_io_tmp_reader_close(struct st_stream_reader * sfr);
static bool st_io_tmp_reader_end_of_file(struct st_stream_reader * sfr);
static off_t st_io_tmp_reader_forward(struct st_stream_reader * sfr, off_t offset);
static void st_io_tmp_reader_free(struct st_stream_reader * sfr);
static ssize_t st_io_tmp_reader_get_block_size(struct st_stream_reader * sfr);
static int st_io_tmp_reader_last_errno(struct st_stream_reader * sfr);
static ssize_t st_io_tmp_reader_position(struct st_stream_reader * sfr);
static ssize_t st_io_tmp_reader_read(struct st_stream_reader * sfr, void * buffer, ssize_t length);

static ssize_t st_io_tmp_writer_before_close(struct st_stream_writer * sfw, void * buffer, ssize_t length);
static int st_io_tmp_writer_close(struct st_stream_writer * sfw);
static void st_io_tmp_writer_free(struct st_stream_writer * sfw);
static ssize_t st_io_tmp_writer_get_available_size(struct st_stream_writer * sfw);
static ssize_t st_io_tmp_writer_get_block_size(struct st_stream_writer * sfw);
static int st_io_tmp_writer_last_errno(struct st_stream_writer * sfw);
static ssize_t st_io_tmp_writer_position(struct st_stream_writer * sfw);
static struct st_stream_reader * st_io_tmp_writer_reopen(struct st_stream_writer * sfw);
static ssize_t st_io_tmp_writer_write(struct st_stream_writer * sfw, const void * buffer, ssize_t length);

static struct st_stream_reader_ops st_io_tmp_reader_ops = {
	.close          = st_io_tmp_reader_close,
	.end_of_file    = st_io_tmp_reader_end_of_file,
	.forward        = st_io_tmp_reader_forward,
	.free           = st_io_tmp_reader_free,
	.get_block_size = st_io_tmp_reader_get_block_size,
	.last_errno     = st_io_tmp_reader_last_errno,
	.position       = st_io_tmp_reader_position,
	.read           = st_io_tmp_reader_read,
};

static struct st_stream_writer_ops st_io_tmp_writer_ops = {
	.before_close       = st_io_tmp_writer_before_close,
	.close              = st_io_tmp_writer_close,
	.free               = st_io_tmp_writer_free,
	.get_available_size = st_io_tmp_writer_get_available_size,
	.get_block_size     = st_io_tmp_writer_get_block_size,
	.last_errno         = st_io_tmp_writer_last_errno,
	.position           = st_io_tmp_writer_position,
	.reopen             = st_io_tmp_writer_reopen,
	.write              = st_io_tmp_writer_write,
};


static int st_io_tmp_close(struct st_io_tmp_private * self) {
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

static ssize_t st_io_tmp_get_block_size(struct st_io_tmp_private * self) {
	self->last_errno = 0;

	struct stat st;
	if (fstat(self->fd, &st)) {
		self->last_errno = errno;
		return -1;
	}

	return st.st_blksize;
}

static ssize_t st_io_tmp_position(struct st_io_tmp_private * self) {
	ssize_t new_pos = lseek(self->fd, 0, SEEK_CUR);

	if (new_pos < 0)
		self->last_errno = errno;

	return new_pos;
}


static int st_io_tmp_reader_close(struct st_stream_reader * sfr) {
	return st_io_tmp_close(sfr->data);
}

static bool st_io_tmp_reader_end_of_file(struct st_stream_reader * sfr) {
	ssize_t cur_pos = st_io_tmp_position(sfr->data);
	if (cur_pos < 0)
		return true;

	struct st_io_tmp_private * self = sfr->data;
	struct stat st;
	if (fstat(self->fd, &st)) {
		self->last_errno = errno;
		return true;
	}

	return cur_pos == st.st_size;
}

static off_t st_io_tmp_reader_forward(struct st_stream_reader * sfr, off_t offset) {
	struct st_io_tmp_private * self = sfr->data;
	self->last_errno = 0;
	ssize_t new_pos = lseek(self->fd, offset, SEEK_CUR);
	if (new_pos < 0)
		self->last_errno = errno;
	return new_pos;
}

static void st_io_tmp_reader_free(struct st_stream_reader * sfr) {
	struct st_io_tmp_private * self = sfr->data;

	if (self->fd >= 0)
		st_io_tmp_reader_close(sfr);

	free(self);
	free(sfr);
}

static ssize_t st_io_tmp_reader_get_block_size(struct st_stream_reader * sfr) {
	return st_io_tmp_get_block_size(sfr->data);
}

static int st_io_tmp_reader_last_errno(struct st_stream_reader * sfr) {
	struct st_io_tmp_private * self = sfr->data;
	return self->last_errno;
}

static ssize_t st_io_tmp_reader_position(struct st_stream_reader * sfr) {
	return st_io_tmp_position(sfr->data);
}

static ssize_t st_io_tmp_reader_read(struct st_stream_reader * sfr, void * buffer, ssize_t length) {
	if (sfr == NULL || buffer == NULL || length < 0)
		return -1;

	if (length == 0)
		return 0;

	struct st_io_tmp_private * self = sfr->data;
	self->last_errno = 0;

	ssize_t nb_read = read(self->fd, buffer, length);
	if (nb_read < 0)
		self->last_errno = errno;

	return nb_read;
}


__asm__(".symver st_io_tmp_writer_v1, st_io_tmp_writer@@LIBSTONE_1.2");
struct st_stream_writer * st_io_tmp_writer_v1() {
	char filename[64] = TMP_DIR "/stoned_XXXXXX";

	int fd = mkstemp(filename);
	if (fd < 0)
		return NULL;

	unlink(filename);

	struct st_io_tmp_private * self = malloc(sizeof(struct st_io_tmp_private));
	self->fd = fd;
	self->last_errno = 0;

	struct st_stream_writer * writer = malloc(sizeof(struct st_stream_writer));
	writer->ops = &st_io_tmp_writer_ops;
	writer->data = self;

	return writer;
}


static ssize_t st_io_tmp_writer_before_close(struct st_stream_writer * sfw, void * buffer __attribute__((unused)), ssize_t length __attribute__((unused))) {
	struct st_io_tmp_private * self = sfw->data;

	if (self->fd < 0) {
		self->last_errno = EBADFD;
		return -1;
	}

	return 0;
}

static int st_io_tmp_writer_close(struct st_stream_writer * sfw) {
	return st_io_tmp_close(sfw->data);
}

static void st_io_tmp_writer_free(struct st_stream_writer * sfw) {
	struct st_io_tmp_private * self = sfw->data;

	if (self->fd >= 0)
		st_io_tmp_writer_close(sfw);

	free(self);
	free(sfw);
}

static ssize_t st_io_tmp_writer_get_available_size(struct st_stream_writer * sfw) {
	struct st_io_tmp_private * self = sfw->data;
	self->last_errno = 0;

	struct statvfs st;
	if (fstatvfs(self->fd, &st)) {
		self->last_errno = errno;
		return -1;
	}

	return st.f_bavail * st.f_bsize;
}

static ssize_t st_io_tmp_writer_get_block_size(struct st_stream_writer * sfw) {
	return st_io_tmp_get_block_size(sfw->data);
}

static int st_io_tmp_writer_last_errno(struct st_stream_writer * sfw) {
	struct st_io_tmp_private * self = sfw->data;
	return self->last_errno;
}

static ssize_t st_io_tmp_writer_position(struct st_stream_writer * sfw) {
	return st_io_tmp_position(sfw->data);
}

static struct st_stream_reader * st_io_tmp_writer_reopen(struct st_stream_writer * sfw) {
	struct st_io_tmp_private * self = sfw->data;
	struct st_io_tmp_private * new_self = malloc(sizeof(struct st_io_tmp_private));
	new_self->fd = self->fd;
	new_self->last_errno = 0;

	self->fd = -1;

	struct st_stream_reader * reader = malloc(sizeof(struct st_stream_reader));
	reader->ops = &st_io_tmp_reader_ops;
	reader->data = new_self;

	return reader;
}

static ssize_t st_io_tmp_writer_write(struct st_stream_writer * sfw, const void * buffer, ssize_t length) {
	if (sfw == NULL || buffer == NULL || length < 0)
		return -1;

	if (length == 0)
		return 0;

	struct st_io_tmp_private * self = sfw->data;
	ssize_t nb_write = write(self->fd, buffer, length);

	if (nb_write < 0)
		self->last_errno = errno;

	return nb_write;
}

