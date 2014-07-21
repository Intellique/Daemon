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
// bzero
#include <strings.h>

#include "media.h"
#include "slot.h"
#include "string.h"
#include "value.h"


__asm__(".symver st_slot_convert_v1, st_slot_convert@@LIBSTONE_1.2");
struct st_value * st_slot_convert_v1(struct st_slot * slot) {
	struct st_value * media = st_value_new_null_v1();
	if (slot->media != NULL)
		media = st_media_convert_v1(slot->media);

	return st_value_pack("{sisssbsbso}",
		"index", (long int) slot->index,
		"volume name", slot->volume_name,
		"ie port", slot->is_ie_port,
		"enable", slot->enable,
		"media", media
	);
}

__asm__(".symver st_slot_type_to_string_v1, st_slot_type_to_string@@LIBSTONE_1.2");
void st_slot_free_v1(struct st_slot * slot) {
	if (slot == NULL)
		return;

	free(slot->volume_name);
	free(slot->data);
}

__asm__(".symver st_slot_type_to_string_v1, st_slot_type_to_string@@LIBSTONE_1.2");
void st_slot_free2_v1(void * slot) {
	st_slot_free_v1(slot);
}

__asm__(".symver st_slot_sync_v1, st_slot_sync@@LIBSTONE_1.2");
void st_slot_sync_v1(struct st_slot * slot, struct st_value * new_slot) {
	free(slot->volume_name);
	slot->volume_name = NULL;

	struct st_value * media = NULL;
	long int index = -1;

	st_value_unpack_v1(new_slot, "{sisssbsbso}",
		"index", &index,
		"volume name", &slot->volume_name,
		"ie port", &slot->is_ie_port,
		"enable", &slot->enable,
		"media", &media
	);

	slot->index = index;

	if (slot->media != NULL && (media == NULL || media->type == st_value_null)) {
		st_media_free_v1(slot->media);
		slot->media = NULL;
	} else if (media != NULL && media->type != st_value_null) {
		if (slot->media == NULL) {
			slot->media = malloc(sizeof(struct st_media));
			bzero(slot->media, sizeof(struct st_media));
		}

		st_media_sync_v1(slot->media, media);
	}
}

