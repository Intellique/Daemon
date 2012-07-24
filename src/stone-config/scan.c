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
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Tue, 24 Jul 2012 23:23:20 +0200                         *
\*************************************************************************/

// glob, globfree
#include <glob.h>
// printf, sscanf
#include <stdio.h>
// calloc, realloc
#include <stdlib.h>
// strcat, strchr, strcpy
#include <string.h>
// readlink
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/library/drive.h>
#include <libstone/log.h>
#include <stoned/library/changer.h>

#include "scan.h"
#include "scsi.h"

int stcfg_scan() {
	glob_t gl;
	gl.gl_offs = 0;
	glob("/sys/class/scsi_device/*/device/scsi_tape", GLOB_DOOFFS, 0, &gl);

	if (gl.gl_pathc == 0) {
		st_log_write_all(st_log_level_error, st_log_type_user_message, "Panic: There is drive found, exit now !!!");
		return 1;
	}

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %zd drive%s", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");
    printf("Library: Found %zd drive%s", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");

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
	}
	globfree(&gl);

	gl.gl_offs = 0;
	glob("/sys/class/scsi_changer/*/device", GLOB_DOOFFS, 0, &gl);

	/**
	 * In the worst case, we have nb_drives changers,
	 * so we alloc enough memory for this worst case.
	 */
	struct st_changer * changers = calloc(nb_drives, sizeof(struct st_changer));
	unsigned int nb_real_changers = gl.gl_pathc;

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %zd changer%s", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");
	printf("Library: Found %zd changer%s", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");

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

		changers[i].device = strdup(device);
		changers[i].status = ST_CHANGER_UNKNOWN;
		changers[i].model = 0;
		changers[i].vendor = 0;
		changers[i].revision = 0;
		changers[i].serial_number = 0;
		changers[i].barcode = 0;

		changers[i].host = host;
		changers[i].target = target;
		changers[i].channel = channel;
		changers[i].bus = bus;

		changers[i].drives = 0;
		changers[i].nb_drives = 0;
		changers[i].slots = 0;
		changers[i].nb_slots = 0;

		changers[i].data = 0;
	}
	globfree(&gl);

	// do loaderinfo
	for (i = 0; i < nb_real_changers; i++)
		stcfg_scsi_loaderinfo(changers[i].device, changers + i);

	// do tapeinfo
	for (i = 0; i < nb_drives; i++)
		stcfg_scsi_tapeinfo(drives[i].scsi_device, drives + i);

	// link drive to real changer
	unsigned int nb_changer_without_drive = 0;
	for (i = 0; i < nb_real_changers; i++) {
		unsigned j;
		for (j = 0; j < nb_drives; j++) {
			if (changers[i].host == drives[j].host && changers[i].target == drives[j].target && changers[i].channel == drives[j].channel) {
				drives[j].changer = changers + i;

				changers[i].drives = realloc(changers[i].drives, (changers[i].nb_drives + 1) * sizeof(struct st_drive));
				changers[i].drives[changers[i].nb_drives] = drives[j];
				changers[i].nb_drives++;
			}
		}

		if (changers[i].nb_drives == 0)
			nb_changer_without_drive++;
	}

	// link stand-alone drive to fake changer
	unsigned int nb_fake_changers = 0;
	for (i = nb_real_changers; i < nb_drives; i++) {
		unsigned j;
		for (j = 0; j < nb_drives; j++) {
			if (!drives[j].changer) {
				drives[j].changer = changers + i;

				changers[i].device = strdup("");
				changers[i].status = ST_CHANGER_UNKNOWN;
				changers[i].model = strdup(drives[j].model);
				changers[i].vendor = strdup(drives[j].vendor);
				changers[i].revision = strdup(drives[j].revision);
				changers[i].serial_number = strdup(drives[j].serial_number);
				changers[i].barcode = 0;

				changers[i].drives = malloc(sizeof(struct st_drive));
				*changers[i].drives = drives[j];
				changers[i].nb_drives = 1;

				nb_fake_changers++;
			}
		}
	}

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %u stand-alone drive%s", nb_fake_changers, nb_fake_changers != 1 ? "s" : "");
	printf("Library: Found %u stand-alone drive%s", nb_fake_changers, nb_fake_changers != 1 ? "s" : "");

	if (drives)
		free(drives);

	struct st_database * db = st_database_get_default_driver();
	if (!db) {
		printf("Warning, there is no database driver loaded\n");
		return 1;
	}

	struct st_database_config * config = db->ops->get_default_config();
	if (!config) {
		printf("Warning, there is no database configured\n");
		return 1;
	}

	struct st_database_connection * connect = config->ops->connect(config);
	if (connect) {
		printf("Synchronization with database\n");

		int failed = 0;
		//for (i = 0; i < nb_real_changers + nb_fake_changers; i++)
		//	failed = connect->ops->sync_changer(connect, changers + i);

		connect->ops->close(connect);
		connect->ops->free(connect);

		if (failed)
			printf("Synchronization failed with status = %d\n", failed);
		else
			printf("Synchronization finished with status = OK\n");
	} else {
		printf("Error: Failed to connect to database\n");
	}

	return 0;
}

