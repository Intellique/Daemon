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

#include <libstoriqone/file.h>
#include <libstoriqone/value.h>
#include <libstoriqone-changer/drive.h>

#include "device.h"
#include "util.h"

void sochgr_vtl_drive_create(struct sochgr_vtl_drive * dr_p, struct so_drive * dr, const char * root_directory, long long index) {
	asprintf(&dr_p->drive_dir, "%s/drives/%Ld", root_directory, index);
	so_file_mkdir(dr_p->drive_dir, 0700);

	char * serial_file;
	asprintf(&serial_file, "%s/drives/%Ld/serial_number", root_directory, index);

	dr->model = strdup("Storiq one vtl drive");
	dr->vendor = strdup("Intellique");
	dr->revision = strdup("B01");
	dr->serial_number = sochgr_vtl_util_get_serial(serial_file);

	dr->status = so_drive_status_unknown;
	dr->enable = true;
	dr->mode = so_media_format_mode_disk;
	dr->index = index;

	so_value_hashtable_put2(dr_p->params, "device", so_value_new_string(dr_p->drive_dir), true);

	free(serial_file);
}

void sochgr_vtl_drive_delete(struct so_drive * drive) {
	free(drive->model);
	free(drive->vendor);
	free(drive->revision);
	free(drive->serial_number);

	so_value_free(drive->db_data);

	drive->ops->free(drive);
}

