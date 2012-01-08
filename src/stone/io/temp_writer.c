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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sun, 08 Jan 2012 17:32:12 +0100                         *
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
// close, fstat
#include <unistd.h>


#include <stone/io.h>

struct st_io_temp_writer_private {
	int fd;
	char * filename;
	ssize_t position;
	int last_errno;
};

static int st_io_temp_writer_close(struct st_stream_writer * io);
static void st_io_temp_writer_free(struct st_stream_writer * io);
static ssize_t st_io_temp_writer_get_block_size(struct st_stream_writer * io);
static int st_io_temp_writer_last_errno(struct st_stream_writer * io);
static ssize_t st_io_temp_writer_position(struct st_stream_writer * io);
static ssize_t st_io_temp_writer_write(struct st_stream_writer * io, const void * buffer, ssize_t length);

static struct st_stream_writer_ops st_io_temp_writer_ops = {
	.close          = st_io_temp_writer_close,
	.free           = st_io_temp_writer_free,
	.get_block_size = st_io_temp_writer_get_block_size,
	.last_errno     = st_io_temp_writer_last_errno,
	.position       = st_io_temp_writer_position,
	.write          = st_io_temp_writer_write,
};


int st_io_temp_writer_close(struct st_stream_writer * io) {
	struct st_io_temp_writer_private * self = io->data;
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

void st_io_temp_writer_free(struct st_stream_writer * io) {
	struct st_io_temp_writer_private * self = io->data;

	if (self->fd > -1)
		st_io_temp_writer_close(io);

	unlink(self->filename);
	free(self->filename);
	free(self);

	io->data = 0;
	io->ops = 0;
}

ssize_t st_io_temp_writer_get_block_size(struct st_stream_writer * io) {
	struct st_io_temp_writer_private * self = io->data;
	self->last_errno = 0;

	struct stat st;
	if (fstat(self->fd, &st)) {
		self->last_errno = errno;
		return -1;
	}

	return st.st_blksize;
}

int st_io_temp_writer_last_errno(struct st_stream_writer * io) {
	struct st_io_temp_writer_private * self = io->data;
	return self->last_errno;
}

ssize_t st_io_temp_writer_position(struct st_stream_writer * io) {
	struct st_io_temp_writer_private * self = io->data;
	return self->position;
}

ssize_t st_io_temp_writer_write(struct st_stream_writer * io, const void * buffer, ssize_t length) {
	struct st_io_temp_writer_private * self = io->data;

	if (self->fd < 0)
		return -1;

	ssize_t nb_write = write(self->fd, buffer, length);

	if (nb_write > 0)
		self->position += nb_write;
	else if (nb_write < 0)
		self->last_errno = errno;

	return nb_write;
}


struct st_stream_writer * st_stream_get_tmp_writer() {
	char filename[] = "/tmp/STone_XXXXXX";

	int fd = mkstemp(filename);
	if (fd < 0)
		return 0;

	struct st_io_temp_writer_private * self = malloc(sizeof(struct st_io_temp_writer_private));
	self->fd = fd;
	self->filename = strdup(filename);
	self->position = 0;

	struct st_stream_writer * w = malloc(sizeof(struct st_stream_writer));
	w->ops = &st_io_temp_writer_ops;
	w->data = self;

	return w;
}

