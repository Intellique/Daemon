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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// poll
#include <poll.h>
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

struct soj_stream_reader_private {
	struct so_drive * drive;

	int command_fd;
	int data_fd;

	ssize_t position;
	ssize_t block_size;

	int last_errno;
};

static int soj_stream_reader_close(struct so_stream_reader * sr);
static bool soj_stream_reader_end_of_file(struct so_stream_reader * sr);
static off_t soj_stream_reader_forward(struct so_stream_reader * sr, off_t offset);
static void soj_stream_reader_free(struct so_stream_reader * sr);
static ssize_t soj_stream_reader_get_block_size(struct so_stream_reader * sr);
static int soj_stream_reader_last_errno(struct so_stream_reader * sr);
static ssize_t soj_stream_reader_position(struct so_stream_reader * sr);
static ssize_t soj_stream_reader_read(struct so_stream_reader * sr, void * buffer, ssize_t length);
static int soj_stream_reader_rewind(struct so_stream_reader * sr);

static struct so_stream_reader_ops soj_reader_ops = {
	.close          = soj_stream_reader_close,
	.end_of_file    = soj_stream_reader_end_of_file,
	.forward        = soj_stream_reader_forward,
	.free           = soj_stream_reader_free,
	.get_block_size = soj_stream_reader_get_block_size,
	.last_errno     = soj_stream_reader_last_errno,
	.position       = soj_stream_reader_position,
	.read           = soj_stream_reader_read,
	.rewind         = soj_stream_reader_rewind,
};


struct so_stream_reader * soj_stream_new_reader(struct so_drive * drive, struct so_value * config) {
	struct so_value * socket_cmd = NULL, * socket_data = NULL;
	long int block_size = 0;
	if (so_value_unpack(config, "{s{soso}si}", "socket", "command", &socket_cmd, "data", &socket_data, "block size", &block_size) < 2)
		return NULL;

	int cmd_socket = so_socket(socket_cmd);
	int data_socket = so_socket(socket_data);

	return soj_stream_new_reader2(drive, cmd_socket, data_socket, block_size);
}

struct so_stream_reader * soj_stream_new_reader2(struct so_drive * drive, int fd_command, int fd_data, ssize_t block_size) {
	struct soj_stream_reader_private * self = malloc(sizeof(struct soj_stream_reader_private));
	bzero(self, sizeof(struct soj_stream_reader_private));
	self->drive = drive;
	self->command_fd = fd_command;
	self->data_fd = fd_data;
	self->position = 0;
	self->block_size = block_size;
	self->last_errno = 0;

	struct so_stream_reader * reader = malloc(sizeof(struct so_stream_reader));
	bzero(reader, sizeof(struct so_stream_reader));
	reader->ops = &soj_reader_ops;
	reader->data = self;

	return reader;
}


static int soj_stream_reader_close(struct so_stream_reader * sr) {
	struct soj_stream_reader_private * self = sr->data;

	struct so_value * request = so_value_pack("{ss}", "command", "close");
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	close(self->data_fd);
	self->data_fd = -1;

	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response == NULL)
		return 1;

	long int failed = 0;
	so_value_unpack(response, "{sb}", "returned", &failed);
	if (failed != 0)
		so_value_unpack(response, "{si}", "last errno", &self->last_errno);
	so_value_free(response);

	return failed ? 1 : 0;
}

static bool soj_stream_reader_end_of_file(struct so_stream_reader * sr __attribute__((unused))) {
	struct soj_stream_reader_private * self = sr->data;

	struct so_value * request = so_value_pack("{ss}", "command", "end of file");
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	close(self->data_fd);
	self->data_fd = -1;

	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response == NULL)
		return true;

	long int failed = 0;
	so_value_unpack(response, "{sb}", "returned", &failed);
	if (failed != 0)
		so_value_unpack(response, "{si}", "last errno", &self->last_errno);
	so_value_free(response);

	return failed != 0;
}

static off_t soj_stream_reader_forward(struct so_stream_reader * sr, off_t offset) {
	struct soj_stream_reader_private * self = sr->data;

	struct so_value * request = so_value_pack("{sss{sz}}", "command", "forward", "params", "offset", offset);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	off_t new_position = (off_t) -1;

	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	so_value_unpack(response, "{si}", "returned", &new_position);
	if (new_position == (off_t) -1)
		so_value_unpack(response, "{si}", "last errno", &self->last_errno);
	else
		self->position = new_position;
	so_value_free(response);

	self->position += offset;

	return self->position;
}

static void soj_stream_reader_free(struct so_stream_reader * sr) {
	struct soj_stream_reader_private * self = sr->data;

	if (self->data_fd > -1)
		soj_stream_reader_close(sr);

	free(self);
	free(sr);
}

static ssize_t soj_stream_reader_get_block_size(struct so_stream_reader * sr) {
	struct soj_stream_reader_private * self = sr->data;
	return self->block_size;
}

static int soj_stream_reader_last_errno(struct so_stream_reader * sr) {
	struct soj_stream_reader_private * self = sr->data;
	return self->last_errno;
}

static ssize_t soj_stream_reader_position(struct so_stream_reader * sr) {
	struct soj_stream_reader_private * self = sr->data;
	return self->position;
}

static ssize_t soj_stream_reader_read(struct so_stream_reader * sr, void * buffer, ssize_t length) {
	struct soj_stream_reader_private * self = sr->data;

	struct so_value * request = so_value_pack("{sss{si}}", "command", "read", "params", "length", length);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	size_t nb_total_read = 0;
	for (;;) {
		struct pollfd fds[] = {
			{ self->command_fd, POLLIN | POLLHUP, 0 },
			{ self->data_fd,    POLLIN | POLLHUP, 0 },
		};

		poll(fds, 2, -1);

		if (fds[0].revents & POLLHUP)
			return -1;

		ssize_t nb_read = -1;
		if (fds[1].revents & POLLIN)
			nb_read = recv(self->data_fd, buffer + nb_total_read, length - nb_total_read, 0);

		if (nb_read > 0) {
			self->position += nb_read;
			nb_total_read += nb_read;
		}

		if (fds[0].revents & POLLIN) {
			struct so_value * response = so_json_parse_fd(self->command_fd, -1);
			if (nb_read < 0)
				so_value_unpack(response, "{si}", "last errno", &self->last_errno);
			so_value_free(response);

			return nb_total_read;
		}
	}

	return -1;
}

static int soj_stream_reader_rewind(struct so_stream_reader * sr) {
	struct soj_stream_reader_private * self = sr->data;

	struct so_value * request = so_value_pack("{ss}", "command", "rewind");
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	long long failed = false;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	so_value_unpack(response, "{si}", "returned", &failed);
	if (failed != 0)
		so_value_unpack(response, "{si}", "last errno", &self->last_errno);
	else
		self->position = 0;
	so_value_free(response);

	return failed;
}

