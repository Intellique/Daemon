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
#include <libstone/poll.h>
#include <libstone/value.h>

#include "drive.h"

static bool stchgr_drive_is_loocked(struct st_drive * dr);
static int stchgr_drive_update_status(struct st_drive * drive);

static struct st_drive_ops drive_ops = {
	.is_locked     = stchgr_drive_is_loocked,
	.update_status = stchgr_drive_update_status,
};

static struct st_value * log_config = NULL;
static struct st_value * db_config = NULL;


static bool stchgr_drive_is_loocked(struct st_drive * dr) {
	return false;
}

__asm__(".symver stchgr_drive_register_v1, stchgr_drive_register@@LIBSTONE_CHANGER_1.2");
void stchgr_drive_register_v1(struct st_drive * drive, struct st_value * config, const char * process_name) {
	if (!drive->enable)
		return;

	struct stchgr_drive * self = malloc(sizeof(struct stchgr_drive));
	bzero(self, sizeof(struct stchgr_drive));

	st_process_new(&self->process, process_name, NULL, 0);
	self->fd_in = st_process_pipe_to(&self->process);
	self->fd_out = st_process_pipe_from(&self->process, st_process_stdout);
	self->config = st_value_pack("{sOsOsO}", "logger", log_config, "drive", config, "database", db_config);

	drive->ops = &drive_ops;
	drive->data = self;

	st_process_start(&self->process, 1);
	st_json_encode_to_fd(self->config, self->fd_in, true);
}

void stchgr_drive_set_config(struct st_value * logger, struct st_value * db) {
	log_config = logger;
	db_config = db;
}

static int stchgr_drive_update_status(struct st_drive * drive) {
	struct stchgr_drive * self = drive->data;

	struct st_value * command = st_value_pack("{ss}", "command", "update status");
	st_json_encode_to_fd(command, self->fd_in, true);
	st_value_free(command);

	struct st_value * returned = st_json_parse_fd(self->fd_out, -1);
	int val = 1;
	struct st_value * new_drive = NULL;

	st_value_unpack(returned, "{siso}", "status", &val, "drive", &new_drive);

	if (new_drive != NULL)
		st_drive_sync(drive, new_drive);

	st_value_free(returned);

	return val;
}

