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
*  Last modified: Tue, 02 Oct 2012 18:36:00 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// errno
#include <errno.h>
// pipe2
#include <fcntl.h>
// poll
#include <poll.h>
// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// fstat, pipe2
#include <sys/stat.h>
// fstat, pipe2
#include <sys/types.h>
// fstatfs
#include <sys/vfs.h>
// waitpid
#include <sys/wait.h>
// close, fstat, pipe2
#include <unistd.h>

#include "file_reader.h"


struct test_stoned_tar_file_private {
	int fd;
	ssize_t position;
	int last_errno;
};

static int test_stoned_tar_file_reader_close(struct st_stream_reader * io);
static int test_stoned_tar_file_reader_end_of_file(struct st_stream_reader * io);
static off_t test_stoned_tar_file_reader_forward(struct st_stream_reader * io, off_t offset);
static void test_stoned_tar_file_reader_free(struct st_stream_reader * io);
static ssize_t test_stoned_tar_file_reader_get_block_size(struct st_stream_reader * io);
static int test_stoned_tar_file_reader_last_errno(struct st_stream_reader * io);
static ssize_t test_stoned_tar_file_reader_position(struct st_stream_reader * io);
static ssize_t test_stoned_tar_file_reader_read(struct st_stream_reader * io, void * buffer, ssize_t length);
static off_t test_stoned_tar_file_reader_set_position(struct st_stream_reader * io, off_t position);

static struct st_stream_reader_ops test_stoned_tar_file_reader_ops = {
	.close          = test_stoned_tar_file_reader_close,
	.end_of_file    = test_stoned_tar_file_reader_end_of_file,
	.forward        = test_stoned_tar_file_reader_forward,
	.free           = test_stoned_tar_file_reader_free,
	.get_block_size = test_stoned_tar_file_reader_get_block_size,
	.last_errno     = test_stoned_tar_file_reader_last_errno,
	.position       = test_stoned_tar_file_reader_position,
	.read           = test_stoned_tar_file_reader_read,
	.set_position   = test_stoned_tar_file_reader_set_position,
};


int test_stoned_tar_file_reader_close(struct st_stream_reader * io) {
	struct test_stoned_tar_file_private * self = io->data;
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

int test_stoned_tar_file_reader_end_of_file(struct st_stream_reader * io) {
	struct test_stoned_tar_file_private * self = io->data;
	self->last_errno = 0;

	struct stat fs;
	int failed = fstat(self->fd, &fs);
	if (failed)
		return -1;

	return fs.st_size <= self->position;
}

off_t test_stoned_tar_file_reader_forward(struct st_stream_reader * io, off_t offset) {
	struct test_stoned_tar_file_private * self = io->data;
	self->last_errno = 0;

	struct stat fs;
	int failed = fstat(self->fd, &fs);
	if (failed)
		return -1;

	if (self->position + offset > fs.st_size)
		offset = fs.st_size - self->position;

	off_t position = lseek(self->fd, offset, SEEK_CUR);
	if (position != -1)
		self->position = position;

	return self->position;
}

void test_stoned_tar_file_reader_free(struct st_stream_reader * io) {
	struct test_stoned_tar_file_private * self = io->data;

	if (self->fd > -1)
		test_stoned_tar_file_reader_close(io);

	free(self);

	io->data = 0;
	io->ops = 0;
}

ssize_t test_stoned_tar_file_reader_get_block_size(struct st_stream_reader * io) {
	struct test_stoned_tar_file_private * self = io->data;
	self->last_errno = 0;

	struct stat st;
	if (fstat(self->fd, &st)) {
		self->last_errno = errno;
		return -1;
	}

	return st.st_blksize;
}

int test_stoned_tar_file_reader_last_errno(struct st_stream_reader * io) {
	struct test_stoned_tar_file_private * self = io->data;
	return self->last_errno;
}

ssize_t test_stoned_tar_file_reader_position(struct st_stream_reader * io) {
	struct test_stoned_tar_file_private * self = io->data;
	return self->position;
}

ssize_t test_stoned_tar_file_reader_read(struct st_stream_reader * io, void * buffer, ssize_t length) {
	struct test_stoned_tar_file_private * self = io->data;
	self->last_errno = 0;

	ssize_t nb_read = read(self->fd, buffer, length);
	if (nb_read > 0)
		self->position += nb_read;
	else if (nb_read < 0)
		self->last_errno = errno;

	return nb_read;
}

off_t test_stoned_tar_file_reader_set_position(struct st_stream_reader * io, off_t position) {
	struct test_stoned_tar_file_private * self = io->data;

	off_t pos = lseek(self->fd, position, SEEK_SET);
	if (pos != -1)
		self->position = pos;

	return self->position;
}


struct st_stream_reader * test_stoned_tar_get_file_reader(const char * filename) {
	if (filename == NULL)
		return NULL;

	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;

	struct test_stoned_tar_file_private * self = malloc(sizeof(struct test_stoned_tar_file_private));
	self->fd = fd;
	self->position = 0;
	self->last_errno = 0;

	struct st_stream_reader * r = malloc(sizeof(struct st_stream_reader));
	r->ops = &test_stoned_tar_file_reader_ops;
	r->data = self;

	return r;
}

