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
*  Last modified: Fri, 27 Jan 2012 20:39:45 +0100                         *
\*************************************************************************/

// errno
#include <errno.h>
#include <netdb.h>
// malloc
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <stone/io/socket.h>

struct st_socket_generic_private {
	int fd;
	enum st_socket_status {
		st_socket_status_binded,
		st_socket_status_connected,
		st_socket_status_closed,
		st_socket_status_new,
	} status;
	int last_errno;

	off_t input_position;
	off_t output_position;

	short input_closed;
	short output_closed;
};

static struct st_io_socket * st_socket_generic_accept(struct st_io_socket * socket);
static int st_socket_generic_bind(struct st_io_socket * socket, const char * hostname, unsigned short port);
static int st_socket_generic_connect(struct st_io_socket * socket, const char * hostname, unsigned short port);
static void st_socket_generic_free(struct st_io_socket * socket);
static struct st_stream_writer * st_socket_generic_get_input_stream(struct st_io_socket * socket);
static struct st_stream_reader * st_socket_generic_get_output_stream(struct st_io_socket * socket);
static int st_socket_generic_is_binded(struct st_io_socket * socket);
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
static ssize_t st_socket_generic_writer_write(struct st_stream_writer * io, const void * buffer, ssize_t length);

static struct st_io_socket_ops st_socket_generic_ops = {
	.accept            = st_socket_generic_accept,
	.bind              = st_socket_generic_bind,
	.connect           = st_socket_generic_connect,
	.free              = st_socket_generic_free,
	.get_input_stream  = st_socket_generic_get_input_stream,
	.get_output_stream = st_socket_generic_get_output_stream,
	.is_binded         = st_socket_generic_is_binded,
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


struct st_io_socket * st_socket_generic_accept(struct st_io_socket * s) {
	struct st_socket_generic_private * self = s->data;
	if (self->status != st_socket_status_binded)
		return 0;

	struct sockaddr addr;
	socklen_t addr_length = sizeof(addr);
	int new_socket = accept(self->fd, &addr, &addr_length);

	if (new_socket < 0)
		return 0;

	struct st_io_socket * ns = st_io_socket_new();
	struct st_socket_generic_private * nsp = ns->data;
	nsp->fd = new_socket;
	nsp->status = st_socket_status_connected;

	return ns;
}

int st_socket_generic_bind(struct st_io_socket * s, const char * hostname, unsigned short port) {
	struct st_socket_generic_private * self = s->data;
	if (self->status != st_socket_status_new)
		return -1;

	struct addrinfo hint = {
		.ai_family    = AF_INET,
		.ai_socktype  = SOCK_STREAM,
		.ai_flags     = AI_PASSIVE,
		.ai_protocol  = 0,
		.ai_canonname = 0,
		.ai_addr      = 0,
		.ai_next      = 0,
	};

	char sport[6];
	snprintf(sport, 6, "%hu", port);

	struct addrinfo * addresses, * ptr;
	int failed = getaddrinfo(hostname, sport, &hint, &addresses);
	if (failed)
		return failed;

	for (ptr = addresses; ptr; ptr = ptr->ai_next) {
		self->fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		if (self->fd < 0)
			continue;

		if (bind(self->fd, ptr->ai_addr, ptr->ai_addrlen) == 0)
			break;

		close(self->fd);
		self->fd = -1;
	}

	freeaddrinfo(addresses);

	if (ptr) {
		self->status = st_socket_status_binded;
		failed = listen(self->fd, 16);
		return 0;
	}

	return -1;
}

int st_socket_generic_connect(struct st_io_socket * s, const char * hostname, unsigned short port) {
	if (!s || !hostname)
		return -1;

	struct st_socket_generic_private * self = s->data;
	if (self->status != st_socket_status_new)
		return -1;

	char sport[6];
	snprintf(sport, 6, "%hu", port);

	struct addrinfo * addresses;
	if (getaddrinfo(hostname, sport, 0, &addresses))
		return -2;

	struct addrinfo * ptr;
	for (ptr = addresses, self->fd = -1; ptr && self->fd < 0; ptr = ptr->ai_next) {
		self->fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		if (self->fd < 0)
			continue;

		if (connect(self->fd, ptr->ai_addr, ptr->ai_addrlen) == -1) {
			close(self->fd);
			self->fd = -1;
		}
	}

	freeaddrinfo(addresses);

	if (self->fd < 0)
		return -3;

	self->status = st_socket_status_connected;
	return 0;
}

void st_socket_generic_free(struct st_io_socket * socket) {
	if (!socket)
		return;

	struct st_socket_generic_private * self = socket->data;
	if (self->fd > -1)
		close(self->fd);

	free(self);
	free(socket);
}

struct st_stream_writer * st_socket_generic_get_input_stream(struct st_io_socket * socket) {
	struct st_socket_generic_private * self = socket->data;
	if (self->status != st_socket_status_connected)
		return 0;

	struct st_stream_writer * sw = malloc(sizeof(struct st_stream_writer));
	sw->ops = &st_socket_generic_writer_ops;
	sw->data = self;

	return sw;
}

struct st_stream_reader * st_socket_generic_get_output_stream(struct st_io_socket * socket) {
	struct st_socket_generic_private * self = socket->data;
	if (self->status != st_socket_status_connected)
		return 0;

	struct st_stream_reader * sr = malloc(sizeof(struct st_stream_reader));
	sr->ops = &st_socket_generic_reader_ops;
	sr->data = self;

	return sr;
}

int st_socket_generic_is_binded(struct st_io_socket * socket) {
	struct st_socket_generic_private * self = socket->data;
	return self->status == st_socket_status_binded;
}

int st_socket_generic_is_connected(struct st_io_socket * socket) {
	struct st_socket_generic_private * self = socket->data;
	return self->status == st_socket_status_connected;
}


int st_socket_generic_reader_close(struct st_stream_reader * io) {
	struct st_socket_generic_private * self = io->data;
	int failed = shutdown(self->fd, SHUT_RD);
	if (!failed)
		self->input_closed = 1;
	return failed;
}

int st_socket_generic_reader_end_of_file(struct st_stream_reader * io) {
	struct st_socket_generic_private * self = io->data;
	return self->input_closed;
}

off_t st_socket_generic_reader_forward(struct st_stream_reader * io, off_t offset) {
	struct st_socket_generic_private * self = io->data;
	if (self->input_closed)
		return self->input_position;

	char buffer[4096];
	ssize_t nb_total_read = 0;
	while (nb_total_read < offset) {
		ssize_t will_read = nb_total_read + 4096 < offset ? 4096 : offset - nb_total_read;
		ssize_t nb_read = recv(self->fd, buffer, will_read, 0);
		if (nb_read < 0) {
			self->last_errno = errno;
			return -1;
		}

		self->input_position += nb_read;
		nb_total_read += nb_read;
	}

	return self->input_position;
}

void st_socket_generic_reader_free(struct st_stream_reader * io) {
	io->ops = 0;
	io->data = 0;
}

ssize_t st_socket_generic_reader_get_block_size(struct st_stream_reader * io __attribute__((unused))) {
	return 4096;
}

int st_socket_generic_reader_last_errno(struct st_stream_reader * io) {
	struct st_socket_generic_private * self = io->data;
	return self->last_errno;
}

ssize_t st_socket_generic_reader_position(struct st_stream_reader * io) {
	struct st_socket_generic_private * self = io->data;
	return self->input_position;
}

ssize_t st_socket_generic_reader_read(struct st_stream_reader * io, void * buffer, ssize_t length) {
	struct st_socket_generic_private * self = io->data;
	ssize_t nb_read = recv(self->fd, buffer, length, 0);
	if (nb_read < 0)
		self->last_errno = errno;
	else if (nb_read > 0)
		self->input_position += nb_read;
	return nb_read;
}


int st_socket_generic_writer_close(struct st_stream_writer * io) {
	struct st_socket_generic_private * self = io->data;
	int failed = shutdown(self->fd, SHUT_WR);
	if (!failed)
		self->output_closed = 1;
	return failed;
}

void st_socket_generic_writer_free(struct st_stream_writer * io) {
	io->ops = 0;
	io->data = 0;
}

ssize_t st_socket_generic_writer_get_available_size(struct st_stream_writer * io) {
	struct st_socket_generic_private * self = io->data;
	return self->output_closed ? 0 : 4096;
}

ssize_t st_socket_generic_writer_get_block_size(struct st_stream_writer * io __attribute__((unused))) {
	return 4096;
}

int st_socket_generic_writer_last_errno(struct st_stream_writer * io) {
	struct st_socket_generic_private * self = io->data;
	return self->last_errno;
}

ssize_t st_socket_generic_writer_position(struct st_stream_writer * io) {
	struct st_socket_generic_private * self = io->data;
	return self->output_position;
}

ssize_t st_socket_generic_writer_write(struct st_stream_writer * io, const void * buffer, ssize_t length) {
	struct st_socket_generic_private * self = io->data;
	ssize_t nb_write = send(self->fd, buffer, length, 0);
	if (nb_write < 0)
		self->last_errno = errno;
	else if (nb_write > 0)
		self->output_position += nb_write;
	return nb_write;
}


struct st_io_socket * st_io_socket_new() {
	struct st_socket_generic_private * self = malloc(sizeof(struct st_socket_generic_private));
	self->fd = -1;
	self->status = st_socket_status_new;
	self->last_errno = 0;

	self->input_closed = 0;
	self->output_closed = 0;

	self->input_position = 0;
	self->output_position = 0;

	struct st_io_socket * socket = malloc(sizeof(struct st_io_socket));
	socket->ops = &st_socket_generic_ops;
	socket->data = self;

	return socket;
}

