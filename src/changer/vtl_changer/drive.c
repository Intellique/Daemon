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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// asprintf
#include <stdio.h>
// free
#include <stdlib.h>
// strdup
#include <string.h>

#include <libstoriqone-changer/drive.h>

#include "device.h"
#include "util.h"

void sochgr_vtl_drive_create(struct so_drive * drive, const char * root_directory, long long index) {
	char * serial_file;
	asprintf(&serial_file, "%s/drives/%Ld/serial_number", root_directory, index);

	drive->model = strdup("Storiq one vtl drive");
	drive->vendor = strdup("Intellique");
	drive->revision = strdup("B01");
	drive->serial_number = sochgr_vtl_util_get_serial(serial_file);

	drive->status = so_drive_status_unknown;
	drive->enable = true;
	drive->mode = so_media_format_mode_disk;
	drive->index = index;

	free(serial_file);
}

void sochgr_vtl_drive_delete(struct so_drive * drive) {
	free(drive->model);
	free(drive->vendor);
	free(drive->revision);
	free(drive->serial_number);

	drive->ops->free(drive);
}

