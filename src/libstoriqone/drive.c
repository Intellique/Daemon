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

// gettext
#include <libintl.h>
// free
#include <stdlib.h>

#define gettext_noop(String) String

#include "drive.h"
#include "media.h"
#include "slot.h"
#include "string.h"
#include "value.h"

static struct st_drive_status2 {
	unsigned long long hash;
	const char * name;
	const enum st_drive_status status;
} st_drive_status[] = {
	[st_drive_status_cleaning]    = { 0, gettext_noop("cleaning"),		st_drive_status_cleaning },
	[st_drive_status_empty_idle]  = { 0, gettext_noop("empty idle"),	st_drive_status_empty_idle },
	[st_drive_status_erasing]     = { 0, gettext_noop("erasing"),		st_drive_status_erasing },
	[st_drive_status_error]       = { 0, gettext_noop("error"),			st_drive_status_error },
	[st_drive_status_loaded_idle] = { 0, gettext_noop("loaded idle"),	st_drive_status_loaded_idle },
	[st_drive_status_loading]     = { 0, gettext_noop("loading"),		st_drive_status_loading },
	[st_drive_status_positioning] = { 0, gettext_noop("positioning"),	st_drive_status_positioning },
	[st_drive_status_reading]     = { 0, gettext_noop("reading"),		st_drive_status_reading },
	[st_drive_status_rewinding]   = { 0, gettext_noop("rewinding"),		st_drive_status_rewinding },
	[st_drive_status_unloading]   = { 0, gettext_noop("unloading"),		st_drive_status_unloading },
	[st_drive_status_writing]     = { 0, gettext_noop("writing"),		st_drive_status_writing },

	[st_drive_status_unknown] = { 0, gettext_noop("unknown"), st_drive_status_unknown },
};

static void st_drive_init(void) __attribute__((constructor));


__asm__(".symver st_drive_convert_v1, st_drive_convert@@LIBSTONE_1.2");
struct st_value * st_drive_convert_v1(struct st_drive_v1 * drive, bool with_slot) {
	struct st_value * last_clean = st_value_new_null();
	if (drive->last_clean > 0)
		last_clean = st_value_new_integer(drive->last_clean);

	struct st_value * dr = st_value_pack("{sssssssssssbsisssfsosb}",
		"model", drive->model,
		"vendor", drive->vendor,
		"revision", drive->revision,
		"serial number", drive->serial_number,

		"status", st_drive_status_to_string(drive->status, false),
		"enable", drive->enable,

		"density code", (long int) drive->density_code,
		"mode", st_media_format_mode_to_string(drive->mode, false),
		"operation duration", drive->operation_duration,
		"last clean", last_clean,
		"is empty", drive->is_empty
	);

	if (with_slot)
		st_value_hashtable_put2_v1(dr, "slot", st_slot_convert_v1(drive->slot), true);

	return dr;
}

__asm__(".symver st_drive_free_v1, st_drive_free@@LIBSTONE_1.2");
void st_drive_free_v1(struct st_drive_v1 * drive) {
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
	unsigned int i;
	for (i = 0; i < sizeof(st_drive_status) / sizeof(*st_drive_status); i++)
		st_drive_status[i].hash = st_string_compute_hash2(st_drive_status[i].name);
}

__asm__(".symver st_drive_status_to_string_v1, st_drive_status_to_string@@LIBSTONE_1.2");
const char * st_drive_status_to_string_v1(enum st_drive_status status, bool translate) {
	const char * value = st_drive_status[status].name;
	if (translate)
		value = gettext(value);
	return value;
}

__asm__(".symver st_drive_string_to_status_v1, st_drive_string_to_status@@LIBSTONE_1.2");
enum st_drive_status st_drive_string_to_status_v1(const char * status) {
	if (status == NULL)
		return st_drive_status_unknown;

	unsigned int i;
	const unsigned long long hash = st_string_compute_hash2(status);
	for (i = 0; i < sizeof(st_drive_status) / sizeof(*st_drive_status); i++)
		if (hash == st_drive_status[i].hash)
			return st_drive_status[i].status;

	return st_drive_status[i].status;
}

__asm__(".symver st_drive_sync_v1, st_drive_sync@@LIBSTONE_1.2");
void st_drive_sync_v1(struct st_drive_v1 * drive, struct st_value * new_drive, bool with_slot) {
	struct st_slot * sl = drive->slot;

	free(drive->model);
	free(drive->vendor);
	free(drive->revision);
	free(drive->serial_number);
	drive->model = drive->vendor = drive->revision = drive->serial_number = NULL;

	char * status = NULL;
	char * mode = NULL;

	if (sl != NULL) {
		free(sl->volume_name);
		sl->volume_name = NULL;
	}

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

	if (with_slot && slot != NULL)
		st_slot_sync(drive->slot, slot);
}

