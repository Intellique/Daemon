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

#define _GNU_SOURCE
// glob, globfree
#include <glob.h>
// ngettext
#include <libintl.h>
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

#include <libstoriqone/changer.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/file.h>
#include <libstoriqone/log.h>
#include <libstoriqone/value.h>

#include "hardware.h"
#include "scsi.h"

struct so_value * soctl_detect_hardware() {
	struct so_value * changers = so_value_new_linked_list();

	glob_t gl;
	glob("/sys/class/scsi_device/*/device/scsi_tape", 0, NULL, &gl);

	if (gl.gl_pathc == 0) {
		so_log_write2(so_log_level_notice, so_log_type_user_message, "There is drive found");
		return changers;
	}

	so_log_write2(so_log_level_info, so_log_type_user_message, "Library: Found %zd drive%s", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");
	printf(ngettext("storiqonectl: Found %zd drive\n", "storiqonectl: Found %zd drives\n", gl.gl_pathc), gl.gl_pathc);

	struct so_value * drives = so_value_new_hashtable2();

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

		// struct so_value * drive = so_value_pack("{sssssssssbsfsis{}}", "device", device, "scsi device", scsi_device, "status", "unknown", "mode", "linear", "enabled", true, "operation duration", 0.0, "last clean", 0, "db");
		struct so_drive * drive = malloc(sizeof(struct so_drive));
		bzero(drive, sizeof(struct so_drive));
		drive->status = so_drive_status_unknown;
		// drive->mode

		soctl_scsi_tapeinfo(scsi_device, drive);

		char dev[35];
		dev[34] = '\0';
		memset(dev, ' ', 35);
		strncpy(dev, drive->vendor, strlen(drive->vendor));
		dev[7] = ' ';
		strncpy(dev + 8, drive->model, strlen(drive->model));
		dev[23] = ' ';
		strcpy(dev + 24, drive->serial_number);

		so_value_hashtable_put2(drives, dev, so_value_new_custom(drive, so_drive_free2), true);
	}
	globfree(&gl);

	glob("/sys/class/scsi_changer/*/device", 0, NULL, &gl);

	so_log_write2(so_log_level_info, so_log_type_user_message, "Library: Found %zd changer%s", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");
	printf(ngettext("storiqonectl: Found %zd changer\n", "storiqonectl: Found %zd changers\n", gl.gl_pathc), gl.gl_pathc);

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

		struct so_changer * changer = malloc(sizeof(struct so_changer));
		bzero(changer, sizeof(struct so_changer));
		changer->status = so_changer_status_unknown;
		changer->enable = true;

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

				char * data = so_file_read_all_from(path);
				cp = strchr(data, '\n');
				if (cp != NULL)
					*cp = '\0';

				asprintf(&changer->wwn, "sas:%s", data);

				free(data);
			}
		}
		free(path);

		soctl_scsi_loaderinfo(device, changer, drives);
		free(device);

		so_value_list_push(changers, so_value_new_custom(changer, so_changer_free2), true);
	}
	globfree(&gl);

	so_value_free(drives);

	return changers;
}

