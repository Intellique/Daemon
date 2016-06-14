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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// snprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// bzero
#include <strings.h>
// close
#include <unistd.h>

#include <libstoriqone/config.h>
#include <libstoriqone/json.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>

#include "drive.h"

static bool sochgr_drive_check_format(struct so_drive * drive, struct so_media * media, struct so_pool * pool, const char * archive_uuid);
static bool sochgr_drive_check_support(struct so_drive * drive, struct so_media_format * format, bool for_writing);
static void sochgr_drive_free(struct so_drive * drive);
static bool sochgr_drive_is_free(struct so_drive * drive);
static int sochgr_drive_load_media(struct so_drive * drive, struct so_media * media);
static int sochgr_drive_lock(struct so_drive * drive, const char * job_id);
static int sochgr_drive_reset(struct so_drive * drive);
static int sochgr_drive_stop(struct so_drive * drive);
static int sochgr_drive_update_status(struct so_drive * drive);

static struct so_drive_ops drive_ops = {
	.check_format  = sochgr_drive_check_format,
	.check_support = sochgr_drive_check_support,
	.free          = sochgr_drive_free,
	.is_free       = sochgr_drive_is_free,
	.load_media    = sochgr_drive_load_media,
	.lock          = sochgr_drive_lock,
	.reset         = sochgr_drive_reset,
	.stop          = sochgr_drive_stop,
	.update_status = sochgr_drive_update_status,
};

static struct so_value * changer_socket = NULL;
static struct so_value * drives_config = NULL;
static struct so_value * log_config = NULL;
static struct so_value * db_config = NULL;


static bool sochgr_drive_check_format(struct so_drive * drive, struct so_media * media, struct so_pool * pool, const char * archive_uuid) {
	struct sochgr_drive * self = drive->data;

	struct so_value * command = so_value_pack("{sss{sososs}}",
		"command", "check format",
		"params",
			"media", so_media_convert(media),
			"pool", so_pool_convert(pool),
			"archive uuid", archive_uuid
	);
	so_json_encode_to_fd(command, self->fd_in, true);
	so_value_free(command);

	struct so_value * returned = so_json_parse_fd(self->fd_out, -1);
	bool ok = false;

	if (returned != NULL) {
		so_value_unpack(returned, "{sb}", "status", &ok);
		so_value_free(returned);
	}

	return ok;
}

static bool sochgr_drive_check_support(struct so_drive * drive, struct so_media_format * format, bool for_writing) {
	struct sochgr_drive * self = drive->data;

	struct so_value * command = so_value_pack("{sss{sosb}}",
		"command", "check support",
		"params",
			"format", so_media_format_convert(format),
			"for writing", for_writing
	);
	so_json_encode_to_fd(command, self->fd_in, true);
	so_value_free(command);

	struct so_value * returned = so_json_parse_fd(self->fd_out, -1);
	bool ok = false;

	if (returned != NULL) {
		so_value_unpack(returned, "{sb}", "status", &ok);
		so_value_free(returned);
	}

	return ok;
}

static void sochgr_drive_free(struct so_drive * drive) {
	if (drive == NULL)
		return;

	struct sochgr_drive * self = drive->data;
	if (!self->process.has_exited)
		so_process_wait(&self->process, 1, true);
	so_process_free(&self->process, 1);

	close(self->fd_in);
	close(self->fd_out);
	so_value_free(self->config);

	free(self);
}

struct so_value * sochgr_drive_get_configs() {
	return drives_config;
}

static bool sochgr_drive_is_free(struct so_drive * drive) {
	struct sochgr_drive * self = drive->data;

	struct so_value * command = so_value_pack("{ss}", "command", "is free");
	so_json_encode_to_fd(command, self->fd_in, true);
	so_value_free(command);

	bool is_free = false;
	struct so_value * returned = so_json_parse_fd(self->fd_out, -1);
	if (returned != NULL) {
		so_value_unpack(returned, "{sb}", "returned", &is_free);
		so_value_free(returned);
	}

	return is_free;
}

static int sochgr_drive_load_media(struct so_drive * drive, struct so_media * media) {
	struct sochgr_drive * self = drive->data;

	struct so_value * command = so_value_pack("{sss{so}}",
		"command", "load media",
		"params",
			"media", so_media_convert(media)
	);
	so_json_encode_to_fd(command, self->fd_in, true);
	so_value_free(command);

	int status = -1;
	struct so_value * returned = so_json_parse_fd(self->fd_out, -1);
	if (returned != NULL) {
		so_value_unpack(returned, "{si}", "status", &status);
		so_value_free(returned);
	}

	return status;
}

static int sochgr_drive_lock(struct so_drive * drive, const char * job_id) {
	if (!drive->enable)
		return -1;

	struct sochgr_drive * self = drive->data;

	struct so_value * command = so_value_pack("{sss{ss}}",
		"command", "lock",
		"params",
			"job id", job_id
	);
	so_json_encode_to_fd(command, self->fd_in, true);
	so_value_free(command);

	bool ok = false;
	struct so_value * returned = so_json_parse_fd(self->fd_out, -1);
	if (returned != NULL) {
		so_value_unpack(returned, "{sb}", "returned", &ok);
		so_value_free(returned);
	}

	return ok ? 0 : -1;
}

void sochgr_drive_register(struct so_drive * drive, struct so_value * config, const char * process_name) {
	if (!drive->enable)
		return;

	struct sochgr_drive * self = malloc(sizeof(struct sochgr_drive));
	bzero(self, sizeof(struct sochgr_drive));

	/**
	 * valgrind
	 * valgrind -v --log-file=valgrind.log --num-callers=24 --leak-check=full --show-reachable=yes --track-origins=yes ./bin/stoned
	 *
	 * const char * params[] = { "-v", "--log-file=valgrind/drive.log", "--track-fds=yes", "--time-stamp=yes", "--num-callers=24", "--leak-check=full", "--show-reachable=yes", "--track-origins=yes", "--fullpath-after=/home/guillaume/prog/StoriqOne/", process_name, buffer_index };
	 * so_process_new(&self->process, "valgrind", params, 11);
	 */

	char buffer_index[12];
	snprintf(buffer_index, 12, "#%u", drive->index);

	const char * params[] = { buffer_index };
	so_process_new(&self->process, process_name, params, 1);

	self->fd_in = so_process_pipe_to(&self->process);
	self->fd_out = so_process_pipe_from(&self->process, so_process_stdout);
	so_process_set_null(&self->process, so_process_stderr);
	self->config = so_value_pack("{sOsOsOsOsO}",
		"logger", log_config,
		"drive", config,
		"database", db_config,
		"socket", changer_socket,
		"default values", so_config_get()
	);

	drive->ops = &drive_ops;
	drive->data = self;

	so_process_start(&self->process, 1);
	so_json_encode_to_fd(self->config, self->fd_in, true);

	struct so_value * dr_config = so_json_parse_fd(self->fd_out, -1);

	if (drives_config == NULL)
		drives_config = so_value_new_linked_list();

	if (so_value_list_get_length(drives_config) <= drive->index)
		so_value_list_push(drives_config, dr_config, true);
}

static int sochgr_drive_reset(struct so_drive * drive) {
	struct sochgr_drive * self = drive->data;

	struct so_value * command = so_value_pack("{ssso}",
		"command", "reset",
		"slot", so_slot_convert(drive->slot)
	);
	so_json_encode_to_fd(command, self->fd_in, true);
	so_value_free(command);

	int val = 1;
	struct so_value * returned = so_json_parse_fd(self->fd_out, -1);
	if (returned != NULL) {
		so_value_unpack(returned, "{si}", "status", &val);
		so_value_free(returned);
	}

	return val;
}

void sochgr_drive_set_config(struct so_value * logger, struct so_value * db, struct so_value * socket_template) {
	log_config = logger;
	db_config = db;
	changer_socket = socket_template;
}

static int sochgr_drive_stop(struct so_drive * drive) {
	struct sochgr_drive * self = drive->data;

	struct so_value * command = so_value_pack("{ss}", "command", "stop");
	so_json_encode_to_fd(command, self->fd_in, true);
	so_value_free(command);

	struct so_value * returned = so_json_parse_fd(self->fd_out, -1);
	int val = 1;

	if (returned != NULL) {
		so_value_unpack(returned, "{si}", "status", &val);
		so_value_free(returned);
	}

	if (val == 0) {
		struct sochgr_drive * self = drive->data;
		so_process_wait(&self->process, 1, true);
	}

	return val;
}

static int sochgr_drive_update_status(struct so_drive * drive) {
	struct sochgr_drive * self = drive->data;

	struct so_value * command = so_value_pack("{ss}", "command", "update status");
	so_json_encode_to_fd(command, self->fd_in, true);
	so_value_free(command);

	int val = 1;
	struct so_value * new_drive = NULL;

	struct so_value * returned = so_json_parse_fd(self->fd_out, -1);
	if (returned != NULL) {
		so_value_unpack(returned, "{siso}",
			"status", &val,
			"drive", &new_drive
		);

		if (new_drive != NULL)
			so_drive_sync(drive, new_drive, true);

		so_value_free(returned);
	}

	return val;
}

