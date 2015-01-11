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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

// malloc
#include <stdlib.h>
// bzero
#include <strings.h>

#include <libstoriqone/json.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/value.h>

#include "drive.h"
#include "io.h"
#include "job.h"

struct soj_drive {
	int fd;
	struct so_value * config;
};

static bool soj_drive_check_header(struct so_drive * drive);
static bool soj_drive_check_support(struct so_drive * drive, struct so_media_format * format, bool for_writing);
static ssize_t soj_drive_find_best_block_size(struct so_drive * drive);
static int soj_drive_format_media(struct so_drive * drive, struct so_pool * pool);
static struct so_stream_reader * soj_drive_get_raw_reader(struct so_drive * drive, int file_position);
static struct so_stream_writer * soj_drive_get_raw_writer(struct so_drive * drive);
static int soj_drive_sync(struct so_drive * drive);

static struct so_drive_ops soj_drive_ops = {
	.check_header         = soj_drive_check_header,
	.check_support        = soj_drive_check_support,
	.find_best_block_size = soj_drive_find_best_block_size,
	.format_media         = soj_drive_format_media,
	.get_raw_reader       = soj_drive_get_raw_reader,
	.get_raw_writer       = soj_drive_get_raw_writer,
	.sync                 = soj_drive_sync,
};


static bool soj_drive_check_header(struct so_drive * drive) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{sss{ss}}",
		"command", "check header",
		"params",
			"job key", job->key
	);
	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return -1;

	bool ok = false;
	so_value_unpack(response, "{sb}", "returned", &ok);
	so_value_free(response);

	return ok;
}

static bool soj_drive_check_support(struct so_drive * drive, struct so_media_format * format, bool for_writing) {
	struct soj_drive * self = drive->data;

	struct so_value * command = so_value_pack("{sss{sosb}}",
		"command", "check support",
		"params",
			"format", so_media_format_convert(format),
			"for writing", for_writing
	);
	so_json_encode_to_fd(command, self->fd, true);
	so_value_free(command);

	struct so_value * returned = so_json_parse_fd(self->fd, -1);
	bool ok = false;

	so_value_unpack(returned, "{sb}", "returned", &ok);
	so_value_free(returned);

	return ok;
}

static ssize_t soj_drive_find_best_block_size(struct so_drive * drive) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{sss{ss}}",
		"command", "find best block size",
		"params",
			"job key", job->key
	);
	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return -1;

	ssize_t block_size = -1;
	so_value_unpack(response, "{si}", "returned", &block_size);
	so_value_free(response);

	return block_size;
}

static int soj_drive_format_media(struct so_drive * drive, struct so_pool * pool) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{sss{ssso}}",
		"command", "format media",
		"params",
			"job key", job->key,
			"pool", so_pool_convert(pool)
	);
	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return -1;

	long int failed = false;
	so_value_unpack(response, "{si}", "returned", &failed);
	so_value_free(response);

	return failed;
}

static struct so_stream_reader * soj_drive_get_raw_reader(struct so_drive * drive, int file_position) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{sss{sssi}}",
		"command", "get raw reader",
		"params",
			"job key", job->key,
			"file position", (long int) file_position
	);
	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return NULL;

	bool ok = false;
	so_value_unpack(response, "{sb}", "status", &ok);

	if (!ok) {
		so_value_free(response);
		return NULL;
	}

	struct so_stream_reader * reader = soj_io_new_stream_reader(drive, self->fd, response);
	so_value_free(response);
	return reader;
}

static struct so_stream_writer * soj_drive_get_raw_writer(struct so_drive * drive) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{sss{ss}}",
		"command", "get raw writer",
		"params",
			"job key", job->key
	);
	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return NULL;

	bool ok = false;
	so_value_unpack(response, "{sb}", "status", &ok);

	if (!ok) {
		so_value_free(response);
		return NULL;
	}

	struct so_stream_writer * writer = soj_io_new_stream_writer(drive, self->fd, response);
	so_value_free(response);
	return writer;
}

void soj_drive_init(struct so_drive * drive, struct so_value * config) {
	struct so_value * socket = NULL;
	so_value_unpack(config, "{sO}", "socket", &socket);

	struct soj_drive * self = drive->data = malloc(sizeof(struct soj_drive));
	bzero(self, sizeof(struct soj_drive));

	self->fd = so_socket(socket);
	self->config = socket;

	drive->ops = &soj_drive_ops;
}

static int soj_drive_sync(struct so_drive * drive) {
	struct soj_drive * self = drive->data;

	struct so_value * request = so_value_pack("{ss}", "command", "sync");
	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return 1;

	struct so_value * vdr = NULL;
	so_value_unpack(response, "{so}", "returned", &vdr);

	so_drive_sync(drive, vdr, true);

	so_value_free(response);

	return 0;
}

