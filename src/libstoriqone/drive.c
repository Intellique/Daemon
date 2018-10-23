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
*  Copyright (C) 2013-2018, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// dgettext, gettext
#include <libintl.h>
// free
#include <stdlib.h>

#define gettext_noop(String) String

#include <libstoriqone/drive.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

static struct so_drive_status2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_drive_status status;
} so_drive_status[] = {
	[so_drive_status_cleaning]    = { 0, 0, gettext_noop("cleaning"),    NULL, so_drive_status_cleaning },
	[so_drive_status_empty_idle]  = { 0, 0, gettext_noop("empty idle"),  NULL, so_drive_status_empty_idle },
	[so_drive_status_erasing]     = { 0, 0, gettext_noop("erasing"),     NULL, so_drive_status_erasing },
	[so_drive_status_error]       = { 0, 0, gettext_noop("error"),       NULL, so_drive_status_error },
	[so_drive_status_loaded_idle] = { 0, 0, gettext_noop("loaded idle"), NULL, so_drive_status_loaded_idle },
	[so_drive_status_loading]     = { 0, 0, gettext_noop("loading"),     NULL, so_drive_status_loading },
	[so_drive_status_positioning] = { 0, 0, gettext_noop("positioning"), NULL, so_drive_status_positioning },
	[so_drive_status_reading]     = { 0, 0, gettext_noop("reading"),     NULL, so_drive_status_reading },
	[so_drive_status_rewinding]   = { 0, 0, gettext_noop("rewinding"),   NULL, so_drive_status_rewinding },
	[so_drive_status_unloading]   = { 0, 0, gettext_noop("unloading"),   NULL, so_drive_status_unloading },
	[so_drive_status_writing]     = { 0, 0, gettext_noop("writing"),     NULL, so_drive_status_writing },

	[so_drive_status_unknown] = { 0, 0, gettext_noop("unknown status"), NULL, so_drive_status_unknown },
};
static const unsigned int so_drive_nb_status = sizeof(so_drive_status) / sizeof(*so_drive_status);

static void so_drive_init(void) __attribute__((constructor));


struct so_value * so_drive_convert(struct so_drive * drive, bool with_slot) {
	struct so_value * last_clean = so_value_new_null();
	if (drive->last_clean > 0)
		last_clean = so_value_new_integer(drive->last_clean);

	struct so_value * dr = so_value_pack("{sssssssssssbsusssfsosb}",
		"model", drive->model,
		"vendor", drive->vendor,
		"revision", drive->revision,
		"serial number", drive->serial_number,

		"status", so_drive_status_to_string(drive->status, false),
		"enable", drive->enable,

		"density code", drive->density_code,
		"mode", so_media_format_mode_to_string(drive->mode, false),
		"operation duration", drive->operation_duration,
		"last clean", last_clean,
		"is empty", drive->is_empty
	);

	if (with_slot)
		so_value_hashtable_put2(dr, "slot", so_slot_convert(drive->slot), true);

	return dr;
}

void so_drive_free(struct so_drive * drive) {
	if (drive == NULL)
		return;

	free(drive->model);
	free(drive->vendor);
	free(drive->revision);
	free(drive->serial_number);

	free(drive->data);
}

void so_drive_free2(void * drive) {
	so_drive_free(drive);
}

static void so_drive_init() {
	unsigned int i;
	for (i = 0; i < so_drive_nb_status; i++) {
		so_drive_status[i].hash = so_string_compute_hash2(so_drive_status[i].name);
		so_drive_status[i].translation = dgettext("libstoriqone", so_drive_status[i].name);
		so_drive_status[i].hash_translated = so_string_compute_hash2(so_drive_status[i].translation);
	}
}

const char * so_drive_status_to_string(enum so_drive_status status, bool translate) {
	if (translate)
		return so_drive_status[status].translation;
	else
		return so_drive_status[status].name;
}

enum so_drive_status so_drive_string_to_status(const char * status, bool translate) {
	if (status == NULL)
		return so_drive_status_unknown;

	unsigned int i;
	const unsigned long long hash = so_string_compute_hash2(status);

	if (translate) {
		for (i = 0; i < so_drive_nb_status; i++)
			if (hash == so_drive_status[i].hash_translated)
				return so_drive_status[i].status;
	} else {
		for (i = 0; i < so_drive_nb_status; i++)
			if (hash == so_drive_status[i].hash)
				return so_drive_status[i].status;
	}

	return so_drive_status_unknown;
}

void so_drive_sync(struct so_drive * drive, struct so_value * new_drive, bool with_slot) {
	struct so_slot * sl = drive->slot;

	free(drive->model);
	free(drive->vendor);
	free(drive->revision);
	free(drive->serial_number);
	drive->model = drive->vendor = drive->revision = drive->serial_number = NULL;

	const char * status = NULL, * mode = NULL;

	if (sl != NULL) {
		free(sl->volume_name);
		sl->volume_name = NULL;
	}

	struct so_value * last_clean = NULL;
	struct so_value * slot = NULL;

	so_value_unpack(new_drive, "{sssssssssSsbsusSsfsosbso}",
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
		drive->status = so_drive_string_to_status(status, false);

	if (mode != NULL)
		drive->mode = so_media_string_to_format_mode(mode, false);

	if (last_clean->type == so_value_null)
		drive->last_clean = 0;
	else
		drive->last_clean = so_value_integer_get(last_clean);

	if (with_slot && slot != NULL)
		so_slot_sync(drive->slot, slot);
}
