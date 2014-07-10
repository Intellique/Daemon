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

// free
#include <stdlib.h>
// strcasecmp
#include <string.h>

#include "drive.h"
#include "media.h"
#include "slot.h"
#include "value.h"

static const struct st_drive_status2 {
	const char * name;
	enum st_drive_status status;
} st_drive_status[] = {
	{ "cleaning",		st_drive_cleaning },
	{ "empty idle",		st_drive_empty_idle },
	{ "erasing",		st_drive_erasing },
	{ "error",			st_drive_error },
	{ "loaded idle",	st_drive_loaded_idle },
	{ "loading",		st_drive_loading },
	{ "positioning",	st_drive_positioning },
	{ "reading",		st_drive_reading },
	{ "rewinding",		st_drive_rewinding },
	{ "unloading",		st_drive_unloading },
	{ "writing",		st_drive_writing },

	{ "unknown", st_drive_unknown },
};


__asm__(".symver st_drive_free_v1, st_drive_free@@LIBSTONE_1.2");
void st_drive_free_v1(struct st_drive * drive) {
	if (drive == NULL)
		return;

	free(drive->device);
	free(drive->scsi_device);
	free(drive->model);
	free(drive->vendor);
	free(drive->revision);
	free(drive->serial_number);

	free(drive->data);
}

__asm__(".symver st_drive_free2_v1, st_drive_free2@@LIBSTONE_1.2");
void st_drive_free2_v1(void * drive) {
	st_drive_free_v1(drive);
}

__asm__(".symver st_drive_status_to_string_v1, st_drive_status_to_string@@LIBSTONE_1.2");
const char * st_drive_status_to_string_v1(enum st_drive_status status) {
	unsigned int i;
	for (i = 0; st_drive_status[i].status != st_drive_unknown; i++)
		if (st_drive_status[i].status == status)
			return st_drive_status[i].name;

	return st_drive_status[i].name;
}

__asm__(".symver st_drive_string_to_status_v1, st_drive_string_to_status@@LIBSTONE_1.2");
enum st_drive_status st_drive_string_to_status_v1(const char * status) {
	if (status == NULL)
		return st_drive_unknown;

	unsigned int i;
	for (i = 0; st_drive_status[i].status != st_drive_unknown; i++)
		if (!strcasecmp(status, st_drive_status[i].name))
			return st_drive_status[i].status;

	return st_drive_status[i].status;
}

__asm__(".symver st_drive_sync_v1, st_drive_sync@@LIBSTONE_1.2");
void st_drive_sync_v1(struct st_drive * drive, struct st_value * new_drive) {
	struct st_slot * sl = drive->slot;

	free(drive->model);
	free(drive->vendor);
	free(drive->revision);
	free(drive->serial_number);
	drive->model = drive->vendor = drive->revision = drive->serial_number = NULL;

	char * status = NULL;
	char * mode = NULL;

	free(sl->volume_name);
	sl->volume_name = NULL;
	char * type = NULL;

	st_value_unpack(new_drive, "{sssssssssssssssbsisssfsisbs{sisssssb}}",
		"model", &drive->model,
		"vendor", &drive->vendor,
		"revision", &drive->revision,
		"serial number", &drive->serial_number,

		"status", &status,
		"enable", &drive->enable,

		"density code", &drive->density_code,
		"mode", &mode,
		"operation duration", &drive->operation_duration,
		"last clean", &drive->last_clean,
		"is empty", &drive->is_empty,

		"slot",
			"index", &sl->index,
			"volume name", &sl->volume_name,
			"type", &type,
			"enable", &sl->enable
	);

	if (status != NULL)
		drive->status = st_drive_string_to_status_v1(status);
	free(status);

	if (mode != NULL)
		drive->mode = st_media_string_to_format_mode_v1(mode);
	free(mode);

	if (type != NULL)
		sl->type = st_slot_string_to_type_v1(type);
	free(type);
}

