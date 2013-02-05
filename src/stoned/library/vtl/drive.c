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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Tue, 05 Feb 2013 10:17:16 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// asprintf
#include <stdio.h>
// malloc
#include <stdlib.h>
// strdup
#include <string.h>
// access
#include <unistd.h>

#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/util/file.h>

#include "common.h"

static bool st_vtl_drive_check_media(struct st_vtl_drive * dr);


static bool st_vtl_drive_check_media(struct st_vtl_drive * dr) {
	return !access(dr->media_path, F_OK);
}

void st_vtl_drive_init(struct st_drive * drive, struct st_slot * slot, const char * base_dir, struct st_media_format * format) {
	char * serial_file;
	asprintf(&serial_file, "%s/serial_number", base_dir);

	char * serial = st_util_file_get_serial(serial_file);
	free(serial_file);

	struct st_vtl_drive * self = malloc(sizeof(struct st_vtl_drive));
	self->path = base_dir;
	asprintf(&self->media_path, "%s/media", base_dir);

	bool has_media = st_vtl_drive_check_media(self);

	drive->device = NULL;
	drive->scsi_device = NULL;
	drive->status = has_media ? st_drive_loaded_idle : st_drive_empty_idle;
	drive->enabled = true;

	drive->density_code = format->density_code;
	drive->mode = st_media_format_mode_linear;
	drive->operation_duration = 0;
	drive->last_clean = 0;
	drive->is_empty = !has_media;

	drive->model = strdup("Stone vtl drive");
	drive->vendor = strdup("Intellique");
	drive->revision = strdup("A00");
	drive->serial_number = serial;

	drive->changer = slot->changer;
	drive->slot = slot;

	drive->data = self;
	drive->db_data = NULL;

	drive->lock = st_ressource_new();
}

