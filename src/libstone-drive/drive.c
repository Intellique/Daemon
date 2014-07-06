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

#include <stddef.h>

#include <libstone/slot.h>
#include <libstone/value.h>

#include "drive.h"

static struct st_drive_driver * current_drive = NULL;


struct st_value * stdr_drive_convert(struct st_drive * dr) {
	struct st_slot * sl = dr->slot;

	return st_value_pack("{sssssssbsisssfsisbsssssssss{sisssssb}}",
		"device", dr->device,
		"scsi device", dr->scsi_device,
		"status", st_drive_status_to_string(dr->status),
		"enable", dr->enable,

		"density code", dr->density_code,
		"mode", "unkown",
		"operation duration", dr->operation_duration,
		"last clean", dr->last_clean,
		"is empty", dr->is_empty,

		"model", dr->model,
		"vendor", dr->vendor,
		"revision", dr->revision,
		"serial number", dr->serial_number,

		"slot",
			"index", sl->index,
			"volume name", sl->volume_name,
			"type", st_slot_type_to_string(sl->type),
			"enable", sl->enable
	);
}

struct st_drive_driver * stdr_drive_get() {
	return current_drive;
}

__asm__(".symver stdr_drive_register_v1, stdr_drive_register@@LIBSTONE_DRIVE_1.2");
void stdr_drive_register_v1(struct st_drive_driver * dr) {
	current_drive = dr;
}

