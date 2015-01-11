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
#include <libstoriqone/poll.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>

#include "drive.h"

static bool sochgr_drive_check_cookie(struct so_drive * dr, const char * cookie);
static bool sochgr_drive_check_support(struct so_drive * drive, struct so_media_format * format, bool for_writing);
static bool sochgr_drive_is_free(struct so_drive * drive);
static int sochgr_drive_load_media(struct so_drive * drive, struct so_media * media);
static int sochgr_drive_lock(struct so_drive * drive, const char * job_key);
static int sochgr_drive_reset(struct so_drive * drive);
static int sochgr_drive_update_status(struct so_drive * drive);

static struct so_drive_ops drive_ops = {
	.check_cookie  = sochgr_drive_check_cookie,
	.check_support = sochgr_drive_check_support,
	.is_free       = sochgr_drive_is_free,
	.load_media    = sochgr_drive_load_media,
	.lock          = sochgr_drive_lock,
	.reset         = sochgr_drive_reset,
	.update_status = sochgr_drive_update_status,
};

static struct so_value * log_config = NULL;
static struct so_value * db_config = NULL;


static bool sochgr_drive_check_cookie(struct so_drive * drive, const char * cookie) {
	struct sochgr_drive * self = drive->data;

	struct so_value * command = so_value_pack("{ssso}", "command", "check cookie", "cookie", cookie);
	so_json_encode_to_fd(command, self->fd_in, true);
	so_value_free(command);

	struct so_value * returned = so_json_parse_fd(self->fd_out, -1);
	bool valid = false;

	so_value_unpack(returned, "{sb}", "valid", &valid);
	so_value_free(returned);

	return valid;
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

	so_value_unpack(returned, "{sb}", "status", &ok);
	so_value_free(returned);

	return ok;
}

static bool sochgr_drive_is_free(struct so_drive * drive) {
	struct sochgr_drive * self = drive->data;

	struct so_value * command = so_value_pack("{ss}", "command", "is free");
	so_json_encode_to_fd(command, self->fd_in, true);
	so_value_free(command);

	struct so_value * returned = so_json_parse_fd(self->fd_out, -1);

	bool is_free = false;
	so_value_unpack(returned, "{sb}", "returned", &is_free);
	so_value_free(returned);

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

	struct so_value * returned = so_json_parse_fd(self->fd_out, -1);
	long long int status;

	so_value_unpack(returned, "{sb}", "status", &status);
	so_value_free(returned);

	return (int) status;
}

static int sochgr_drive_lock(struct so_drive * drive, const char * job_key) {
	if (!drive->enable)
		return -1;

	struct sochgr_drive * self = drive->data;

	struct so_value * command = so_value_pack("{sss{ss}}", "command", "lock", "params", "job key", job_key);
	so_json_encode_to_fd(command, self->fd_in, true);
	so_value_free(command);

	bool ok = false;
	struct so_value * returned = so_json_parse_fd(self->fd_out, -1);
	so_value_unpack(returned, "{sb}", "returned", &ok);
	so_value_free(returned);

	return ok ? 0 : -1;
}

void sochgr_drive_register(struct so_drive * drive, struct so_value * config, const char * process_name) {
	if (!drive->enable)
		return;

	struct sochgr_drive * self = malloc(sizeof(struct sochgr_drive));
	bzero(self, sizeof(struct sochgr_drive));

	so_process_new(&self->process, process_name, NULL, 0);
	self->fd_in = so_process_pipe_to(&self->process);
	self->fd_out = so_process_pipe_from(&self->process, so_process_stdout);
	so_process_set_null(&self->process, so_process_stderr);
	self->config = so_value_pack("{sOsOsO}", "logger", log_config, "drive", config, "database", db_config);

	drive->ops = &drive_ops;
	drive->data = self;

	so_process_start(&self->process, 1);
	so_json_encode_to_fd(self->config, self->fd_in, true);
}

static int sochgr_drive_reset(struct so_drive * drive) {
	struct sochgr_drive * self = drive->data;

	struct so_value * command = so_value_pack("{ssso}", "command", "reset", "slot", so_slot_convert(drive->slot));
	so_json_encode_to_fd(command, self->fd_in, true);
	so_value_free(command);

	struct so_value * returned = so_json_parse_fd(self->fd_out, -1);
	long int val = 1;

	so_value_unpack(returned, "{si}", "status", &val);
	so_value_free(returned);

	return (int) val;
}

void sochgr_drive_set_config(struct so_value * logger, struct so_value * db) {
	log_config = logger;
	db_config = db;
}

static int sochgr_drive_update_status(struct so_drive * drive) {
	struct sochgr_drive * self = drive->data;

	struct so_value * command = so_value_pack("{ss}", "command", "update status");
	so_json_encode_to_fd(command, self->fd_in, true);
	so_value_free(command);

	struct so_value * returned = so_json_parse_fd(self->fd_out, -1);
	long int val = 1;
	struct so_value * new_drive = NULL;

	so_value_unpack(returned, "{siso}", "status", &val, "drive", &new_drive);

	if (new_drive != NULL)
		so_drive_sync(drive, new_drive, true);

	so_value_free(returned);

	return (int) val;
}

