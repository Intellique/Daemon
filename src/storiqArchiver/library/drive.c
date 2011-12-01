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
*  Last modified: Thu, 01 Dec 2011 15:24:43 +0100                         *
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
// read
#include <unistd.h>

#include <storiqArchiver/library/drive.h>
#include <storiqArchiver/log.h>

#include "common.h"

struct sa_drive_generic {
	int fd_nst;
};

static int sa_drive_generic_eod(struct sa_drive * drive);
static ssize_t sa_drive_generic_get_block_size(struct sa_drive * drive);
static struct sa_stream_reader * sa_drive_generic_get_reader(struct sa_drive * drive);
static void sa_drive_generic_on_failed(struct sa_drive * drive);
static int sa_drive_generic_rewind(struct sa_drive * drive);
static int sa_drive_generic_set_file_position(struct sa_drive * drive, int file_position);
static void sa_drive_generic_update_status(struct sa_drive * drive);

static struct sa_drive_ops sa_drive_generic_ops = {
	.eod               = sa_drive_generic_eod,
	.get_block_size    = sa_drive_generic_get_block_size,
	.get_reader        = sa_drive_generic_get_reader,
	.rewind            = sa_drive_generic_rewind,
	.set_file_position = sa_drive_generic_set_file_position,
};


int sa_drive_generic_eod(struct sa_drive * drive) {
	struct sa_drive_generic * self = drive->data;

	sa_log_write_all(sa_log_level_debug, "Drive: do space to end of tape");

	struct mtop eod = { MTEOM, 1 };
	int failed = ioctl(self->fd_nst, MTIOCTOP, &eod);

	if (failed) {
		sa_drive_generic_on_failed(drive);
		sa_drive_generic_update_status(drive);
		failed = ioctl(self->fd_nst, MTIOCTOP, &eod);
	}

	sa_log_write_all(failed ? sa_log_level_error : sa_log_level_debug, "Drive: space to end of tape, finish with code = %d", failed);

	sa_drive_generic_update_status(drive);

	return failed;
}

ssize_t sa_drive_generic_get_block_size(struct sa_drive * drive) {
	if (drive->block_size < 1) {
		sa_drive_generic_update_status(drive);

		if (!drive->is_bottom_of_tape)
			sa_drive_generic_rewind(drive);

		struct sa_drive_generic * self = drive->data;

		unsigned int i;
		ssize_t nb_read;
		ssize_t block_size = 1 << 15;
		char * buffer = malloc(block_size);
		for (i = 0; i < 5; i++) {
			nb_read = read(self->fd_nst, buffer, block_size);
			sa_drive_generic_rewind(drive);

			if (nb_read > 0) {
				free(buffer);
				return nb_read;
			}

			sa_drive_generic_on_failed(drive);

			block_size <<= 1;
			buffer = realloc(buffer, block_size);
		}

		sa_log_write_all(sa_log_level_error, "Drive: do failed to detect block size");
	}

	return drive->block_size;
}

struct sa_stream_reader * sa_drive_generic_get_reader(struct sa_drive * drive) {
	struct sa_drive_generic * self = drive->data;

	// if (drive->status != sa_drive_loaded_idle);
	return sa_drive_reader_new(drive, self->fd_nst);
}

void sa_drive_generic_on_failed(struct sa_drive * drive) {
	struct sa_drive_generic * self = drive->data;

	sa_log_write_all(sa_log_level_debug, "Drive: Try to recover an error");
	close(self->fd_nst);
	sleep(20);
	self->fd_nst = open(drive->device, O_RDWR | O_NONBLOCK);
}

int sa_drive_generic_rewind(struct sa_drive * drive) {
	struct sa_drive_generic * self = drive->data;

	sa_log_write_all(sa_log_level_debug, "Drive: do rewind tape");

	struct mtop rewind = { MTREW, 1 };
	int failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);

	unsigned int i;
	for (i = 0; i < 3 && failed; i++) {
		sa_drive_generic_on_failed(drive);
		sa_drive_generic_update_status(drive);
		failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);
	}

	sa_log_write_all(failed ? sa_log_level_error : sa_log_level_debug, "Drive: rewind tape, finish with code = %d", failed);

	sa_drive_generic_update_status(drive);

	return failed;
}

int sa_drive_generic_set_file_position(struct sa_drive * drive, int file_position) {}

void sa_drive_generic_update_status(struct sa_drive * drive) {
	struct sa_drive_generic * self = drive->data;

	struct mtget status;
	int failed = ioctl(self->fd_nst, MTIOCGET, &status);

	if (failed) {
		sa_drive_generic_on_failed(drive);
		failed = ioctl(self->fd_nst, MTIOCGET, &status);
	}

	if (failed) {
		drive->status = sa_drive_error;

		struct mtop reset = { MTRESET, 1 };
		failed = ioctl(self->fd_nst, MTIOCTOP, &reset);

		if (!failed)
			failed = ioctl(self->fd_nst, MTIOCGET, &status);
	}

	if (!failed) {
		drive->nb_files     = status.mt_fileno;
		drive->block_number = status.mt_blkno;

		drive->is_bottom_of_tape = GMT_BOT(status.mt_gstat) ? 1 : 0;
		drive->is_end_of_file    = GMT_EOF(status.mt_gstat) ? 1 : 0;
		drive->is_end_of_tape    = GMT_EOT(status.mt_gstat) ? 1 : 0;
		drive->is_writable       = GMT_WR_PROT(status.mt_gstat) ? 0 : 1;
		drive->is_online         = GMT_ONLINE(status.mt_gstat) ? 1 : 0;
		drive->is_door_opened    = GMT_DR_OPEN(status.mt_gstat) ? 1 : 0;

		drive->block_size   = (status.mt_dsreg & MT_ST_BLKSIZE_MASK) >> MT_ST_BLKSIZE_SHIFT;
		drive->density_code = (status.mt_dsreg & MT_ST_DENSITY_MASK) >> MT_ST_DENSITY_SHIFT;
	}
}

void sa_drive_setup(struct sa_drive * drive) {
	int fd = open(drive->device, O_RDWR | O_NONBLOCK);

	struct sa_drive_generic * self = malloc(sizeof(struct sa_drive_generic));
	self->fd_nst = fd;

	drive->ops = &sa_drive_generic_ops;
	drive->data = self;

	sa_drive_generic_update_status(drive);
}

