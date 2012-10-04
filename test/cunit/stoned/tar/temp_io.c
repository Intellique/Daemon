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
*  Last modified: Tue, 02 Oct 2012 12:57:27 +0200                         *
\*************************************************************************/

// errno
#include <errno.h>
// mkstemp
#include <stdlib.h>
// strdup
#include <string.h>
// fstat
#include <sys/stat.h>
// fstat
#include <sys/types.h>
// fstatfs
#include <sys/vfs.h>
// close, fstat
#include <unistd.h>

#include "temp_io.h"

struct test_stoned_tar_tempio_writer_private {
	int fd;
	char * filename;
	ssize_t position;
	ssize_t size;
	int last_errno;
};

static int test_stoned_tar_tempio_writer_close(struct st_stream_writer * io);
static void test_stoned_tar_tempio_writer_free(struct st_stream_writer * io);
static ssize_t test_stoned_tar_tempio_writer_get_available_size(struct st_stream_writer * io);
static ssize_t test_stoned_tar_tempio_writer_get_block_size(struct st_stream_writer * io);
static int test_stoned_tar_tempio_writer_last_errno(struct st_stream_writer * io);
static ssize_t test_stoned_tar_tempio_writer_position(struct st_stream_writer * io);
static ssize_t test_stoned_tar_tempio_writer_write(struct st_stream_writer * io, const void * buffer, ssize_t length);

static struct st_stream_writer_ops test_stoned_tar_tempio_writer_ops = {
	.close              = test_stoned_tar_tempio_writer_close,
	.free               = test_stoned_tar_tempio_writer_free,
	.get_available_size = test_stoned_tar_tempio_writer_get_available_size,
	.get_block_size     = test_stoned_tar_tempio_writer_get_block_size,
	.last_errno         = test_stoned_tar_tempio_writer_last_errno,
	.position           = test_stoned_tar_tempio_writer_position,
	.write              = test_stoned_tar_tempio_writer_write,
};


int test_stoned_tar_tempio_writer_close(struct st_stream_writer * io) {
	struct test_stoned_tar_tempio_writer_private * self = io->data;
	self->last_errno = 0;

	if (self->size > 0) {
		ssize_t block_size = test_stoned_tar_tempio_writer_get_block_size(io);
		if (self->position % block_size) {
			char buffer[block_size];

			ssize_t will_write = block_size - self->position % block_size;
			bzero(buffer, will_write);
			write(self->fd, buffer, will_write);
		}
	}

	int failed = 0;
	if (self->fd > -1) {
		failed = close(self->fd);
		self->fd = -1;
		if (failed)
			self->last_errno = errno;
	}

	return failed;
}

void test_stoned_tar_tempio_writer_free(struct st_stream_writer * io) {
	struct test_stoned_tar_tempio_writer_private * self = io->data;

	if (self->fd > -1)
		test_stoned_tar_tempio_writer_close(io);

	free(self->filename);
	free(self);

	io->data = 0;
	io->ops = 0;
}

ssize_t test_stoned_tar_tempio_writer_get_available_size(struct st_stream_writer * io) {
	struct test_stoned_tar_tempio_writer_private * self = io->data;
	self->last_errno = 0;

	if (self->size > 0)
		return self->size - self->position;

	struct statfs st;
	if (fstatfs(self->fd, &st)) {
		self->last_errno = errno;
		return -1;
	}

	return st.f_bavail * st.f_bsize;
}

ssize_t test_stoned_tar_tempio_writer_get_block_size(struct st_stream_writer * io) {
	struct test_stoned_tar_tempio_writer_private * self = io->data;
	self->last_errno = 0;

	struct stat st;
	if (fstat(self->fd, &st)) {
		self->last_errno = errno;
		return -1;
	}

	return st.st_blksize;
}

int test_stoned_tar_tempio_writer_last_errno(struct st_stream_writer * io) {
	struct test_stoned_tar_tempio_writer_private * self = io->data;
	return self->last_errno;
}

ssize_t test_stoned_tar_tempio_writer_position(struct st_stream_writer * io) {
	struct test_stoned_tar_tempio_writer_private * self = io->data;
	return self->position;
}

ssize_t test_stoned_tar_tempio_writer_write(struct st_stream_writer * io, const void * buffer, ssize_t length) {
	struct test_stoned_tar_tempio_writer_private * self = io->data;

	if (self->fd < 0)
		return -1;

	if (self->size > 0 && self->size - self->position < length)
		length = self->size - self->position;

	ssize_t nb_write = write(self->fd, buffer, length);

	if (nb_write > 0)
		self->position += nb_write;
	else if (nb_write < 0)
		self->last_errno = errno;

	return nb_write;
}


char * test_stoned_tar_get_filename(struct st_stream_writer * file) {
	struct test_stoned_tar_tempio_writer_private * self = file->data;
    return strdup(self->filename);
}

struct st_stream_writer * test_stoned_tar_get_temp_file(ssize_t limit_size) {
	char filename[] = "/tmp/STone_XXXXXX";

	int fd = mkstemp(filename);
	if (fd < 0)
		return 0;

	struct test_stoned_tar_tempio_writer_private * self = malloc(sizeof(struct test_stoned_tar_tempio_writer_private));
	self->fd = fd;
	self->filename = strdup(filename);
	self->position = 0;
	self->size = limit_size;

	struct st_stream_writer * w = malloc(sizeof(struct st_stream_writer));
	w->ops = &test_stoned_tar_tempio_writer_ops;
	w->data = self;

	return w;
}

