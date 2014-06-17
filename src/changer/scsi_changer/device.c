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
// stat
#include <sys/stat.h>
// stat
#include <sys/types.h>
// readlink, stat
#include <unistd.h>

#include <libstone/file.h>
#include <libstone/value.h>
#include <libstone-changer/changer.h>

#include "device.h"
#include "scsi.h"

static int scsi_changer_init(struct st_value * config);

struct st_changer_ops scsi_changer_ops = {
	.init = scsi_changer_init,
};

static struct st_changer scsi_changer = {
	.device  = NULL,
	.status  = st_changer_unknown,
	.enabled = true,

	.model         = NULL,
	.vendor        = NULL,
	.revision      = NULL,
	.serial_number = NULL,
	.wwn           = NULL,
	.barcode       = false,

	.drives    = NULL,
	.nb_drives = 0,

	.slots    = NULL,
	.nb_slots = 0,

	.ops = &scsi_changer_ops,

	.data = NULL,
	.db_data = NULL,
};


struct st_changer * scsichanger_get_device() {
	return &scsi_changer;
}

static int scsi_changer_init(struct st_value * config) {
	st_value_unpack(config, "{sssssssbsbss}", "model", &scsi_changer.model, "vendor", &scsi_changer.vendor, "serial number", &scsi_changer.serial_number, "enable", &scsi_changer.enabled, "is online", &scsi_changer.is_online, "wwn", &scsi_changer.wwn);

	glob_t gl;
	glob("/sys/class/scsi_changer/*/device", 0, NULL, &gl);

	unsigned int i;
	bool found = false, has_wwn = scsi_changer.wwn != NULL;
	for (i = 0; i < gl.gl_pathc && !found; i++) {
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

				char * wwn;
				asprintf(&wwn, "sas:%s", data);

				if (has_wwn && !strcmp(wwn, scsi_changer.wwn))
					found = true;
				else if (!has_wwn) {
					free(scsi_changer.wwn);
					scsi_changer.wwn = wwn;
				}

				if (has_wwn)
					free(wwn);
				free(data);
			}
		}
		free(path);

		if (!found)
			found = scsichanger_scsi_check_device(&scsi_changer, device);

		if (found) {
			scsi_changer.device = device;
		} else {
			free(device);
		}
	}
	globfree(&gl);

	struct st_value * available_drives = st_value_new_hashtable2();
	struct st_value * drives = NULL;
	st_value_unpack(config, "{so}", "drives", &drives);

	if (drives != NULL) {
		struct st_value_iterator * iter = st_value_list_get_iterator(drives);
		while (st_value_iterator_has_next(iter)) {
			struct st_value * dr = st_value_iterator_get_value(iter, true);

			char * vendor = NULL, * model = NULL, * serial_number = NULL;
			st_value_unpack(dr, "{ssssss}", "vendor", &vendor, "model", &model, "serial number", &serial_number);

			char dev[35];
			memset(dev, ' ', 34);
			dev[34] = '\0';

			strncpy(dev, vendor, strlen(vendor));
			dev[7] = ' ';
			strncpy(dev + 8, model, strlen(model));
			dev[23] = ' ';
			strcpy(dev + 23, serial_number);

			st_value_hashtable_put2(available_drives, dev, dr, false);

			free(vendor);
			free(model);
			free(serial_number);
		}
		st_value_iterator_free(iter);
	}

	if (found && scsi_changer.enabled && scsi_changer.is_online)
		scsichanger_scsi_new_status(&scsi_changer, available_drives);

	st_value_free(available_drives);

	return found ? 0 : 1;
}

