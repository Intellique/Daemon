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

#define _GNU_SOURCE
// glob, globfree
#include <glob.h>
// open
#include <fcntl.h>
// bool
#include <stdbool.h>
// printf, sscanf, snprintf
#include <stdio.h>
// free
#include <stdlib.h>
// strrchr
#include <string.h>
// struct mtget
#include <sys/mtio.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// open, size_t
#include <sys/types.h>
// time
#include <time.h>
// close
#include <unistd.h>

#include <libstone/log.h>
#include <libstone/media.h>
#include <libstone/slot.h>
#include <libstone/value.h>
#include <libstone-drive/drive.h>

#include "device.h"
#include "scsi.h"

static int tape_drive_init(struct st_value * config);
static void tape_drive_on_failed(bool verbose, unsigned int sleep_time);
static void tape_drive_operation_start(void);
static void tape_drive_operation_stop(void);
static int tape_drive_update_status(void);

static int fd_nst = -1;
static struct mtget status;
static struct timespec last_start;


static struct st_drive_ops tape_drive_ops = {
	.init          = tape_drive_init,
	.update_status = tape_drive_update_status,
};

static struct st_drive tape_drive = {
	.device      = NULL,
	.scsi_device = NULL,
	.status      = st_drive_unknown,
	.enable      = true,

	.density_code       = 0,
	.mode               = st_media_format_mode_linear,
	.operation_duration = 0,
	.is_empty           = true,

	.model         = NULL,
	.vendor        = NULL,
	.revision      = NULL,
	.serial_number = NULL,

	.changer = NULL,
	.slot    = NULL,

	.ops = &tape_drive_ops,

	.data    = NULL,
	.db_data = NULL,
};


struct st_drive * tape_drive_get_device() {
	return &tape_drive;
}

static int tape_drive_init(struct st_value * config) {
	struct st_slot * sl = tape_drive.slot = malloc(sizeof(struct st_slot));
	bzero(tape_drive.slot, sizeof(struct st_slot));
	sl->drive = &tape_drive;

	char * type = NULL;
	st_value_unpack(config, "{sssssssbs{si}}", "model", &tape_drive.model, "vendor", &tape_drive.vendor, "serial number", &tape_drive.serial_number, "enable", &tape_drive.enable, "slot", "index", &sl->index, "type", type);
	sl->type = st_slot_string_to_type(type);
	free(type);

	glob_t gl;
	glob("/sys/class/scsi_device/*/device/scsi_tape", 0, NULL, &gl);

	unsigned int i;
	bool found = false;
	for (i = 0; i < gl.gl_pathc && !found; i++) {
		char * ptr = strrchr(gl.gl_pathv[i], '/');
		*ptr = '\0';

		char link[256];
		ssize_t length = readlink(gl.gl_pathv[i], link, 256);
		link[length] = '\0';

		ptr = strrchr(link, '/') + 1;

		char path[256];
		strcpy(path, gl.gl_pathv[i]);
		strcat(path, "/generic");
		length = readlink(path, link, 256);
		link[length] = '\0';

		char scsi_device[12];
		ptr = strrchr(link, '/');
		strcpy(scsi_device, "/dev");
		strcat(scsi_device, ptr);

		strcpy(path, gl.gl_pathv[i]);
		strcat(path, "/tape");
		length = readlink(path, link, 256);
		link[length] = '\0';

		char device[12];
		ptr = strrchr(link, '/') + 1;
		strcpy(device, "/dev/n");
		strcat(device, ptr);

		found = tape_drive_scsi_check_drive(&tape_drive, scsi_device);
		if (found) {
			tape_drive.device = strdup(device);
			tape_drive.scsi_device = strdup(scsi_device);
		}
	}
	globfree(&gl);

	if (found) {
		fd_nst = open(tape_drive.device, O_RDWR | O_NONBLOCK);
		tape_drive_operation_start();
		int failed = ioctl(fd_nst, MTIOCGET, &status);
		tape_drive_operation_stop();

		if (failed != 0) {
			if (GMT_ONLINE(status.mt_gstat)) {
				tape_drive.status = st_drive_loaded_idle;
				tape_drive.is_empty = false;
			} else {
				tape_drive.status = st_drive_empty_idle;
				tape_drive.is_empty = true;
			}
		}
	}

	return found ? 0 : 1;
}

static void tape_drive_on_failed(bool verbose, unsigned int sleep_time) {
	// if (verbose)
	//	st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: Try to recover an error", drive->vendor, drive->model, drive - drive->changer->drives);

	close(fd_nst);
	sleep(sleep_time);
	fd_nst = open(tape_drive.device, O_RDWR | O_NONBLOCK);
}

static void tape_drive_operation_start() {
	clock_gettime(CLOCK_MONOTONIC, &last_start);
}

static void tape_drive_operation_stop() {
	struct timespec finish;
	clock_gettime(CLOCK_MONOTONIC, &finish);

	tape_drive.operation_duration += (finish.tv_sec - last_start.tv_sec) + (finish.tv_nsec - last_start.tv_nsec) / 1000000000;
}

static int tape_drive_update_status() {
	tape_drive_operation_start();
	int failed = ioctl(fd_nst, MTIOCGET, &status);
	tape_drive_operation_stop();

	unsigned int i;
	for (i = 0; i < 5 && failed != 0; i++) {
		tape_drive_on_failed(true, 20);

		tape_drive_operation_start();
		failed = ioctl(fd_nst, MTIOCGET, &status);
		tape_drive_operation_stop();
	}

	if (failed != 0) {
		tape_drive.status = st_drive_error;

		static struct mtop reset = { MTRESET, 1 };
		tape_drive_operation_start();
		failed = ioctl(fd_nst, MTIOCTOP, &reset);
		tape_drive_operation_stop();
	}

	/**
	 * Make sure that we have the correct status of tape
	 */
	while (!failed && GMT_ONLINE(status.mt_gstat)) {
		unsigned int density_code = ((status.mt_dsreg & MT_ST_DENSITY_MASK) >> MT_ST_DENSITY_SHIFT) & 0xFF;

		if (density_code != 0)
			break;

		// get the tape status again
		tape_drive_on_failed(true, 5);

		tape_drive_operation_start();
		failed = ioctl(fd_nst, MTIOCGET, &status);
		tape_drive_operation_stop();
	}

	if (failed == 0) {
		bool is_empty = tape_drive.is_empty;

		if (GMT_ONLINE(status.mt_gstat)) {
			tape_drive.status = st_drive_loaded_idle;
			tape_drive.is_empty = false;

			unsigned int density_code = ((status.mt_dsreg & MT_ST_DENSITY_MASK) >> MT_ST_DENSITY_SHIFT) & 0xFF;
			if (tape_drive.density_code < density_code)
				tape_drive.density_code = density_code & 0xFF;
		} else {
			tape_drive.status = st_drive_empty_idle;
			tape_drive.is_empty = true;
		}

		if (tape_drive.slot->media == NULL && !tape_drive.is_empty) {
			char medium_serial_number[32];
			int fd = open(tape_drive.scsi_device, O_RDWR);

			// failed = st_scsi_tape_read_medium_serial_number(fd, medium_serial_number, 32);

			close(fd);

			// if (failed == 0)
			// tape_drive.slot->media = st_media_get_by_medium_serial_number(medium_serial_number);

			// if (drive->slot->media == NULL)
			//	st_scsi_tape_drive_create_media(drive);
		}

		if (tape_drive.slot->media != NULL && !tape_drive.is_empty) {
			struct st_media * media = tape_drive.slot->media;

			int fd = open(tape_drive.scsi_device, O_RDWR);
			// st_scsi_tape_size_available(fd, media);
			close(fd);

			media->type = GMT_WR_PROT(status.mt_gstat) ? st_media_type_readonly : st_media_type_rewritable;

			if (is_empty && media->status == st_media_status_in_use) {
				/*
				st_log_write(st_log_level_info, "[%s | %s | #%td]: Checking media header", tape_drive.vendor, tape_drive.model, drive - tape_drive.changer->drives);
				if (!st_media_check_header(drive)) {
					media->status = st_media_status_error;
					st_log_write_all(st_log_level_error, st_log_type_drive, "[%s | %s | #%td]: Error while checking media header", drive->vendor, drive->model, drive - drive->changer->drives);
					self->allow_update_media = true;
					return 1;
				} */
			}
		}
	} else {
		tape_drive.status = st_drive_error;
	}

	return failed;
}

