/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
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
*  Last modified: Fri, 23 Dec 2011 22:48:03 +0100                         *
\*************************************************************************/

// open
#include <fcntl.h>
// glob
#include <glob.h>
// calloc, realloc
#include <stdlib.h>
// sscanf
#include <stdio.h>
// strcat, strchr, strcpy
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// time
#include <time.h>
// readlink
#include <unistd.h>

#include <stone/io.h>
#include <stone/log.h>

#include "common.h"
#include "scsi.h"

static struct st_changer * st_changers = 0;
static unsigned int st_nb_fake_changers = 0;
static unsigned int st_nb_real_changers = 0;


void st_changer_setup() {
	glob_t gl;
	gl.gl_offs = 0;
	glob("/sys/class/scsi_device/*/device/scsi_tape", GLOB_DOOFFS, 0, &gl);

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %zd drive%c", gl.gl_pathc, gl.gl_pathc != 1 ? 's' : '\0');

	struct st_drive * drives = calloc(gl.gl_pathc, sizeof(struct st_drive));
	unsigned int nb_drives = gl.gl_pathc;

	unsigned int i;
	for (i = 0; i < gl.gl_pathc; i++) {
		char * ptr = strrchr(gl.gl_pathv[i], '/');
		*ptr = '\0';

		char link[256];
		ssize_t length = readlink(gl.gl_pathv[i], link, 256);
		link[length] = '\0';

		ptr = strrchr(link, '/') + 1;
		int host = 0, target = 0, channel = 0, bus = 0;
		sscanf(ptr, "%d:%d:%d:%d", &host, &target, &channel, &bus);

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

		drives[i].id = -1;
		drives[i].device = strdup(device);
		drives[i].scsi_device = strdup(scsi_device);
		drives[i].status = ST_DRIVE_UNKNOWN;
		drives[i].model = 0;
		drives[i].vendor = 0;
		drives[i].revision = 0;
		drives[i].serial_number = 0;

		drives[i].host = host;
		drives[i].target = target;
		drives[i].channel = channel;
		drives[i].bus = bus;

		drives[i].changer = 0;
		drives[i].slot = 0;

		drives[i].data = 0;
		drives[i].file_position = 0;
		drives[i].nb_files = 0;
		drives[i].block_number = 0;

		drives[i].is_bottom_of_tape = 0;
		drives[i].is_end_of_file = 0;
		drives[i].is_writable = 0;
		drives[i].is_online = 0;
		drives[i].is_door_opened = 0;

		drives[i].block_size = 0;
		drives[i].density_code = 0;
		drives[i].operation_duration = 0;
		drives[i].last_clean = time(0);
	}
	globfree(&gl);

	gl.gl_offs = 0;
	glob("/sys/class/scsi_changer/*/device", GLOB_DOOFFS, 0, &gl);

	/**
	 * In the worst case, we have nb_drives changers,
	 * so we alloc enough memory for this worst case.
	 */
	st_changers = calloc(nb_drives, sizeof(struct st_changer));
	st_nb_real_changers = gl.gl_pathc;

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %zd changer%c", gl.gl_pathc, gl.gl_pathc != 1 ? 's' : '\0');

	for (i = 0; i < gl.gl_pathc; i++) {
		char link[256];
		ssize_t length = readlink(gl.gl_pathv[i], link, 256);
		link[length] = '\0';

		char * ptr = strrchr(link, '/') + 1;
		int host = 0, target = 0, channel = 0, bus = 0;
		sscanf(ptr, "%d:%d:%d:%d", &host, &target, &channel, &bus);

		char path[256];
		strcpy(path, gl.gl_pathv[i]);
		strcat(path, "/generic");
		length = readlink(path, link, 256);
		link[length] = '\0';

		char device[12];
		ptr = strrchr(link, '/');
		strcpy(device, "/dev");
		strcat(device, ptr);

		st_changers[i].id = -1;
		st_changers[i].device = strdup(device);
		st_changers[i].status = ST_CHANGER_UNKNOWN;
		st_changers[i].model = 0;
		st_changers[i].vendor = 0;
		st_changers[i].revision = 0;
		st_changers[i].serial_number = 0;
		st_changers[i].barcode = 0;

		st_changers[i].host = host;
		st_changers[i].target = target;
		st_changers[i].channel = channel;
		st_changers[i].bus = bus;

		st_changers[i].drives = 0;
		st_changers[i].nb_drives = 0;
		st_changers[i].slots = 0;
		st_changers[i].nb_slots = 0;

		st_changers[i].data = 0;
		st_changers[i].lock = 0;
		st_changers[i].transport_address = 0;
	}
	globfree(&gl);


	for (i = 0; i < st_nb_real_changers; i++) {
		unsigned j;
		for (j = 0; j < nb_drives; j++) {
			if (st_changers[i].host == drives[j].host && st_changers[i].target == drives[j].target) {
				drives[j].changer = st_changers + i;

				st_changers[i].drives = realloc(st_changers[i].drives, (st_changers[i].nb_drives + 1) * sizeof(struct st_drive));
				st_changers[i].drives[st_changers[i].nb_drives] = drives[j];
				st_changers[i].nb_drives++;
			}
		}
	}

	for (i = st_nb_real_changers; i < nb_drives; i++) {
		unsigned j;
		for (j = 0; j < nb_drives; j++) {
			if (!drives[j].changer) {
				drives[j].changer = st_changers + i;

				st_changers[i].drives = malloc(sizeof(struct st_drive));
				*st_changers[i].drives = drives[j];
				st_changers[i].nb_drives = 1;

				st_nb_fake_changers++;
			}
		}
	}

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %u stand-alone drive%c", st_nb_fake_changers, st_nb_fake_changers != 1 ? 's' : '\0');

	if (drives)
		free(drives);

	for (i = 0; i < st_nb_real_changers; i++)
		st_realchanger_setup(st_changers + i);

	for (i = st_nb_real_changers; i < st_nb_fake_changers + st_nb_real_changers; i++)
		st_fakechanger_setup(st_changers + i);
}

