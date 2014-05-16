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
// printf, sscanf, snprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// memset, strcat, strchr, strcpy, strdup, strncpy, strstr
#include <string.h>
// bzero
#include <strings.h>
// stat
#include <sys/stat.h>
// stat
#include <sys/types.h>
// readlink, stat
#include <unistd.h>

#include <libstone/changer.h>
#include <libstone/drive.h>
#include <libstone/file.h>
#include <libstone/log.h>
#include <libstone/value.h>

#include "hardware.h"
#include "scsi.h"

struct st_value * stctl_detect_hardware() {
	struct st_value * changers = st_value_new_linked_list();

	glob_t gl;
	glob("/sys/class/scsi_device/*/device/scsi_tape", 0, NULL, &gl);

	if (gl.gl_pathc == 0) {
		st_log_write2(st_log_level_notice, st_log_type_user_message, "There is drive found");
		return changers;
	}

	st_log_write2(st_log_level_info, st_log_type_user_message, "Library: Found %zd drive%s", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");
	printf("Library: Found %zd drive%s\n", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");

	struct st_value * drives = st_value_new_hashtable2();

	unsigned int i;
	for (i = 0; i < gl.gl_pathc; i++) {
		char * ptr = strrchr(gl.gl_pathv[i], '/');
		*ptr = '\0';

		char link[256];
		ssize_t length = readlink(gl.gl_pathv[i], link, 256);
		link[length] = '\0';

		ptr = strrchr(link, '/') + 1;

		char * path;
		asprintf(&path, "%s/generic", gl.gl_pathv[i]);
		length = readlink(path, link, 256);
		link[length] = '\0';
		free(path);

		char * scsi_device;
		ptr = strrchr(link, '/');
		asprintf(&scsi_device, "/dev%s", ptr);

		asprintf(&path, "%s/tape", gl.gl_pathv[i]);
		length = readlink(path, link, 256);
		link[length] = '\0';
		free(path);

		char * device;
		ptr = strrchr(link, '/') + 1;
		asprintf(&device, "/dev/n%s", ptr);

		// struct st_value * drive = st_value_pack("{sssssssssbsfsis{}}", "device", device, "scsi device", scsi_device, "status", "unknown", "mode", "linear", "enabled", true, "operation duration", 0.0, "last clean", 0, "db");
		struct st_drive * drive = malloc(sizeof(struct st_drive));
		bzero(drive, sizeof(struct st_drive));
		drive->device = device;
		drive->scsi_device = scsi_device;
		drive->status = st_drive_unknown;
		// drive->mode

		stctl_scsi_tapeinfo(scsi_device, drive);

		char dev[35];
		dev[34] = '\0';
		memset(dev, ' ', 35);
		strncpy(dev, drive->vendor, strlen(drive->vendor));
		dev[7] = ' ';
		strncpy(dev + 8, drive->model, strlen(drive->model));
		dev[23] = ' ';
		strcpy(dev + 24, drive->serial_number);

		st_value_hashtable_put2(drives, dev, st_value_new_custom(drive, st_drive_free2), true);
	}
	globfree(&gl);

	glob("/sys/class/scsi_changer/*/device", 0, NULL, &gl);

	st_log_write2(st_log_level_info, st_log_type_user_message, "Library: Found %zd changer%s", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");
	printf("Library: Found %zd changer%s\n", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");

	for (i = 0; i < gl.gl_pathc; i++) {
		char link[256];
		ssize_t length = readlink(gl.gl_pathv[i], link, 256);
		link[length] = '\0';

		char * ptr = strrchr(link, '/') + 1;
		int host = 0, target = 0, channel = 0, bus = 0;
		sscanf(ptr, "%d:%d:%d:%d", &host, &target, &channel, &bus);

		char * path;
		asprintf(&path, "%s/generic", gl.gl_pathv[i]);
		length = readlink(path, link, 256);
		link[length] = '\0';
		free(path);

		char * device;
		ptr = strrchr(link, '/');
		asprintf(&device, "/dev%s", ptr);

		struct st_changer * changer = malloc(sizeof(struct st_changer));
		bzero(changer, sizeof(struct st_changer));
		changer->device = strdup(device);
		changer->status = st_changer_unknown;
		changer->enabled = true;

		asprintf(&path, "/sys/class/sas_host/host%d", host);
		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			realpath(gl.gl_pathv[i], link);

			ptr = strstr(link, "end_device-");
			if (ptr != NULL) {
				char * cp = strchr(ptr, '/');
				if (cp != NULL)
					*cp = '\0';

				free(path);
				asprintf(&path, "/sys/class/sas_device/%s/sas_address", ptr);

				char * data = st_file_read_all_from(path);
				cp = strchr(data, '\n');
				if (cp != NULL)
					*cp = '\0';

				asprintf(&changer->wwn, "sas:%s", data);

				free(data);
			}
		}
		free(path);

		stctl_scsi_loaderinfo(device, changer, drives);
		free(device);

		st_value_list_push(changers, st_value_new_custom(changer, st_changer_free2), true);
	}
	globfree(&gl);

	st_value_free(drives);

	return changers;
}

