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
*  Last modified: Thu, 04 Apr 2013 15:42:51 +0200                            *
\****************************************************************************/

// errno
#include <errno.h>
// open
#include <fcntl.h>
// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// fstat, open
#include <sys/stat.h>
// fstatvfs
#include <sys/statvfs.h>
// fstat, lseek, open
#include <sys/types.h>
// close, fstat, lseek, read, write
#include <unistd.h>


#include "../io.h"

struct st_io_file_private {
	int fd;
	bool end_of_file;
	ssize_t position;
	int last_errno;
};

static int st_io_file_close(struct st_io_file_private * self);
static ssize_t st_io_file_get_block_size(struct st_io_file_private * self);

static struct st_stream_reader * st_io_file_reader2(int fd);
static int st_io_file_reader_close(struct st_stream_reader * sfr);
static bool st_io_file_reader_end_of_file(struct st_stream_reader * sfr);
static off_t st_io_file_reader_forward(struct st_stream_reader * sfr, off_t offset);
static void st_io_file_reader_free(struct st_stream_reader * sfr);
static ssize_t st_io_file_reader_get_block_size(struct st_stream_reader * sfr);
static int st_io_file_reader_last_errno(struct st_stream_reader * sfr);
static ssize_t st_io_file_reader_position(struct st_stream_reader * sfr);
static ssize_t st_io_file_reader_read(struct st_stream_reader * sfr, void * buffer, ssize_t length);

static ssize_t st_io_file_writer_before_close(struct st_stream_writer * sfw, void * buffer, ssize_t length);
static int st_io_file_writer_close(struct st_stream_writer * sfw);
static void st_io_file_writer_free(struct st_stream_writer * sfw);
static ssize_t st_io_file_writer_get_available_size(struct st_stream_writer * sfw);
static ssize_t st_io_file_writer_get_block_size(struct st_stream_writer * sfw);
static int st_io_file_writer_last_errno(struct st_stream_writer * sfw);
static ssize_t st_io_file_writer_position(struct st_stream_writer * sfw);
static struct st_stream_reader * st_io_file_writer_reopen(struct st_stream_writer * sfw);
static ssize_t st_io_file_writer_write(struct st_stream_writer * sfw, const void * buffer, ssize_t length);

static struct st_stream_reader_ops st_io_file_reader_ops = {
	.close          = st_io_file_reader_close,
	.end_of_file    = st_io_file_reader_end_of_file,
	.forward        = st_io_file_reader_forward,
	.free           = st_io_file_reader_free,
	.get_block_size = st_io_file_reader_get_block_size,
	.last_errno     = st_io_file_reader_last_errno,
	.position       = st_io_file_reader_position,
	.read           = st_io_file_reader_read,
};

static struct st_stream_writer_ops st_io_file_writer_ops = {
	.before_close       = st_io_file_writer_before_close,
	.close              = st_io_file_writer_close,
	.free               = st_io_file_writer_free,
	.get_available_size = st_io_file_writer_get_available_size,
	.get_block_size     = st_io_file_writer_get_block_size,
	.last_errno         = st_io_file_writer_last_errno,
	.position           = st_io_file_writer_position,
	.reopen             = st_io_file_writer_reopen,
	.write              = st_io_file_writer_write,
};


static int st_io_file_close(struct st_io_file_private * self) {
	self->last_errno = 0;

	int failed = 0;
	if (self->fd > -1) {
		failed = close(self->fd);
		self->fd = -1;
		if (failed)
			self->last_errno = errno;
	}

	return failed;
}

static ssize_t st_io_file_get_block_size(struct st_io_file_private * self) {
	self->last_errno = 0;

	struct stat st;
	if (fstat(self->fd, &st)) {
		self->last_errno = errno;
		return -1;
	}

	return st.st_blksize;
}


struct st_stream_reader * st_io_file_reader(const char * filename) {
	if (filename == NULL)
		return NULL;

	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;

	return st_io_file_reader2(fd);
}

static struct st_stream_reader * st_io_file_reader2(int fd) {
	struct st_io_file_private * self = malloc(sizeof(struct st_io_file_private));
	self->fd = fd;
	self->end_of_file = false;
	self->position = 0;

	struct st_stream_reader * r = malloc(sizeof(struct st_stream_reader));
	r->ops = &st_io_file_reader_ops;
	r->data = self;

	return r;
}

static int st_io_file_reader_close(struct st_stream_reader * sfr) {
	return st_io_file_close(sfr->data);
}

static bool st_io_file_reader_end_of_file(struct st_stream_reader * sfr) {
	struct st_io_file_private * self = sfr->data;
	return self->end_of_file;
}

static off_t st_io_file_reader_forward(struct st_stream_reader * sfr, off_t offset) {
	struct st_io_file_private * self = sfr->data;
	self->last_errno = 0;

	struct stat st;
	if (fstat(self->fd, &st)) {
		self->last_errno = errno;
		return -1;
	}

	off_t position = lseek(self->fd, 0, SEEK_CUR);
	if (position == (off_t) -1) {
		self->last_errno = errno;
		return -1;
	}

	if (position + offset > st.st_size)
		offset = st.st_size - position;

	position = lseek(self->fd, offset, SEEK_CUR);
	if (position == (off_t) -1) {
		self->last_errno = errno;
		return -1;
	}

	self->position += offset;

	return position;
}

static void st_io_file_reader_free(struct st_stream_reader * sfr) {
	struct st_io_file_private * self = sfr->data;

	if (self->fd > -1)
		st_io_file_close(self);

	free(self);

	sfr->data = 0;
	sfr->ops = 0;
}

static ssize_t st_io_file_reader_get_block_size(struct st_stream_reader * sfr) {
	return st_io_file_get_block_size(sfr->data);
}

static int st_io_file_reader_last_errno(struct st_stream_reader * sfr) {
	struct st_io_file_private * self = sfr->data;
	return self->last_errno;
}

static ssize_t st_io_file_reader_position(struct st_stream_reader * sfr) {
	struct st_io_file_private * self = sfr->data;
	return self->position;
}

static ssize_t st_io_file_reader_read(struct st_stream_reader * sfr, void * buffer, ssize_t length) {
	struct st_io_file_private * self = sfr->data;

	self->last_errno = 0;

	if (self->fd < 0)
		return -1;

	ssize_t nb_read = read(self->fd, buffer, length);
	if (nb_read > 0)
		self->position += nb_read;
	else if (nb_read < 0)
		self->last_errno = errno;
	else
		self->end_of_file = true;

	return nb_read;
}


struct st_stream_writer * st_io_file_writer(const char * filename) {
	if (filename == NULL)
		return NULL;

	int fd = open(filename, O_RDWR | O_TRUNC | O_CREAT, 0644);
	if (fd < 0)
		return NULL;

	return st_io_file_writer2(fd);
}

struct st_stream_writer * st_io_file_writer2(int fd) {
	struct st_io_file_private * self = malloc(sizeof(struct st_io_file_private));
	self->fd = fd;
	self->end_of_file = false;
	self->position = 0;

	struct st_stream_writer * w = malloc(sizeof(struct st_stream_writer));
	w->ops = &st_io_file_writer_ops;
	w->data = self;

	return w;
}

static ssize_t st_io_file_writer_before_close(struct st_stream_writer * sfw, void * buffer, ssize_t length) {
	if (sfw == NULL || buffer == NULL || length < 0)
		return -1;

	return 0;
}

static int st_io_file_writer_close(struct st_stream_writer * sfw) {
	return st_io_file_close(sfw->data);
}

static void st_io_file_writer_free(struct st_stream_writer * sfw) {
	struct st_io_file_private * self = sfw->data;

	if (self->fd > -1)
		st_io_file_close(self);

	free(self);

	sfw->data = 0;
	sfw->ops = 0;
}

static ssize_t st_io_file_writer_get_available_size(struct st_stream_writer * sfw) {
	struct st_io_file_private * self = sfw->data;
	self->last_errno = 0;

	struct statvfs st;
	if (fstatvfs(self->fd, &st)) {
		self->last_errno = errno;
		return -1;
	}

	return st.f_bavail * st.f_bsize;
}

static ssize_t st_io_file_writer_get_block_size(struct st_stream_writer * sfw) {
	return st_io_file_get_block_size(sfw->data);
}

static int st_io_file_writer_last_errno(struct st_stream_writer * sfw) {
	struct st_io_file_private * self = sfw->data;
	return self->last_errno;
}

static ssize_t st_io_file_writer_position(struct st_stream_writer * sfw) {
	struct st_io_file_private * self = sfw->data;
	return self->position;
}

static struct st_stream_reader * st_io_file_writer_reopen(struct st_stream_writer * sfw) {
	struct st_io_file_private * self = sfw->data;

	if (self->fd < 0)
		return NULL;

	lseek(self->fd, 0, SEEK_SET);

	struct st_stream_reader * reader = st_io_file_reader2(self->fd);
	self->fd = -1;

	return reader;
}

static ssize_t st_io_file_writer_write(struct st_stream_writer * sfw, const void * buffer, ssize_t length) {
	struct st_io_file_private * self = sfw->data;

	if (self->fd < 0)
		return -1;

	ssize_t nb_write = write(self->fd, buffer, length);

	if (nb_write > 0)
		self->position += nb_write;
	else if (nb_write < 0)
		self->last_errno = errno;

	return nb_write;
}

