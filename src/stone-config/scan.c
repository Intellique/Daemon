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
*  Last modified: Tue, 10 Jun 2014 19:12:07 +0200                            *
\****************************************************************************/

#define _GNU_SOURCE
// glob, globfree
#include <glob.h>
// realpath
#include <limits.h>
// printf, sscanf, snprintf
#include <stdio.h>
// calloc, free, realloc, realpath
#include <stdlib.h>
// strcat, strchr, strcpy, strstr
#include <string.h>
// stat
#include <sys/stat.h>
// stat
#include <sys/types.h>
// readlink, stat
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/log.h>
#include <libstone/util/file.h>

#include "scan.h"
#include "scsi.h"

int stcfg_scan(void) {
	glob_t gl;
	glob("/sys/class/scsi_device/*/device/scsi_tape", 0, NULL, &gl);

	if (gl.gl_pathc == 0) {
		st_log_write_all(st_log_level_error, st_log_type_user_message, "Panic: There is drive found, exit now !!!");
		return 1;
	}

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %zd drive%s", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");
	printf("Library: Found %zd drive%s\n", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");

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
		drives[i].status = st_drive_unknown;
		drives[i].enabled = true;

		drives[i].model = NULL;
		drives[i].vendor = NULL;
		drives[i].revision = NULL;
		drives[i].serial_number = NULL;

		drives[i].changer = NULL;
		drives[i].slot = NULL;

		drives[i].data = NULL;
		drives[i].db_data = NULL;
	}
	globfree(&gl);

	glob("/sys/class/scsi_changer/*/device", 0, NULL, &gl);

	/**
	 * In the worst case, we have nb_drives changers,
	 * so we alloc enough memory for this worst case.
	 */
	struct st_changer * changers = calloc(nb_drives, sizeof(struct st_changer));
	unsigned int nb_real_changers = gl.gl_pathc;

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %zd changer%s", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");
	printf("Library: Found %zd changer%s\n", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");

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
		changers[i].status = st_changer_unknown;
		changers[i].enabled = true;

		changers[i].model = NULL;
		changers[i].vendor = NULL;
		changers[i].revision = NULL;
		changers[i].serial_number = NULL;
		changers[i].wwn = NULL;
		changers[i].barcode = false;

		snprintf(path, sizeof(path), "/sys/class/sas_host/host%d", host);
		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			realpath(gl.gl_pathv[i], link);

			ptr = strstr(link, "end_device-");
			if (ptr != NULL) {
				char * cp = strchr(ptr, '/');
				if (cp != NULL)
					*cp = '\0';

				snprintf(path, sizeof(path), "/sys/class/sas_device/%s/sas_address", ptr);

				char * data = st_util_file_read_all_from(path);
				cp = strchr(data, '\n');
				if (cp != NULL)
					*cp = '\0';

				asprintf(&changers[i].wwn, "sas:%s", data);
				free(data);
			}
		}

		changers[i].drives = NULL;
		changers[i].nb_drives = 0;
		changers[i].slots = NULL;
		changers[i].nb_slots = 0;

		changers[i].data = NULL;
		changers[i].db_data = NULL;
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
			if (drives[j].changer == NULL && stcfg_scsi_drive_in_changer(drives + j, changers + i)) {
				drives[j].changer = changers + i;

				changers[i].drives = realloc(changers[i].drives, (changers[i].nb_drives + 1) * sizeof(struct st_drive));
				changers[i].drives[changers[i].nb_drives] = drives[j];
				changers[i].nb_drives++;
			}
		}

		if (changers[i].nb_drives == 0)
			nb_changer_without_drive++;
		else {
			struct st_changer * ch = changers + i;
			stcfg_scsi_loader_status_new(ch->device, ch);
		}
	}

	// link stand-alone drive to fake changer
	unsigned int nb_fake_changers = 0;
	for (i = nb_real_changers; i < nb_drives; i++) {
		unsigned j;
		for (j = 0; j < nb_drives; j++) {
			if (drives[j].changer == NULL) {
				drives[j].changer = changers + i;

				changers[i].device = strdup("");
				changers[i].status = st_changer_unknown;
				changers[i].model = strdup(drives[j].model);
				changers[i].vendor = strdup(drives[j].vendor);
				changers[i].revision = strdup(drives[j].revision);
				changers[i].serial_number = strdup(drives[j].serial_number);
				changers[i].barcode = false;

				changers[i].drives = malloc(sizeof(struct st_drive));
				*changers[i].drives = drives[j];
				changers[i].nb_drives = 1;

				nb_fake_changers++;
			}
		}
	}

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %u stand-alone drive%s", nb_fake_changers, nb_fake_changers != 1 ? "s" : "");
	printf("Library: Found %u stand-alone drive%s\n", nb_fake_changers, nb_fake_changers != 1 ? "s" : "");

	if (drives)
		free(drives);

	struct st_database * db = st_database_get_default_driver();
	if (db == NULL) {
		printf("Warning, there is no database driver loaded\n");
		return 1;
	}

	struct st_database_config * config = db->ops->get_default_config();
	if (config == NULL) {
		printf("Warning, there is no database configured\n");
		return 1;
	}

	struct st_database_connection * connect = config->ops->connect(config);
	if (connect != NULL) {
		printf("Synchronization with database\n");

		int failed = 0;
		for (i = 0; i < nb_real_changers + nb_fake_changers; i++)
			failed = connect->ops->sync_changer(connect, changers + i, false);

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

