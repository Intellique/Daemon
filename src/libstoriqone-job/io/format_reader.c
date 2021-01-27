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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
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
#include <libstoriqone/format.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/value.h>

#include "../io.h"

struct soj_format_reader_private {
	struct so_drive * drive;

	int command_fd;
	int data_fd;

	ssize_t block_size;
	ssize_t position;

	int last_errno;

	struct so_value * digest;
};

static int soj_format_reader_close(struct so_format_reader * fr);
static bool soj_format_reader_end_of_file(struct so_format_reader * fr);
static enum so_format_reader_header_status soj_format_reader_forward(struct so_format_reader * fr, off_t offset);
static void soj_format_reader_free(struct so_format_reader * fr);
static ssize_t soj_format_reader_get_block_size(struct so_format_reader * fr);
static struct so_value * soj_format_reader_get_digests(struct so_format_reader * fr);
static enum so_format_reader_header_status soj_format_reader_get_header(struct so_format_reader * fr, struct so_format_file * file, const char * expected_path, const char * selected_path);
static char * soj_format_reader_get_root(struct so_format_reader * fr);
static int soj_format_reader_last_errno(struct so_format_reader * fr);
static ssize_t soj_format_reader_position(struct so_format_reader * fr);
static ssize_t soj_format_reader_read(struct so_format_reader * fr, void * buffer, ssize_t length);
static ssize_t soj_format_reader_read_to_end_of_data(struct so_format_reader * fr);
static int soj_format_reader_rewind(struct so_format_reader * fr);
static enum so_format_reader_header_status soj_format_reader_skip_file(struct so_format_reader * fr);

static struct so_format_reader_ops soj_format_reader_ops = {
	.close               = soj_format_reader_close,
	.end_of_file         = soj_format_reader_end_of_file,
	.forward             = soj_format_reader_forward,
	.free                = soj_format_reader_free,
	.get_block_size      = soj_format_reader_get_block_size,
	.get_digests         = soj_format_reader_get_digests,
	.get_header          = soj_format_reader_get_header,
	.get_root            = soj_format_reader_get_root,
	.last_errno          = soj_format_reader_last_errno,
	.position            = soj_format_reader_position,
	.read                = soj_format_reader_read,
	.read_to_end_of_data = soj_format_reader_read_to_end_of_data,
	.rewind              = soj_format_reader_rewind,
	.skip_file           = soj_format_reader_skip_file,
};


struct so_format_reader * soj_format_new_reader(struct so_drive * drive, struct so_value * config) {
	struct so_value * socket_cmd = NULL, * socket_data = NULL;
	ssize_t block_size = 0;
	if (so_value_unpack(config, "{s{soso}sz}", "socket", "command", &socket_cmd, "data", &socket_data, "block size", &block_size) < 2)
		return NULL;

	return soj_format_new_reader2(drive, so_socket(socket_cmd), so_socket(socket_data), block_size);
}

struct so_format_reader * soj_format_new_reader2(struct so_drive * drive, int fd_command, int fd_data, ssize_t block_size) {
	struct soj_format_reader_private * self = malloc(sizeof(struct soj_format_reader_private));
	bzero(self, sizeof(struct soj_format_reader_private));
	self->drive = drive;
	self->command_fd = fd_command;
	self->data_fd = fd_data;
	self->block_size = block_size;
	self->position = 0;
	self->last_errno = 0;
	self->digest = so_value_new_hashtable2();

	struct so_format_reader * reader = malloc(sizeof(struct so_format_reader));
	reader->ops = &soj_format_reader_ops;
	reader->data = self;

	return reader;
}


static int soj_format_reader_close(struct so_format_reader * fr) {
	struct soj_format_reader_private * self = fr->data;

	struct so_value * request = so_value_pack("{ss}", "command", "close");
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	close(self->data_fd);
	self->data_fd = -1;

	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response == NULL)
		return 1;

	int failed = 0;
	so_value_unpack(response, "{si}", "returned", &failed);
	if (failed != 0)
		so_value_unpack(response, "{si}", "last errno", &self->last_errno);
	else if (so_value_hashtable_has_key2(response, "digests")) {
		so_value_free(self->digest);
		so_value_unpack(response, "{sO}", "digests", &self->digest);
	}
	so_value_free(response);

	return failed;
}

static bool soj_format_reader_end_of_file(struct so_format_reader * fr) {
	struct soj_format_reader_private * self = fr->data;

	struct so_value * request = so_value_pack("{ss}", "command", "end of file");
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	bool eof = true;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response != NULL) {
		so_value_unpack(response, "{sbsi}",
			"returned", &eof,
			"last errno", &self->last_errno
		);
		so_value_free(response);
	}

	return eof;
}

static enum so_format_reader_header_status soj_format_reader_forward(struct so_format_reader * fr, off_t offset) {
	struct soj_format_reader_private * self = fr->data;

	struct so_value * request = so_value_pack("{sss{sz}}",
		"command", "forward",
		"params",
			"offset", offset
	);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	enum so_format_reader_header_status status = so_format_reader_header_bad_header;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response != NULL) {
		so_value_unpack(response, "{sisz}",
			"returned", &status,
			"position", &self->position
		);
		if (status != so_format_reader_header_ok)
			so_value_unpack(response, "{si}", "last errno", &self->last_errno);
		so_value_free(response);
	}

	return status;
}

static void soj_format_reader_free(struct so_format_reader * fr) {
	struct soj_format_reader_private * self = fr->data;

	if (self->data_fd > -1)
		soj_format_reader_close(fr);

	so_value_free(self->digest);
	free(self);
	free(fr);
}

static ssize_t soj_format_reader_get_block_size(struct so_format_reader * fr) {
	struct soj_format_reader_private * self = fr->data;
	return self->block_size;
}

static struct so_value * soj_format_reader_get_digests(struct so_format_reader * fr) {
	struct soj_format_reader_private * self = fr->data;
	return so_value_share(self->digest);
}

static enum so_format_reader_header_status soj_format_reader_get_header(struct so_format_reader * fr, struct so_format_file * file, const char * expected_path, const char * selected_path) {
	struct soj_format_reader_private * self = fr->data;

	struct so_value * request = so_value_pack("{sss{ssss}}",
		"command", "get header",
		"params",
			"expected_path", expected_path,
			"selected_path", selected_path
	);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response == NULL)
		return so_format_reader_header_bad_header;

	int status = 0;
	struct so_value * vfile = NULL;

	so_value_unpack(response, "{s{siso}szsi}",
		"returned",
			"status", &status,
			"file", &vfile,
		"position", &self->position,
		"last errno", &self->last_errno
	);

	so_format_file_init(file);
	so_format_file_sync(file, vfile);

	so_value_free(response);

	return status;
}

static char * soj_format_reader_get_root(struct so_format_reader * fr) {
	struct soj_format_reader_private * self = fr->data;

	struct so_value * request = so_value_pack("{ss}", "command", "get root");
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	char * root = NULL;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response != NULL) {
		so_value_unpack(response, "{ss}", "returned", &root);
		so_value_free(response);
	}

	return root;
}

static int soj_format_reader_last_errno(struct so_format_reader * fr) {
	struct soj_format_reader_private * self = fr->data;
	return self->last_errno;
}

static ssize_t soj_format_reader_position(struct so_format_reader * fr) {
	struct soj_format_reader_private * self = fr->data;
	return self->position;
}

static ssize_t soj_format_reader_read(struct so_format_reader * fr, void * buffer, ssize_t length) {
	struct soj_format_reader_private * self = fr->data;

	struct so_value * request = so_value_pack("{sss{sz}}",
		"command", "read",
		"params",
			"length", length
	);
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

		if (nb_read > 0)
			nb_total_read += nb_read;

		if (fds[0].revents & POLLIN) {
			struct so_value * response = so_json_parse_fd(self->command_fd, -1);
			so_value_unpack(response, "{sz}", "position", &self->position);
			if (nb_read < 0)
				so_value_unpack(response, "{si}", "last errno", &self->last_errno);
			so_value_free(response);

			return nb_total_read;
		}
	}

	return -1;
}

static ssize_t soj_format_reader_read_to_end_of_data(struct so_format_reader * fr) {
	struct soj_format_reader_private * self = fr->data;

	struct so_value * request = so_value_pack("{ss}", "command", "read to end of data");
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response != NULL) {
		so_value_unpack(response, "{sz}", "returned", &self->position);
		if (self->position == -1)
			so_value_unpack(response, "{si}", "last errno", &self->last_errno);
		so_value_free(response);
	}

	return self->position;
}

static int soj_format_reader_rewind(struct so_format_reader * fr) {
	struct soj_format_reader_private * self = fr->data;

	struct so_value * request = so_value_pack("{ss}", "command", "rewind");
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	int failed = -1;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response != NULL) {
		so_value_unpack(response, "{si}", "returned", &failed);
		if (failed != 0)
			so_value_unpack(response, "{si}", "last errno", &self->last_errno);
		so_value_free(response);
	}

	return failed;
}

static enum so_format_reader_header_status soj_format_reader_skip_file(struct so_format_reader * fr) {
	struct soj_format_reader_private * self = fr->data;

	struct so_value * request = so_value_pack("{ss}", "command", "skip file");
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	enum so_format_reader_header_status status = so_format_reader_header_bad_header;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response != NULL) {
		so_value_unpack(response, "{siszsi}",
			"returned", &status,
			"position", &self->position,
			"last errno", &self->last_errno
		);
		so_value_free(response);
	}

	return status;
}
