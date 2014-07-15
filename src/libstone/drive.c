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

#include "drive.h"
#include "media.h"
#include "slot.h"
#include "string.h"
#include "value.h"

static struct st_drive_status2 {
	const char * name;
	const enum st_drive_status status;
	unsigned long long hash;
} st_drive_status[] = {
	{ "cleaning",		st_drive_status_cleaning,    0 },
	{ "empty idle",		st_drive_status_empty_idle,  0 },
	{ "erasing",		st_drive_status_erasing,     0 },
	{ "error",			st_drive_status_error,       0 },
	{ "loaded idle",	st_drive_status_loaded_idle, 0 },
	{ "loading",		st_drive_status_loading,     0 },
	{ "positioning",	st_drive_status_positioning, 0 },
	{ "reading",		st_drive_status_reading,     0 },
	{ "rewinding",		st_drive_status_rewinding,   0 },
	{ "unloading",		st_drive_status_unloading,   0 },
	{ "writing",		st_drive_status_writing,     0 },

	{ "unknown", st_drive_status_unknown, 0 },
};

static void st_drive_init(void) __attribute__((constructor));


__asm__(".symver st_drive_convert_v1, st_drive_convert@@LIBSTONE_1.2");
struct st_value * st_drive_convert_v1(struct st_drive * drive, bool with_slot) {
	struct st_value * last_clean = st_value_new_null();
	if (drive->last_clean > 0)
		last_clean = st_value_new_integer(drive->last_clean);

	struct st_value * dr = st_value_pack("{sssssssssssbsisssfsosb}",
		"model", drive->model,
		"vendor", drive->vendor,
		"revision", drive->revision,
		"serial number", drive->serial_number,

		"status", st_drive_status_to_string(drive->status),
		"enable", drive->enable,

		"density code", (long int) drive->density_code,
		"mode", st_media_format_mode_to_string(drive->mode),
		"operation duration", drive->operation_duration,
		"last clean", last_clean,
		"is empty", drive->is_empty
	);

	if (with_slot)
		st_value_hashtable_put2_v1(dr, "slot", st_slot_convert_v1(drive->slot), true);

	return dr;
}

__asm__(".symver st_drive_free_v1, st_drive_free@@LIBSTONE_1.2");
void st_drive_free_v1(struct st_drive * drive) {
	if (drive == NULL)
		return;

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

static void st_drive_init() {
	int i;
	for (i = 0; st_drive_status[i].status != st_drive_status_unknown; i++)
		st_drive_status[i].hash = st_string_compute_hash2(st_drive_status[i].name);
	st_drive_status[i].hash = st_string_compute_hash2(st_drive_status[i].name);
}

__asm__(".symver st_drive_status_to_string_v1, st_drive_status_to_string@@LIBSTONE_1.2");
const char * st_drive_status_to_string_v1(enum st_drive_status status) {
	unsigned int i;
	for (i = 0; st_drive_status[i].status != st_drive_status_unknown; i++)
		if (st_drive_status[i].status == status)
			return st_drive_status[i].name;

	return st_drive_status[i].name;
}

__asm__(".symver st_drive_string_to_status_v1, st_drive_string_to_status@@LIBSTONE_1.2");
enum st_drive_status st_drive_string_to_status_v1(const char * status) {
	if (status == NULL)
		return st_drive_status_unknown;

	unsigned int i;
	const unsigned long long hash = st_string_compute_hash2(status);
	for (i = 0; st_drive_status[i].status != st_drive_status_unknown; i++)
		if (hash == st_drive_status[i].hash)
			return st_drive_status[i].status;

	return st_drive_status[i].status;
}

__asm__(".symver st_drive_sync_v1, st_drive_sync@@LIBSTONE_1.2");
void st_drive_sync_v1(struct st_drive * drive, struct st_value * new_drive, bool with_slot) {
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

	struct st_value * last_clean = NULL;
	struct st_value * slot = NULL;

	st_value_unpack(new_drive, "{sssssssssssbsisssfsosbso}",
		"model", &drive->model,
		"vendor", &drive->vendor,
		"revision", &drive->revision,
		"serial number", &drive->serial_number,

		"status", &status,
		"enable", &drive->enable,

		"density code", &drive->density_code,
		"mode", &mode,
		"operation duration", &drive->operation_duration,
		"last clean", &last_clean,
		"is empty", &drive->is_empty,

		"slot", &slot
	);

	if (status != NULL)
		drive->status = st_drive_string_to_status_v1(status);
	free(status);

	if (mode != NULL)
		drive->mode = st_media_string_to_format_mode_v1(mode);
	free(mode);

	if (last_clean->type == st_value_null)
		drive->last_clean = 0;
	else
		drive->last_clean = st_value_integer_get_v1(last_clean);

	if (type != NULL)
		sl->type = st_slot_string_to_type_v1(type);
	free(type);

	if (with_slot && slot != NULL)
		st_slot_sync(drive->slot, slot);
}

