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

#include <libstoriqone/json.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/value.h>

#include "../io.h"

struct soj_stream_writer {
	struct so_drive * drive;

	int command_fd;
	int data_fd;

	int file_position;
	ssize_t position;
	ssize_t block_size;
	ssize_t available_size;

	long int last_errno;
};

static ssize_t soj_stream_writer_before_close(struct so_stream_writer * sw, void * buffer, ssize_t length);
static int soj_stream_writer_close(struct so_stream_writer * sw);
static int soj_stream_writer_file_position(struct so_stream_writer * sw);
static void soj_stream_writer_free(struct so_stream_writer * sw);
static ssize_t soj_stream_writer_get_available_size(struct so_stream_writer * sw);
static ssize_t soj_stream_writer_get_block_size(struct so_stream_writer * sw);
static int soj_stream_writer_last_errno(struct so_stream_writer * sw);
static ssize_t soj_stream_writer_position(struct so_stream_writer * sw);
static struct so_stream_reader * soj_stream_writer_reopen(struct so_stream_writer * sw);
static ssize_t soj_stream_writer_write(struct so_stream_writer * sw, const void * buffer, ssize_t length);

static struct so_stream_writer_ops soj_writer_ops = {
	.before_close       = soj_stream_writer_before_close,
	.close              = soj_stream_writer_close,
	.file_position      = soj_stream_writer_file_position,
	.free               = soj_stream_writer_free,
	.get_available_size = soj_stream_writer_get_available_size,
	.get_block_size     = soj_stream_writer_get_block_size,
	.last_errno         = soj_stream_writer_last_errno,
	.position           = soj_stream_writer_position,
	.reopen             = soj_stream_writer_reopen,
	.write              = soj_stream_writer_write,
};


struct so_stream_writer * soj_stream_new_writer(struct so_drive * drive, int fd_command, struct so_value * config) {
	struct so_value * socket = NULL;
	long int block_size = 0;
	long int file_position = -1;
	if (so_value_unpack(config, "{sosisi}", "socket", &socket, "block size", &block_size, "file position", &file_position) < 2)
		return NULL;

	int data_socket = so_socket(socket);

	struct soj_stream_writer * self = malloc(sizeof(struct soj_stream_writer));
	bzero(self, sizeof(struct soj_stream_writer));
	self->drive = drive;
	self->command_fd = fd_command;
	self->data_fd = data_socket;
	self->position = 0;
	self->file_position = file_position;
	self->block_size = block_size;
	self->last_errno = 0;

	struct so_stream_writer * writer = malloc(sizeof(struct so_stream_writer));
	bzero(writer, sizeof(struct so_stream_writer));
	writer->ops = &soj_writer_ops;
	writer->data = self;

	return writer;
}


static ssize_t soj_stream_writer_before_close(struct so_stream_writer * sw, void * buffer, ssize_t length) {
	struct soj_stream_writer * self = sw->data;

	struct so_value * request = so_value_pack("{sss{si}}", "command", "writer: before close", "params", "length", length);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	ssize_t nb_read = 0;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	so_value_unpack(response, "{si}", "returned", &nb_read);
	if (nb_read < 0)
		so_value_unpack(response, "{si}", "last errno", &self->last_errno);

	so_value_free(response);

	if (nb_read > 0) {
		self->position += nb_read;
		return recv(self->data_fd, buffer, nb_read, 0);
	}

	return 0;
}

static int soj_stream_writer_close(struct so_stream_writer * sw) {
	struct soj_stream_writer * self = sw->data;

	struct so_value * request = so_value_pack("{ss}", "command", "writer: close");
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	if (self->data_fd >= 0)
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

static int soj_stream_writer_file_position(struct so_stream_writer * sw) {
	struct soj_stream_writer * self = sw->data;
	return self->file_position;
}

static void soj_stream_writer_free(struct so_stream_writer * sw) {
	struct soj_stream_writer * self = sw->data;

	if (self->data_fd > -1)
		soj_stream_writer_close(sw);

	free(self);
	free(sw);
}

static ssize_t soj_stream_writer_get_available_size(struct so_stream_writer * sw) {
	struct soj_stream_writer * self = sw->data;
	return self->available_size;
}

static ssize_t soj_stream_writer_get_block_size(struct so_stream_writer * sw) {
	struct soj_stream_writer * self = sw->data;
	return self->block_size;
}

static int soj_stream_writer_last_errno(struct so_stream_writer * sw) {
	struct soj_stream_writer * self = sw->data;
	return self->last_errno;
}

static ssize_t soj_stream_writer_position(struct so_stream_writer * sw) {
	struct soj_stream_writer * self = sw->data;
	return self->position;
}

static struct so_stream_reader * soj_stream_writer_reopen(struct so_stream_writer * sw) {
	struct soj_stream_writer * self = sw->data;

	struct so_value * request = so_value_pack("{ss}", "command", "writer: reopen");
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	bool ok = false;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	so_value_unpack(response, "{sb}", "returned", &ok);
	if (!ok)
		so_value_unpack(response, "{si}", "last errno", &self->last_errno);
	so_value_free(response);

	if (!ok)
		return NULL;

	struct so_stream_reader * new_reader = soj_stream_new_reader2(self->drive, self->command_fd, self->data_fd, self->block_size);

	self->command_fd = -1;
	self->data_fd = -1;

	return new_reader;
}

static ssize_t soj_stream_writer_write(struct so_stream_writer * sw, const void * buffer, ssize_t length) {
	struct soj_stream_writer * self = sw->data;

	struct so_value * request = so_value_pack("{sss{si}}", "command", "writer: write", "params", "length", length);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	send(self->data_fd, buffer, length, 0);

	ssize_t nb_write = 0;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	so_value_unpack(response, "{si}", "returned", &nb_write);
	if (nb_write < 0)
		so_value_unpack(response, "{si}", "last errno", &self->last_errno);
	so_value_free(response);

	if (nb_write > 0)
		self->position += nb_write;

	return nb_write;
}

