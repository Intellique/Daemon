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
*  Last modified: Mon, 01 Oct 2012 13:03:41 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// errno
#include <errno.h>
// pipe2
#include <fcntl.h>
// mkstemp
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

#include "pipe_io.h"

struct test_stoned_tar_pipeio_writer_private {
	int fd;
	int pid;
	ssize_t position;
	int last_errno;
};

static int test_stoned_tar_pipeio_writer_close(struct st_stream_writer * io);
static void test_stoned_tar_pipeio_writer_free(struct st_stream_writer * io);
//static ssize_t test_stoned_tar_pipeio_writer_get_available_size(struct st_stream_writer * io);
static ssize_t test_stoned_tar_pipeio_writer_get_block_size(struct st_stream_writer * io);
static int test_stoned_tar_pipeio_writer_last_errno(struct st_stream_writer * io);
static ssize_t test_stoned_tar_pipeio_writer_position(struct st_stream_writer * io);
static ssize_t test_stoned_tar_pipeio_writer_write(struct st_stream_writer * io, const void * buffer, ssize_t length);

static struct st_stream_writer_ops test_stoned_tar_pipeio_writer_ops = {
	.close              = test_stoned_tar_pipeio_writer_close,
	.free               = test_stoned_tar_pipeio_writer_free,
	.get_available_size = test_stoned_tar_pipeio_writer_get_block_size,
	.get_block_size     = test_stoned_tar_pipeio_writer_get_block_size,
	.last_errno         = test_stoned_tar_pipeio_writer_last_errno,
	.position           = test_stoned_tar_pipeio_writer_position,
	.write              = test_stoned_tar_pipeio_writer_write,
};


int test_stoned_tar_pipeio_writer_close(struct st_stream_writer * io) {
	struct test_stoned_tar_pipeio_writer_private * self = io->data;
	self->last_errno = 0;

	int failed = 0;
	if (self->fd > -1) {
		failed = close(self->fd);
		self->fd = -1;
		if (failed)
			self->last_errno = errno;
	}

	int status = 0;
	waitpid(self->pid, &status, 0);

	if (WIFEXITED(status))
		failed = WEXITSTATUS(status);
	else
		failed = -1;

	return failed;
}

void test_stoned_tar_pipeio_writer_free(struct st_stream_writer * io) {
	struct test_stoned_tar_pipeio_writer_private * self = io->data;

	if (self->fd > -1)
		test_stoned_tar_pipeio_writer_close(io);

	free(self);

	io->data = 0;
	io->ops = 0;
}

ssize_t test_stoned_tar_pipeio_writer_get_block_size(struct st_stream_writer * io) {
	struct test_stoned_tar_pipeio_writer_private * self = io->data;
	self->last_errno = 0;

	struct stat st;
	if (fstat(self->fd, &st)) {
		self->last_errno = errno;
		return -1;
	}

	return st.st_blksize;
}

int test_stoned_tar_pipeio_writer_last_errno(struct st_stream_writer * io) {
	struct test_stoned_tar_pipeio_writer_private * self = io->data;
	return self->last_errno;
}

ssize_t test_stoned_tar_pipeio_writer_position(struct st_stream_writer * io) {
	struct test_stoned_tar_pipeio_writer_private * self = io->data;
	return self->position;
}

ssize_t test_stoned_tar_pipeio_writer_write(struct st_stream_writer * io, const void * buffer, ssize_t length) {
	struct test_stoned_tar_pipeio_writer_private * self = io->data;

	if (self->fd < 0)
		return -1;

	ssize_t nb_write = write(self->fd, buffer, length);

	if (nb_write > 0)
		self->position += nb_write;
	else if (nb_write < 0)
		self->last_errno = errno;

	return nb_write;
}


struct st_stream_writer * test_stoned_tar_get_pipe(char * const params[], const char * directory) {
	int fds[2];
	int failed = pipe2(fds, O_CLOEXEC);
	if (failed)
		return NULL;

	int pid = fork();
	if (pid < 0)
		return NULL;

	if (pid == 0) {
		if (directory)
			chdir(directory);

		dup2(fds[0], 0);

		close(fds[0]);
		close(fds[1]);
		//close(2);

		execvp(params[0], params);

		_exit(1);
	}

	close(fds[0]);

	struct test_stoned_tar_pipeio_writer_private * self = malloc(sizeof(struct test_stoned_tar_pipeio_writer_private));
	self->fd = fds[1];
	self->pid = pid;
	self->position = 0;
	self->last_errno = 0;

	struct st_stream_writer * w = malloc(sizeof(struct st_stream_writer));
	w->ops = &test_stoned_tar_pipeio_writer_ops;
	w->data = self;

	return w;
}

