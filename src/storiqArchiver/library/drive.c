/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 28 Nov 2011 11:35:18 +0100                         *
\*************************************************************************/

// open
#include <fcntl.h>
// malloc
#include <stdlib.h>
// struct mtget
#include <sys/mtio.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>

#include <storiqArchiver/library/drive.h>

#include "common.h"

struct sa_drive_generic {
	int fd_nst;
};

static ssize_t sa_drive_get_block_size(struct sa_drive * drive);
static int sa_drive_generic_rewind(struct sa_drive * drive);
static int sa_drive_generic_set_file_position(struct sa_drive * drive, int file_position);
static void sa_drive_generic_update_status(struct sa_drive * drive);

static struct sa_drive_ops sa_drive_generic_ops = {
	.rewind            = sa_drive_generic_rewind,
	.set_file_position = sa_drive_generic_set_file_position,
};


ssize_t sa_drive_get_block_size(struct sa_drive * drive) {
	return drive->block_size;
}

int sa_drive_generic_rewind(struct sa_drive * drive) {}

int sa_drive_generic_set_file_position(struct sa_drive * drive, int file_position) {}

void sa_drive_generic_update_status(struct sa_drive * drive) {
	struct sa_drive_generic * self = drive->data;

	struct mtget status;
	int failed = ioctl(self->fd_nst, MTIOCGET, &status);

	if (!failed) {
		drive->is_bottom_of_tape = GMT_BOT(status.mt_gstat) ? 1 : 0;
		drive->is_end_of_file    = GMT_EOF(status.mt_gstat) ? 1 : 0;
		drive->is_end_of_tape    = GMT_EOT(status.mt_gstat) ? 1 : 0;
		drive->is_writable       = GMT_WR_PROT(status.mt_gstat) ? 0 : 1;
		drive->is_online         = GMT_ONLINE(status.mt_gstat) ? 1 : 0;
		drive->is_door_opened    = GMT_DR_OPEN(status.mt_gstat) ? 1 : 0;

		drive->block_size = (status.mt_dsreg & MT_ST_BLKSIZE_MASK) >> MT_ST_BLKSIZE_SHIFT;
	}
}

void sa_drive_setup(struct sa_drive * drive) {
	int fd = open(drive->device, O_RDWR);

	struct sa_drive_generic * self = malloc(sizeof(struct sa_drive_generic));
	self->fd_nst = fd;

	drive->ops = &sa_drive_generic_ops;
	drive->data = self;

	sa_drive_generic_update_status(drive);
}

