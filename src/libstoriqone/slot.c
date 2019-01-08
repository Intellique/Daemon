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
*  Copyright (C) 2013-2019, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// free
#include <stdlib.h>
// bzero
#include <strings.h>

#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>


struct so_value * so_slot_convert(struct so_slot * slot) {
	struct so_value * media = so_value_new_null();
	if (slot->media != NULL)
		media = so_media_convert(slot->media);

	return so_value_pack("{susbsssbsbso}",
		"index", slot->index,
		"full", slot->full,
		"volume name", slot->volume_name,
		"ie port", slot->is_ie_port,
		"enable", slot->enable,
		"media", media
	);
}

void so_slot_free(struct so_slot * slot) {
	if (slot == NULL)
		return;

	free(slot->volume_name);
	free(slot->data);
}

void so_slot_free2(void * slot) {
	so_slot_free(slot);
}

void so_slot_sync(struct so_slot * slot, struct so_value * new_slot) {
	free(slot->volume_name);
	slot->volume_name = NULL;

	struct so_value * media = NULL;

	so_value_unpack(new_slot, "{susbsssbsbso}",
		"index", &slot->index,
		"full", &slot->full,
		"volume name", &slot->volume_name,
		"ie port", &slot->is_ie_port,
		"enable", &slot->enable,
		"media", &media
	);

	if (slot->media != NULL && (media == NULL || media->type == so_value_null)) {
		so_media_free(slot->media);
		slot->media = NULL;
	} else if (media != NULL && media->type != so_value_null) {
		if (slot->media == NULL) {
			slot->media = malloc(sizeof(struct so_media));
			bzero(slot->media, sizeof(struct so_media));
		}

		so_media_sync(slot->media, media);
	}
}

void so_slot_swap(struct so_slot * sa, struct so_slot * sb) {
	struct so_media * media = sa->media;
	sa->media = sb->media;
	sb->media = media;

	char * volume_name = sa->volume_name;
	sa->volume_name = sb->volume_name;
	sb->volume_name = volume_name;

	bool full = sa->full;
	sa->full = sb->full;
	sb->full = full;
}
