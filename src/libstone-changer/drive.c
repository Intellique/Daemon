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

#include <libstone/poll.h>
#include <libstone/value.h>

#include "drive.h"

static struct st_value * log_config = NULL;
static struct st_value * db_config = NULL;

__asm__(".symver stchgr_drive_register_v1, stchgr_drive_register@@LIBSTONE_CHANGER_1.2");
void stchgr_drive_register_v1(struct st_drive * drive, const char * process_name) {
	struct stchgr_drive * self = malloc(sizeof(struct stchgr_drive));
	bzero(self, sizeof(struct stchgr_drive));

	st_process_new(&self->process, process_name, NULL, 0);
	self->fd_in = st_process_pipe_to(&self->process);
	self->fd_out = st_process_pipe_from(&self->process, st_process_stdout);
	drive->data = self;

	// struct st_value * config = st_value_pack("{sOsO}", "logger", log_config, "database", db_config);

	// st_process_start(&self->process, 1);
	// st_json_encode_to_fd(config, self->fd_in, true);
}

void stchgr_drive_set_config_v1(struct st_value * logger, struct st_value * db) {
	log_config = logger;
	db_config = db;
}

