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

// free, malloc
#include <stdlib.h>
// bzero
#include <strings.h>
// recv, send
#include <sys/socket.h>
// recv, send
#include <sys/types.h>
// close
#include <unistd.h>

#include <libstone/json.h>
#include <libstone/socket.h>
#include <libstone/value.h>

#include "../io.h"

struct stj_io_writer {
	struct st_drive * drive;

	int command_fd;
	int data_fd;

	ssize_t position;
	ssize_t block_size;
	ssize_t available_size;

	int last_errno;
};

static ssize_t stj_io_writer_before_close(struct st_stream_writer * sfw, void * buffer, ssize_t length);
static int stj_io_writer_close(struct st_stream_writer * sfw);
static void stj_io_writer_free(struct st_stream_writer * sfw);
static ssize_t stj_io_writer_get_available_size(struct st_stream_writer * sfw);
static ssize_t stj_io_writer_get_block_size(struct st_stream_writer * sfw);
static int stj_io_writer_last_errno(struct st_stream_writer * sfw);
static ssize_t stj_io_writer_position(struct st_stream_writer * sfw);
static struct st_stream_reader * stj_io_writer_reopen(struct st_stream_writer * sfw);
static ssize_t stj_io_writer_write(struct st_stream_writer * sfw, const void * buffer, ssize_t length);

static struct st_stream_writer_ops stj_writer_ops = {
	.before_close = stj_io_writer_before_close,
	.close = stj_io_writer_close,
	.free = stj_io_writer_free,
	.get_available_size = stj_io_writer_get_available_size,
	.get_block_size = stj_io_writer_get_block_size,
	.last_errno = stj_io_writer_last_errno,
	.position = stj_io_writer_position,
	.reopen = stj_io_writer_reopen,
	.write = stj_io_writer_write,
};


static ssize_t stj_io_writer_before_close(struct st_stream_writer * sfw, void * buffer, ssize_t length) {
	struct stj_io_writer * self = sfw->data;

	struct st_value * request = st_value_pack("{sbsssi}", "stop", false, "command", "before close", "length", length);
	st_json_encode_to_fd(request, self->command_fd, true);
	st_value_free(request);

	ssize_t nb_read = recv(self->data_fd, buffer, length, 0);

	struct st_value * response = st_json_parse_fd(self->command_fd, -1);
	st_value_free(response);

	return nb_read;
}

static int stj_io_writer_close(struct st_stream_writer * sfw) {
	struct stj_io_writer * self = sfw->data;

	struct st_value * request = st_value_pack("{sb}", "stop", true);
	st_json_encode_to_fd(request, self->command_fd, true);
	st_value_free(request);

	close(self->data_fd);
	self->data_fd = -1;

	struct st_value * response = st_json_parse_fd(self->command_fd, -1);
	if (response == NULL)
		return 1;

	bool closed = false;
	st_value_unpack(response, "{sb}", "close", &closed);
	st_value_free(response);

	return closed ? 0 : 1;
}

static void stj_io_writer_free(struct st_stream_writer * sfw) {
	struct stj_io_writer * self = sfw->data;

	if (self->data_fd > -1)
		stj_io_writer_close(sfw);

	free(self);
	free(sfw);
}

static ssize_t stj_io_writer_get_available_size(struct st_stream_writer * sfw) {
	struct stj_io_writer * self = sfw->data;
	return self->available_size;
}

static ssize_t stj_io_writer_get_block_size(struct st_stream_writer * sfw) {
	struct stj_io_writer * self = sfw->data;
	return self->block_size;
}

static int stj_io_writer_last_errno(struct st_stream_writer * sfw) {
	struct stj_io_writer * self = sfw->data;
	return self->last_errno;
}

static ssize_t stj_io_writer_position(struct st_stream_writer * sfw) {
	struct stj_io_writer * self = sfw->data;
	return self->position;
}

static struct st_stream_reader * stj_io_writer_reopen(struct st_stream_writer * sfw __attribute__((unused))) {
	return NULL;
}

static ssize_t stj_io_writer_write(struct st_stream_writer * sfw, const void * buffer, ssize_t length) {
	struct stj_io_writer * self = sfw->data;

	struct st_value * request = st_value_pack("{sbsssi}", "stop", false, "command", "write", "length", length);
	st_json_encode_to_fd(request, self->command_fd, true);
	st_value_free(request);

	ssize_t nb_write = send(self->data_fd, buffer, length, 0);

	struct st_value * response = st_json_parse_fd(self->command_fd, -1);
	st_value_free(response);

	return nb_write;
}

struct st_stream_writer * stj_io_new_stream_writer(struct st_drive * drive, int fd_command, struct st_value * config) {
	struct st_value * socket = NULL;
	long int block_size = 0;
	if (st_value_unpack(config, "{sosi}", "socket", &socket, "block size", &block_size) < 2)
		return NULL;

	int data_socket = st_socket(socket);

	struct stj_io_writer * self = malloc(sizeof(struct stj_io_writer));
	bzero(self, sizeof(struct stj_io_writer));
	self->drive = drive;
	self->command_fd = fd_command;
	self->data_fd = data_socket;
	self->position = 0;
	self->block_size = block_size;
	self->last_errno = 0;

	struct st_stream_writer * writer = malloc(sizeof(struct st_stream_writer));
	bzero(writer, sizeof(struct st_stream_writer));
	writer->ops = &stj_writer_ops;
	writer->data = self;

	return writer;
}

