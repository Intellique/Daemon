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
*  Last modified: Mon, 23 Jan 2012 22:38:07 +0100                         *
\*************************************************************************/

// malloc
#include <stdlib.h>

#include <stone/io/socket.h>

struct st_socket_generic_private {
	int fd;
	short connected;
};

static int st_socket_generic_connect(struct st_io_socket * socket, const char * hostname, unsigned short port);
static void st_socket_generic_free(struct st_io_socket * socket);
static struct st_stream_writer * st_socket_generic_get_input_stream(struct st_io_socket * socket);
static struct st_stream_reader * st_socket_generic_get_output_stream(struct st_io_socket * socket);
static int st_socket_generic_is_connected(struct st_io_socket * socket);

static int st_socket_generic_reader_close(struct st_stream_reader * io);
static int st_socket_generic_reader_end_of_file(struct st_stream_reader * io);
static off_t st_socket_generic_reader_forward(struct st_stream_reader * io, off_t offset);
static void st_socket_generic_reader_free(struct st_stream_reader * io);
static ssize_t st_socket_generic_reader_get_block_size(struct st_stream_reader * io);
static int st_socket_generic_reader_last_errno(struct st_stream_reader * io);
static ssize_t st_socket_generic_reader_position(struct st_stream_reader * io);
static ssize_t st_socket_generic_reader_read(struct st_stream_reader * io, void * buffer, ssize_t length);

static int st_socket_generic_writer_close(struct st_stream_writer * io);
static void st_socket_generic_writer_free(struct st_stream_writer * io);
static ssize_t st_socket_generic_writer_get_available_size(struct st_stream_writer * io);
static ssize_t st_socket_generic_writer_get_block_size(struct st_stream_writer * io);
static int st_socket_generic_writer_last_errno(struct st_stream_writer * io);
static ssize_t st_socket_generic_writer_position(struct st_stream_writer * io);
static ssize_t st_socket_generic_writer_write(struct st_stream_writer * io, const char * buffer, ssize_t length);

static struct st_io_socket_ops st_socket_generic_ops = {
	.connect           = st_socket_generic_connect,
	.free              = st_socket_generic_free,
	.get_input_stream  = st_socket_generic_get_input_stream,
	.get_output_stream = st_socket_generic_get_output_stream,
	.is_connected      = st_socket_generic_is_connected,
};

static struct st_stream_reader_ops st_socket_generic_reader_ops = {
	.close          = st_socket_generic_reader_close,
	.end_of_file    = st_socket_generic_reader_end_of_file,
	.forward        = st_socket_generic_reader_forward,
	.free           = st_socket_generic_reader_free,
	.get_block_size = st_socket_generic_reader_get_block_size,
	.last_errno     = st_socket_generic_reader_last_errno,
	.position       = st_socket_generic_reader_position,
	.read           = st_socket_generic_reader_read,
};

static struct st_stream_writer_ops st_socket_generic_writer_ops = {
	.close              = st_socket_generic_writer_close,
	.free               = st_socket_generic_writer_free,
	.get_available_size = st_socket_generic_writer_get_available_size,
	.get_block_size     = st_socket_generic_writer_get_block_size,
	.last_errno         = st_socket_generic_writer_last_errno,
	.position           = st_socket_generic_writer_position,
	.write              = st_socket_generic_writer_write,
};


int st_socket_generic_connect(struct st_io_socket * socket, const char * hostname, unsigned short port) {
	return 0;
}

void st_socket_generic_free(struct st_io_socket * socket) {
}

struct st_stream_writer * st_socket_generic_get_input_stream(struct st_io_socket * socket) {
	return 0;
}

struct st_stream_reader * st_socket_generic_get_output_stream(struct st_io_socket * socket) {
	return 0;
}

int st_socket_generic_is_connected(struct st_io_socket * socket) {
	return 0;
}


int st_socket_generic_reader_close(struct st_stream_reader * io) {
	return 0;
}

int st_socket_generic_reader_end_of_file(struct st_stream_reader * io) {
	return 0;
}

off_t st_socket_generic_reader_forward(struct st_stream_reader * io, off_t offset) {
	return 0;
}

void st_socket_generic_reader_free(struct st_stream_reader * io) {
}

ssize_t st_socket_generic_reader_get_block_size(struct st_stream_reader * io) {
	return 0;
}

int st_socket_generic_reader_last_errno(struct st_stream_reader * io) {
	return 0;
}

ssize_t st_socket_generic_reader_position(struct st_stream_reader * io) {
	return 0;
}

ssize_t st_socket_generic_reader_read(struct st_stream_reader * io, void * buffer, ssize_t length) {
	return 0;
}


int st_socket_generic_writer_close(struct st_stream_writer * io) {
	return 0;
}

void st_socket_generic_writer_free(struct st_stream_writer * io) {
}

ssize_t st_socket_generic_writer_get_available_size(struct st_stream_writer * io) {
	return 0;
}

ssize_t st_socket_generic_writer_get_block_size(struct st_stream_writer * io) {
	return 0;
}

int st_socket_generic_writer_last_errno(struct st_stream_writer * io) {
	return 0;
}

ssize_t st_socket_generic_writer_position(struct st_stream_writer * io) {
	return 0;
}

ssize_t st_socket_generic_writer_write(struct st_stream_writer * io, const char * buffer, ssize_t length) {
	return 0;
}


struct st_io_socket * st_io_socket_new() {
	struct st_socket_generic_private * self = malloc(sizeof(struct st_socket_generic_private));
	self->fd = -1;
	self->connected = 0;

	struct st_io_socket * socket = malloc(sizeof(struct st_io_socket));
	socket->ops = &st_socket_generic_ops;
	socket->data = self;

	return socket;
}

