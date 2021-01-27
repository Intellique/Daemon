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

// free, malloc
#include <stdlib.h>
// bzero
#include <strings.h>
// send
#include <sys/socket.h>
// send
#include <sys/types.h>
// close
#include <unistd.h>

#include <libstoriqone/json.h>
#include <libstoriqone/format.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/value.h>

#include "../io.h"

struct soj_format_writer_private {
	struct so_drive * drive;

	int command_fd;
	int data_fd;

	ssize_t block_size;
	int last_errno;
	int file_position;
	ssize_t position;
	ssize_t available_size;
	char * last_alternate_path;

	struct so_value * digest;
};

static enum so_format_writer_status soj_format_writer_add_file(struct so_format_writer * fw, const struct so_format_file * file, const char * selected_path);
static enum so_format_writer_status soj_format_writer_add_label(struct so_format_writer * fw, const char * label);
static int soj_format_writer_close(struct so_format_writer * fw, bool change_volume);
static ssize_t soj_format_writer_compute_size_of_file(struct so_format_writer * fw, const struct so_format_file * file);
static ssize_t soj_format_writer_end_of_file(struct so_format_writer * fw);
static void soj_format_writer_free(struct so_format_writer * fw);
static char * soj_format_writer_get_alternate_path(struct so_format_writer * fw);
static ssize_t soj_format_writer_get_available_size(struct so_format_writer * fw);
static ssize_t soj_format_writer_get_block_size(struct so_format_writer * fw);
static struct so_value * soj_format_writer_get_digests(struct so_format_writer * fw);
static int soj_format_writer_file_position(struct so_format_writer * fw);
static int soj_format_writer_last_errno(struct so_format_writer * fw);
static ssize_t soj_format_writer_position(struct so_format_writer * fw);
static struct so_format_reader * soj_format_writer_reopen(struct so_format_writer * fw);
static enum so_format_writer_status soj_format_writer_restart_file(struct so_format_writer * fw, const struct so_format_file * file);
static ssize_t soj_format_writer_write(struct so_format_writer * fw, const void * buffer, ssize_t length);
static ssize_t soj_format_writer_write_metadata(struct so_format_writer * fw, struct so_value * metadata);

static struct so_format_writer_ops soj_format_writer_ops = {
	.add_file             = soj_format_writer_add_file,
	.add_label            = soj_format_writer_add_label,
	.close                = soj_format_writer_close,
	.compute_size_of_file = soj_format_writer_compute_size_of_file,
	.end_of_file          = soj_format_writer_end_of_file,
	.file_position        = soj_format_writer_file_position,
	.free                 = soj_format_writer_free,
	.get_alternate_path   = soj_format_writer_get_alternate_path,
	.get_available_size   = soj_format_writer_get_available_size,
	.get_block_size       = soj_format_writer_get_block_size,
	.get_digests          = soj_format_writer_get_digests,
	.last_errno           = soj_format_writer_last_errno,
	.position             = soj_format_writer_position,
	.reopen               = soj_format_writer_reopen,
	.restart_file         = soj_format_writer_restart_file,
	.write                = soj_format_writer_write,
	.write_metadata       = soj_format_writer_write_metadata,
};


struct so_format_writer * soj_format_new_writer(struct so_drive * drive, struct so_value * config) {
	struct so_value * socket_cmd = NULL, * socket_data = NULL;
	ssize_t block_size = 0;
	int file_position = -1;
	ssize_t available_size = -1;
	if (so_value_unpack(config, "{s{soso}szsisz}",
			"socket",
				"command", &socket_cmd,
				"data", &socket_data,
			"block size", &block_size,
			"file position", &file_position,
			"available size", &available_size
	) < 5)
		return NULL;

	struct soj_format_writer_private * self = malloc(sizeof(struct soj_format_writer_private));
	bzero(self, sizeof(struct soj_format_writer_private));
	self->drive = drive;
	self->command_fd = so_socket(socket_cmd);
	self->data_fd = so_socket(socket_data);
	self->block_size = block_size;
	self->last_errno = 0;
	self->file_position = file_position;
	self->position = 0;
	self->available_size = available_size;
	self->last_alternate_path = NULL;
	self->digest = so_value_new_linked_list();

	struct so_format_writer * writer = malloc(sizeof(struct so_format_writer));
	writer->ops = &soj_format_writer_ops;
	writer->data = self;

	return writer;
}


static enum so_format_writer_status soj_format_writer_add_file(struct so_format_writer * fw, const struct so_format_file * file, const char * selected_path) {
	soj_format_writer_add_file_async(fw, file, selected_path);
	return soj_format_writer_add_file_return(fw);
}

void soj_format_writer_add_file_async(struct so_format_writer * fw, const struct so_format_file * file, const char * selected_path) {
	struct soj_format_writer_private * self = fw->data;

	struct so_value * request = so_value_pack("{sss{soss}}",
		"command", "add file",
		"params",
			"file", so_format_file_convert(file),
			"selected path", selected_path
	);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);
}

enum so_format_writer_status soj_format_writer_add_file_return(struct so_format_writer * fw) {
	struct soj_format_writer_private * self = fw->data;

	enum so_format_writer_status status = so_format_writer_error;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response != NULL) {
		so_value_unpack(response, "{si}", "returned", &status);

		if (status == so_format_writer_error)
			so_value_unpack(response, "{si}", "last errno", &self->last_errno);
		else {
			if (self->last_alternate_path != NULL)
				free(self->last_alternate_path);

			so_value_unpack(response, "{szsz}",
				"position", &self->position,
				"available size", &self->available_size,
				"alternate path", &self->last_alternate_path
			);
		}
		so_value_free(response);
	}

	return status;
}

static enum so_format_writer_status soj_format_writer_add_label(struct so_format_writer * fw, const char * label) {
	struct soj_format_writer_private * self = fw->data;

	struct so_value * request = so_value_pack("{sss{ss}}",
		"command", "add label",
		"params",
			"label", label
	);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	enum so_format_writer_status status = so_format_writer_error;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response != NULL) {
		so_value_unpack(response, "{si}", "returned", &status);

		if (status == so_format_writer_error)
			so_value_unpack(response, "{si}", "last errno", &self->last_errno);
		else
			so_value_unpack(response, "{szsz}",
				"position", &self->position,
				"available size", &self->available_size
			);
		so_value_free(response);
	}

	return status;
}

static int soj_format_writer_close(struct so_format_writer * fw, bool change_volume) {
	struct soj_format_writer_private * self = fw->data;

	struct so_value * request = so_value_pack("{sss{sb}}",
		"command", "close",
		"params",
			"change volume", change_volume
	);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	int failed = -1;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response != NULL) {
		so_value_unpack(response, "{si}", "returned", &failed);

		if (failed != 0)
			so_value_unpack(response, "{si}", "last errno", &self->last_errno);
		else if (so_value_hashtable_has_key2(response, "digests")) {
			so_value_free(self->digest);
			so_value_unpack(response, "{sO}", "digests", &self->digest);
		}
		so_value_free(response);
	}

	return failed;
}

static ssize_t soj_format_writer_compute_size_of_file(struct so_format_writer * fw, const struct so_format_file * file) {
	struct soj_format_writer_private * self = fw->data;

	struct so_value * request = so_value_pack("{sss{so}}",
		"command", "compute size of file",
		"params",
			"file", so_format_file_convert(file)
	);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	ssize_t size = -1;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response != NULL) {
		so_value_unpack(response, "{sz}", "returned", &size);
		so_value_free(response);
	}

	return size;
}

static ssize_t soj_format_writer_end_of_file(struct so_format_writer * fw) {
	struct soj_format_writer_private * self = fw->data;

	struct so_value * request = so_value_pack("{ss}", "command", "end of file");
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	ssize_t eof = -1;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response != NULL) {
		so_value_unpack(response, "{sz}", "returned", &eof);

		if (eof < 0)
			so_value_unpack(response, "{si}", "last errno", &self->last_errno);
		else
			so_value_unpack(response, "{sisi}",
				"position", &self->position,
				"available size", &self->available_size
			);
		so_value_free(response);
	}

	return eof;
}

static int soj_format_writer_file_position(struct so_format_writer * fw) {
	struct soj_format_writer_private * self = fw->data;
	return self->file_position;
}

static void soj_format_writer_free(struct so_format_writer * fw) {
	struct soj_format_writer_private * self = fw->data;

	if (self->data_fd > -1)
		soj_format_writer_close(fw, false);

	struct so_value * request = so_value_pack("{ss}", "command", "free");
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	close(self->data_fd);
	self->data_fd = -1;

	so_value_free(self->digest);
	free(self);
	free(fw);
}

static char * soj_format_writer_get_alternate_path(struct so_format_writer * fw) {
	struct soj_format_writer_private * self = fw->data;

	char * alternate_path = self->last_alternate_path;
	self->last_alternate_path = NULL;
	return alternate_path;
}

static ssize_t soj_format_writer_get_available_size(struct so_format_writer * fw) {
	struct soj_format_writer_private * self = fw->data;
	return self->available_size;
}

static ssize_t soj_format_writer_get_block_size(struct so_format_writer * fw) {
	struct soj_format_writer_private * self = fw->data;
	return self->block_size;
}

static struct so_value * soj_format_writer_get_digests(struct so_format_writer * fw) {
	struct soj_format_writer_private * self = fw->data;
	return so_value_share(self->digest);
}

static int soj_format_writer_last_errno(struct so_format_writer * fw) {
	struct soj_format_writer_private * self = fw->data;
	return self->last_errno;
}

static ssize_t soj_format_writer_position(struct so_format_writer * fw) {
	struct soj_format_writer_private * self = fw->data;
	return self->position;
}

static struct so_format_reader * soj_format_writer_reopen(struct so_format_writer * fw) {
	struct soj_format_writer_private * self = fw->data;

	struct so_value * request = so_value_pack("{ss}", "command", "reopen");
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response == NULL)
		return NULL;

	bool ok = false;
	so_value_unpack(response, "{sb}", "status", &ok);
	if (ok && so_value_hashtable_has_key2(response, "digests")) {
		so_value_free(self->digest);
		so_value_unpack(response, "{sO}", "digests", &self->digest);
	}
	so_value_free(response);

	if (!ok)
		return NULL;

	struct so_format_reader * reader = soj_format_new_reader2(self->drive, self->command_fd, self->data_fd, self->block_size);
	self->data_fd = -1;
	return reader;
}

static enum so_format_writer_status soj_format_writer_restart_file(struct so_format_writer * fw, const struct so_format_file * file) {
	struct soj_format_writer_private * self = fw->data;

	struct so_value * request = so_value_pack("{sss{so}}",
		"command", "restart file",
		"params",
			"file", so_format_file_convert(file)
	);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	enum so_format_writer_status status = so_format_writer_error;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	if (response != NULL) {
		so_value_unpack(response, "{si}", "returned", &status);

		if (status == so_format_writer_error)
			so_value_unpack(response, "{si}", "last errno", &self->last_errno);
		else
			so_value_unpack(response, "{sisi}",
				"position", &self->position,
				"available size", &self->available_size
			);
		so_value_free(response);
	}

	return status;
}

static ssize_t soj_format_writer_write(struct so_format_writer * fw, const void * buffer, ssize_t length) {
	soj_format_writer_write_async(fw, buffer, length);
	return soj_format_writer_write_return(fw);
}

void soj_format_writer_write_async(struct so_format_writer * fw, const void * buffer, ssize_t length) {
	struct soj_format_writer_private * self = fw->data;

	struct so_value * request = so_value_pack("{sss{sz}}", "command", "write", "params", "length", length);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	send(self->data_fd, buffer, length, 0);
}

static ssize_t soj_format_writer_write_metadata(struct so_format_writer * fw, struct so_value * metadata) {
	struct soj_format_writer_private * self = fw->data;

	struct so_value * request = so_value_pack("{sss{sO}}", "command", "write metadata", "params", "metadata", metadata);
	so_json_encode_to_fd(request, self->command_fd, true);
	so_value_free(request);

	ssize_t nb_write = 0;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	so_value_unpack(response, "{sz}", "returned", &nb_write);
	if (nb_write < 0)
		so_value_unpack(response, "{si}", "last errno", &self->last_errno);
	else
		so_value_unpack(response, "{szsz}",
			"position", &self->position,
			"available size", &self->available_size
		);
	so_value_free(response);

	return nb_write;
}

ssize_t soj_format_writer_write_return(struct so_format_writer * fw) {
	struct soj_format_writer_private * self = fw->data;

	ssize_t nb_write = 0;
	struct so_value * response = so_json_parse_fd(self->command_fd, -1);
	so_value_unpack(response, "{si}", "returned", &nb_write);
	if (nb_write < 0)
		so_value_unpack(response, "{si}", "last errno", &self->last_errno);
	else
		so_value_unpack(response, "{szsz}",
			"position", &self->position,
			"available size", &self->available_size
		);
	so_value_free(response);

	return nb_write;
}
