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

#include <libstone/media.h>
#include <libstone/slot.h>
#include <libstone/value.h>

#include "drive.h"

static struct st_drive_driver * current_drive = NULL;


struct st_value * stdr_drive_convert(struct st_drive * dr) {
	struct st_slot * sl = dr->slot;
	struct st_value * media = st_value_new_null();

	if (sl->media != NULL) {
		media = st_value_pack("{sssssssssssssisisisisisisisisisisisisisiss}",
			"uuid", sl->media->uuid[0] != '\0' ? sl->media->uuid : NULL,
			"label", sl->media->label,
			"medium serial number", sl->media->medium_serial_number,
			"name", sl->media->name,

			"status", st_media_status_to_string(sl->media->status),
			"location", st_media_location_to_string(sl->media->location),

			"first used", sl->media->first_used,
			"use before", sl->media->use_before,

			"load count", sl->media->load_count,
			"read count", sl->media->read_count,
			"write count", sl->media->write_count,
			"operation count", sl->media->operation_count,

			"nb total read", sl->media->nb_total_read,
			"nb total write", sl->media->nb_total_write,

			"nb read errors", (unsigned long int) sl->media->nb_read_errors,
			"nb write errors", (unsigned long int) sl->media->nb_write_errors,

			"block size", sl->media->block_size,
			"free block", sl->media->free_block,
			"total block", sl->media->total_block,

			"nb volumes", (unsigned long int) sl->media->nb_volumes,
			"type", st_media_type_to_string(sl->media->type)
		);

		if (sl->media->last_read > 0)
			st_value_hashtable_put2(media, "last read", st_value_new_integer(sl->media->last_read), true);
		else
			st_value_hashtable_put2(media, "last read", st_value_new_null(), true);

		if (sl->media->last_write > 0)
			st_value_hashtable_put2(media, "last write", st_value_new_integer(sl->media->last_write), true);
		else
			st_value_hashtable_put2(media, "last write", st_value_new_null(), true);
	}

	long int density_code = dr->density_code;
	long int slot_index = sl->index;

	struct st_value * last_clean = st_value_new_null();
	if (dr->last_clean > 0)
		last_clean = st_value_new_integer(dr->last_clean);

	return st_value_pack("{sssssssssssbsisssfsosbs{sisssssbso}}",
		"model", dr->model,
		"vendor", dr->vendor,
		"revision", dr->revision,
		"serial number", dr->serial_number,

		"status", st_drive_status_to_string(dr->status),
		"enable", dr->enable,

		"density code", density_code,
		"mode", st_media_format_mode_to_string(dr->mode),
		"operation duration", dr->operation_duration,
		"last clean", last_clean,
		"is empty", dr->is_empty,

		"slot",
			"index", slot_index,
			"volume name", sl->volume_name,
			"type", st_slot_type_to_string(sl->type),
			"enable", sl->enable,
			"media", media
	);
}

struct st_drive_driver * stdr_drive_get() {
	return current_drive;
}

__asm__(".symver stdr_drive_register_v1, stdr_drive_register@@LIBSTONE_DRIVE_1.2");
void stdr_drive_register_v1(struct st_drive_driver * dr) {
	current_drive = dr;
}

