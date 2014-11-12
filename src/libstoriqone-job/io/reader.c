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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// free, malloc
#include <stdlib.h>
// bzero
#include <strings.h>
// recv
#include <sys/socket.h>
// recv
#include <sys/types.h>
// close
#include <unistd.h>

#include <libstoriqone/json.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/value.h>

#include "../io.h"

struct soj_io_reader {
	struct so_drive * drive;

	int command_fd;
	int data_fd;

	ssize_t position;
	ssize_t block_size;

	int last_errno;
};

static int soj_io_reader_close(struct so_stream_reader * io);
static bool soj_io_reader_end_of_file(struct so_stream_reader * io);
static off_t soj_io_reader_forward(struct so_stream_reader * io, off_t offset);
static void soj_io_reader_free(struct so_stream_reader * io);
static ssize_t soj_io_reader_get_block_size(struct so_stream_reader * io);
static int soj_io_reader_last_errno(struct so_stream_reader * io);
static ssize_t soj_io_reader_position(struct so_stream_reader * io);
static ssize_t soj_io_reader_read(struct so_stream_reader * io, void * buffer, ssize_t length);

static struct so_stream_reader_ops soj_reader_ops = {
	.close          = soj_io_reader_close,
	.end_of_file    = soj_io_reader_end_of_file,
	.forward        = soj_io_reader_forward,
	.free           = soj_io_reader_free,
	.get_block_size = soj_io_reader_get_block_size,
	.last_errno     = soj_io_reader_last_errno,
	.position       = soj_io_reader_position,
	.read           = soj_io_reader_read,
};


static int soj_io_reader_close(struct so_stream_reader * io) {
	struct soj_io_reader * self = io->data;

	struct so_value * request = so_value_pack("{sb}", "stop", true);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	close(self->data_fd);
	self->data_fd = -1;

	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response == NULL)
		return 1;

	bool closed = false;
	so_value_unpack(response, "{sb}", "close", &closed);
	so_value_free(response);

	return closed ? 0 : 1;
}

static bool soj_io_reader_end_of_file(struct so_stream_reader * io __attribute__((unused))) {
	return false;
}

static off_t soj_io_reader_forward(struct so_stream_reader * io, off_t offset) {
	struct soj_io_reader * self = io->data;

	struct so_value * request = so_value_pack("{sbsssi}", "stop", false, "command", "forward", "offset", offset);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	so_value_unpack(response, "{si}", "new position", &self->position);
	so_value_free(response);

	self->position += offset;

	return self->position;
}

static void soj_io_reader_free(struct so_stream_reader * io) {
	struct soj_io_reader * self = io->data;

	if (self->data_fd > -1)
		soj_io_reader_close(io);

	free(self);
	free(io);
}

static ssize_t soj_io_reader_get_block_size(struct so_stream_reader * io) {
	struct soj_io_reader * self = io->data;
	return self->block_size;
}

static int soj_io_reader_last_errno(struct so_stream_reader * io) {
	struct soj_io_reader * self = io->data;
	return self->last_errno;
}

struct so_stream_reader * soj_io_new_stream_reader(struct so_drive * drive, int fd_command, struct so_value * config) {
	struct so_value * socket = NULL;
	long int block_size = 0;
	if (so_value_unpack(config, "{sosi}", "socket", &socket, "block size", &block_size) < 2)
		return NULL;

	int data_socket = so_socket(socket);

	struct soj_io_reader * self = malloc(sizeof(struct soj_io_reader));
	bzero(self, sizeof(struct soj_io_reader));
	self->drive = drive;
	self->command_fd = fd_command;
	self->data_fd = data_socket;
	self->position = 0;
	self->block_size = block_size;
	self->last_errno = 0;

	struct so_stream_reader * reader = malloc(sizeof(struct so_stream_reader));
	bzero(reader, sizeof(struct so_stream_reader));
	reader->ops = &soj_reader_ops;
	reader->data = self;

	return reader;
}

static ssize_t soj_io_reader_position(struct so_stream_reader * io) {
	struct soj_io_reader * self = io->data;
	return self->position;
}

static ssize_t soj_io_reader_read(struct so_stream_reader * io, void * buffer, ssize_t length) {
	struct soj_io_reader * self = io->data;

	struct so_value * request = so_value_pack("{sbsssi}", "stop", false, "command", "read", "length", length);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	ssize_t nb_read = recv(self->data_fd, buffer, length, 0);

	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	so_value_free(response);

	return nb_read;
}
