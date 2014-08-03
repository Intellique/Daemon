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

struct stj_drive {
	int fd;
	struct st_value * config;
};

static int stj_drive_sync(struct st_drive * drive);

static struct st_drive_ops stj_drive_ops = {
	.sync = stj_drive_sync,
};


void stj_drive_init(struct st_drive * drive, struct st_value * config) {
	struct st_value * socket = NULL;
	st_value_unpack(config, "{sO}", "socket", &socket);

	struct stj_drive * self = drive->data = malloc(sizeof(struct stj_drive));
	bzero(self, sizeof(struct stj_drive));

	self->fd = st_socket(socket);
	self->config = socket;

	drive->ops = &stj_drive_ops;
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
	st_value_unpack(response, "{so}", "drive", &vdr);

	st_drive_sync(drive, vdr, true);

	st_value_free(response);

	return 0;
}

