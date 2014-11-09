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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// malloc
#include <stdlib.h>

#include <libstoriqone/changer.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>

#include "media.h"

void sochgr_media_init(struct so_changer * changer) {
	unsigned int i;
	for (i = 0; i < changer->nb_slots; i++)
		sochgr_media_init_slot(changer->slots + i);
}

void sochgr_media_init_slot(struct so_slot * slot) {
	struct so_media * media = slot->media;
	if (media != NULL && media->changer_data == NULL) {
		struct sochgr_media * md = media->changer_data = malloc(sizeof(struct sochgr_media));
		md->peer = NULL;
	}
}

