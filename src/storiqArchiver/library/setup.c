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
*  Last modified: Sun, 27 Nov 2011 18:56:01 +0100                         *
\*************************************************************************/

// open
#include <fcntl.h>
// glob
#include <glob.h>
// calloc, malloc, realloc
#include <stdlib.h>
// sscanf
#include <stdio.h>
// strcat, strchr, strcpy
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// readlink
#include <unistd.h>

#include <storiqArchiver/io.h>
#include <storiqArchiver/log.h>

#include "common.h"
#include "scsi.h"

static struct sa_changer * changers = 0;
static unsigned int nb_changers = 0;
static struct sa_drive * drives = 0;
static unsigned int nb_drives = 0;

static char * sa_changer_read(const char * filename);
static void sa_changer_setup_realchanger(struct sa_changer * changer);


char * sa_changer_read(const char * filename) {
	int fd = open(filename, O_RDONLY);

	struct stat st;
	fstat(fd, &st);

	char * data = malloc(128);
	ssize_t nbRead = read(fd, data, 128);
	close(fd);

	data[nbRead - 1] = '\0';
	data = realloc(data, nbRead);
	return data;
}

void sa_changer_setup() {
	glob_t gl;
	gl.gl_offs = 0;
	glob("/sys/class/scsi_changer/*/device", GLOB_DOOFFS, 0, &gl);

	changers = calloc(gl.gl_pathc, sizeof(struct sa_changer));
	nb_changers = gl.gl_pathc;

	sa_log_write_all(sa_log_level_info, "Library: Found %zd librar%s", gl.gl_pathc, gl.gl_pathc != 1 ? "ies" : "y");

	unsigned int i;
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

		changers[i].id = -1;
		changers[i].device = strdup(device);
		changers[i].status = sa_changer_unknown;

		changers[i].barcode = 0;
		changers[i].host = host;
		changers[i].target = target;
		changers[i].channel = channel;
		changers[i].bus = bus;

		changers[i].drives = 0;
		changers[i].nb_drives = 0;
		changers[i].slots = 0;
		changers[i].nb_slots = 0;
	}
	globfree(&gl);

	gl.gl_offs = 0;
	glob("/sys/class/scsi_device/*/device/scsi_tape", GLOB_DOOFFS, 0, &gl);

	sa_log_write_all(sa_log_level_info, "Library: Found %zd drive%c", gl.gl_pathc, gl.gl_pathc != 1 ? 's' : '\0');

	drives = calloc(gl.gl_pathc, sizeof(struct sa_drive));
	nb_drives = gl.gl_pathc;

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

		char scsiDevice[12];
		ptr = strrchr(link, '/');
		strcpy(scsiDevice, "/dev");
		strcat(scsiDevice, ptr);

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
		drives[i].scsiDevice = strdup(scsiDevice);
		drives[i].status = sa_drive_unknown;

		strcpy(path, gl.gl_pathv[i]);
		strcat(path, "/model");
		drives[i].model = sa_changer_read(path);

		strcpy(path, gl.gl_pathv[i]);
		strcat(path, "/vendor");
		drives[i].vendor = sa_changer_read(path);

		drives[i].host = host;
		drives[i].target = target;
		drives[i].channel = channel;
		drives[i].bus = bus;

		drives[i].changer = 0;
		drives[i].slot = 0;
	}
	globfree(&gl);

	for (i = 0; i < nb_changers; i++) {
		unsigned j;
		for (j = 0; j < nb_drives; j++) {
			if (changers[i].host == drives[j].host && changers[i].target == drives[j].target) {
				drives[j].changer = changers + i;

				changers[i].drives = realloc(changers[i].drives, (changers[i].nb_drives + 1) * sizeof(struct sa_drive));
				changers[i].drives[changers[i].nb_drives] = drives[j];
				changers[i].nb_drives++;
			}
		}
	}

	if (nb_changers > 0)
		sa_changer_setup_realchanger(changers);
}

void sa_changer_setup_realchanger(struct sa_changer * changer) {
	int fd = open(changer->device, O_RDWR);

	sa_scsi_loaderinfo(fd, changer);
	sa_scsi_mtx_status_new(fd, changer);

	sa_realchanger_setup(changer, fd);
}

