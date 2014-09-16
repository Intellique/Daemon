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

// malloc
#include <stdlib.h>
// bzero
#include <strings.h>

#include <libstone/json.h>
#include <libstone/socket.h>
#include <libstone/value.h>

#include "drive.h"
#include "io.h"

struct stj_drive {
	int fd;
	struct st_value * config;
};

static bool stj_drive_check_header(struct st_drive * drive);
static bool stj_drive_check_support(struct st_drive * drive, struct st_media_format * format, bool for_writing);
static ssize_t stj_drive_find_best_block_size(struct st_drive * drive);
static int stj_drive_format_media(struct st_drive * drive, struct st_pool * pool);
static struct st_stream_reader * stj_drive_get_raw_reader(struct st_drive * drive, int file_position, const char * cookie);
static struct st_stream_writer * stj_drive_get_raw_writer(struct st_drive * drive, const char * cookie);
static char * stj_drive_lock(struct st_drive * drive);
static int stj_drive_sync(struct st_drive * drive);

static struct st_drive_ops stj_drive_ops = {
	.check_header         = stj_drive_check_header,
	.check_support        = stj_drive_check_support,
	.find_best_block_size = stj_drive_find_best_block_size,
	.format_media         = stj_drive_format_media,
	.get_raw_reader       = stj_drive_get_raw_reader,
	.get_raw_writer       = stj_drive_get_raw_writer,
	.lock                 = stj_drive_lock,
	.sync                 = stj_drive_sync,
};


static bool stj_drive_check_header(struct st_drive * drive) {
	struct stj_drive * self = drive->data;

	struct st_value * request = st_value_pack("{ss}", "command", "check header");
	st_json_encode_to_fd(request, self->fd, true);
	st_value_free(request);

	struct st_value * response = st_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return -1;

	bool ok = false;
	st_value_unpack(response, "{sb}", "returned", &ok);
	st_value_free(response);

	return ok;
}

static bool stj_drive_check_support(struct st_drive * drive, struct st_media_format * format, bool for_writing) {
	struct stj_drive * self = drive->data;

	struct st_value * command = st_value_pack("{sss{sosb}}",
		"command", "check support",
		"params",
			"format", st_media_format_convert(format),
			"for writing", for_writing
	);
	st_json_encode_to_fd(command, self->fd, true);
	st_value_free(command);

	struct st_value * returned = st_json_parse_fd(self->fd, -1);
	bool ok = false;

	st_value_unpack(returned, "{sb}", "returned", &ok);
	st_value_free(returned);

	return ok;
}

static ssize_t stj_drive_find_best_block_size(struct st_drive * drive) {
	struct stj_drive * self = drive->data;

	struct st_value * request = st_value_pack("{ss}", "command", "find best block size");
	st_json_encode_to_fd(request, self->fd, true);
	st_value_free(request);

	struct st_value * response = st_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return -1;

	ssize_t block_size = -1;
	st_value_unpack(response, "{si}", "returned", &block_size);
	st_value_free(response);

	return block_size;
}

static int stj_drive_format_media(struct st_drive * drive, struct st_pool * pool) {
	struct stj_drive * self = drive->data;

	struct st_value * request = st_value_pack("{sss{so}}",
		"command", "format media",
		"params",
			"pool", st_pool_convert(pool)
	);
	st_json_encode_to_fd(request, self->fd, true);
	st_value_free(request);

	struct st_value * response = st_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return -1;

	long int failed = false;
	st_value_unpack(response, "{si}", "returned", &failed);
	st_value_free(response);

	return failed;
}

static struct st_stream_reader * stj_drive_get_raw_reader(struct st_drive * drive, int file_position, const char * cookie) {
	struct stj_drive * self = drive->data;

	struct st_value * request = st_value_pack("{sssiss}", "command", "get raw reader", "file position", (long int) file_position, "cookie", cookie);
	st_json_encode_to_fd(request, self->fd, true);
	st_value_free(request);

	struct st_value * response = st_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return NULL;

	bool ok = false;
	st_value_unpack(response, "{sb}", "status", &ok);

	if (!ok) {
		st_value_free(response);
		return NULL;
	}

	struct st_stream_reader * reader = stj_io_new_stream_reader(drive, self->fd, response);
	st_value_free(response);
	return reader;
}

static struct st_stream_writer * stj_drive_get_raw_writer(struct st_drive * drive, const char * cookie) {
	struct stj_drive * self = drive->data;

	struct st_value * request = st_value_pack("{ssss}", "command", "get raw writer", "cookie", cookie);
	st_json_encode_to_fd(request, self->fd, true);
	st_value_free(request);

	struct st_value * response = st_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return NULL;

	bool ok = false;
	st_value_unpack(response, "{sb}", "status", &ok);

	if (!ok) {
		st_value_free(response);
		return NULL;
	}

	struct st_stream_writer * writer = stj_io_new_stream_writer(drive, self->fd, response);
	st_value_free(response);
	return writer;
}

void stj_drive_init(struct st_drive * drive, struct st_value * config) {
	struct st_value * socket = NULL;
	st_value_unpack(config, "{sO}", "socket", &socket);

	struct stj_drive * self = drive->data = malloc(sizeof(struct stj_drive));
	bzero(self, sizeof(struct stj_drive));

	self->fd = st_socket(socket);
	self->config = socket;

	drive->ops = &stj_drive_ops;
}

static char * stj_drive_lock(struct st_drive * drive) {
	struct stj_drive * self = drive->data;

	struct st_value * request = st_value_pack("{ss}", "command", "lock");
	st_json_encode_to_fd(request, self->fd, true);
	st_value_free(request);

	struct st_value * returned = st_json_parse_fd(self->fd, -1);
	if (returned == NULL)
		return NULL;

	bool locked = false;
	char * cookie = NULL;
	st_value_unpack(returned, "{s{sbss}}", "returned", "locked", &locked, "cookie", &cookie);
	st_value_free(returned);

	return cookie;
}

static int stj_drive_sync(struct st_drive * drive) {
	struct stj_drive * self = drive->data;

	struct st_value * request = st_value_pack("{ss}", "command", "sync");
	st_json_encode_to_fd(request, self->fd, true);
	st_value_free(request);

	struct st_value * response = st_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return 1;

	struct st_value * vdr = NULL;
	st_value_unpack(response, "{so}", "returned", &vdr);

	st_drive_sync(drive, vdr, true);

	st_value_free(response);

	return 0;
}

