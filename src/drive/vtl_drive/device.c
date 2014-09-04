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

#include <libstone/slot.h>
#include <libstone/value.h>
#include <libstone-drive/drive.h>

#include "device.h"

static bool vtl_drive_check_header(struct st_database_connection * db);
static bool vtl_drive_check_support(struct st_media_format * format, bool for_writing, struct st_database_connection * db);
static int vtl_drive_format_media(struct st_pool * pool, struct st_database_connection * db);
static void vtl_drive_create_media(struct st_database_connection * db);
static ssize_t vtl_drive_find_best_block_size(struct st_database_connection * db);
static struct st_stream_reader * vtl_drive_get_raw_reader(int file_position, struct st_database_connection * db);
static struct st_stream_writer * vtl_drive_get_raw_writer(bool append, struct st_database_connection * db);
static int vtl_drive_init(struct st_value * config);
static void vtl_drive_on_failed(bool verbose, unsigned int sleep_time);
static int vtl_drive_reset(struct st_database_connection * db);
static int vtl_drive_set_file_position(int file_position, struct st_database_connection * db);
static int vtl_drive_update_status(struct st_database_connection * db);

static struct st_drive_ops vtl_drive_ops = {
	.check_header         = vtl_drive_check_header,
	.check_support        = vtl_drive_check_support,
	.find_best_block_size = vtl_drive_find_best_block_size,
	.format_media         = vtl_drive_format_media,
	.get_raw_reader       = vtl_drive_get_raw_reader,
	.get_raw_writer       = vtl_drive_get_raw_writer,
	.init                 = vtl_drive_init,
	.reset                = vtl_drive_reset,
	.update_status        = vtl_drive_update_status,
};

static struct st_drive vtl_drive = {
	.status      = st_drive_status_unknown,
	.enable      = true,

	.density_code       = 0,
	.mode               = st_media_format_mode_linear,
	.operation_duration = 0,
	.is_empty           = true,

	.model         = "Stone vtl drive",
	.vendor        = "Intellique",
	.revision      = "A01",
	.serial_number = NULL,

	.changer = NULL,
	.index   = 0,
	.slot    = NULL,

	.ops = &vtl_drive_ops,

	.data    = NULL,
	.db_data = NULL,
};


static bool vtl_drive_check_header(struct st_database_connection * db) {}

static bool vtl_drive_check_support(struct st_media_format * format, bool for_writing, struct st_database_connection * db) {}

static int vtl_drive_format_media(struct st_pool * pool, struct st_database_connection * db) {}

static ssize_t vtl_drive_find_best_block_size(struct st_database_connection * db) {}

struct st_drive * vtl_drive_get_device() {
	return &vtl_drive;
}

static struct st_stream_reader * vtl_drive_get_raw_reader(int file_position, struct st_database_connection * db) {}

static struct st_stream_writer * vtl_drive_get_raw_writer(bool append, struct st_database_connection * db) {}

static int vtl_drive_init(struct st_value * config) {
	struct st_slot * sl = vtl_drive.slot = malloc(sizeof(struct st_slot));
	bzero(vtl_drive.slot, sizeof(struct st_slot));
	sl->drive = &vtl_drive;

	char * root_dir = NULL;
	st_value_unpack(config, "{ssss}", "serial number", &vtl_drive.serial_number, "root directory", &root_dir);
}

static void vtl_drive_on_failed(bool verbose, unsigned int sleep_time) {}

static int vtl_drive_reset(struct st_database_connection * db) {}

static int vtl_drive_set_file_position(int file_position, struct st_database_connection * db) {}

static int vtl_drive_update_status(struct st_database_connection * db) {}

