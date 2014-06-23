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
// bool
#include <stdbool.h>
// printf, sscanf, snprintf
#include <stdio.h>
// free
#include <stdlib.h>
// strrchr
#include <string.h>
// open, size_t
#include <sys/types.h>
// close
#include <unistd.h>

#include <libstone/media.h>
#include <libstone/value.h>
#include <libstone-drive/drive.h>

#include "device.h"
#include "scsi.h"

static int tape_drive_init(struct st_value * config);
static int tape_drive_update_status(void);

static struct st_drive_ops tape_drive_ops = {
	.init          = tape_drive_init,
	.update_status = tape_drive_update_status,
};

static struct st_drive tape_drive = {
	.device      = NULL,
	.scsi_device = NULL,
	.status      = st_drive_unknown,
	.enabled     = true,

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


struct st_drive * tapedrive_get_device() {
	return &tape_drive;
}

static int tape_drive_init(struct st_value * config) {
	st_value_unpack(config, "{sssssssb}", "model", &tape_drive.model, "vendor", &tape_drive.vendor, "serial number", &tape_drive.serial_number, "enable", &tape_drive.enabled);

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

		found = tapedrive_scsi_check_drive(&tape_drive, scsi_device);
		if (found) {
			tape_drive.device = strdup(device);
			tape_drive.scsi_device = strdup(scsi_device);
		}
	}
	globfree(&gl);

	return found ? 0 : 1;
}

static int tape_drive_update_status() {
	return 0;
}

