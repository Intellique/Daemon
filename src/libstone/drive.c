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

	struct st_value * last_clean = NULL;

	struct st_value * media = NULL;
	st_value_unpack(new_drive, "{sssssssssssbsisssfsosbs{sisssssbso}}",
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

		"slot",
			"index", &sl->index,
			"volume name", &sl->volume_name,
			"type", &type,
			"enable", &sl->enable,
			"media", &media
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

	if (sl->media != NULL && (media == NULL || media->type == st_value_null)) {
		st_media_free_v1(sl->media);
		sl->media = NULL;
	} else if (media != NULL && media->type != st_value_null) {
		if (sl->media == NULL) {
			sl->media = malloc(sizeof(struct st_media));
			bzero(sl->media, sizeof(struct st_media));
		} else {
			free(sl->media->label);
			free(sl->media->medium_serial_number);
			free(sl->media->name);
			sl->media->label = sl->media->medium_serial_number = sl->media->name = NULL;
		}

		struct st_value * uuid = NULL;
		char * status = NULL, * location = NULL, * type = NULL;
		struct st_value * last_read = NULL, * last_write = NULL;
		long int nb_read_errors = 0, nb_write_errors = 0;
		long int nb_volumes = 0;

		st_value_unpack(media, "{sosssssssssssisisososisisisisisisisisisiss}",
			"uuid", &uuid,
			"label", &sl->media->label,
			"medium serial number", &sl->media->medium_serial_number,
			"name", &sl->media->name,

			"status", &status,
			"location", &location,

			"first used", &sl->media->first_used,
			"use before", &sl->media->use_before,
			"last read", &last_read,
			"last write", &last_write,

			"load count", &sl->media->load_count,
			"read count", &sl->media->read_count,
			"write count", &sl->media->write_count,
			"operation count", &sl->media->operation_count,

			"nb total read", &sl->media->nb_total_read,
			"nb total write", &sl->media->nb_total_write,

			"nb read errors", &nb_read_errors,
			"nb write errors", &nb_write_errors,

			"block size", &sl->media->block_size,
			"free block", &sl->media->free_block,
			"total block", &sl->media->total_block,

			"nb volumes", &nb_volumes,
			"type", &type
		);

		if (uuid != NULL) {
			if (uuid->type != st_value_null) {
				strncpy(sl->media->uuid, st_value_string_get_v1(uuid), 36);
				sl->media->uuid[36] = '\0';
			} else {
				sl->media->uuid[0] = '\0';
			}
		}

		sl->media->status = st_media_string_to_status_v1(status);
		sl->media->location = st_media_string_to_location_v1(location);
		free(status);
		free(location);

		if (last_read != NULL) {
			if (last_read->type != st_value_null)
				sl->media->last_read = st_value_integer_get_v1(last_read);
			else
				sl->media->last_read = 0;
		}

		if (last_write != NULL) {
			if (last_write->type != st_value_null)
				sl->media->last_write = st_value_integer_get_v1(last_write);
			else
				sl->media->last_write = 0;
		}

		sl->media->nb_read_errors = nb_read_errors;
		sl->media->nb_write_errors = nb_write_errors;

		sl->media->nb_volumes = nb_volumes;
		sl->media->type = st_media_string_to_type_v1(type);
		free(type);
	}
}

