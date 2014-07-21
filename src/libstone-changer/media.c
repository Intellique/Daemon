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

// malloc
#include <stdlib.h>

#include <libstone/changer.h>
#include <libstone/media.h>
#include <libstone/slot.h>

#include "media.h"

void stchgr_media_init(struct st_changer * changer) {
	unsigned int i;
	for (i = 0; i < changer->nb_slots; i++)
		stchgr_media_init_slot(changer->slots + i);
}

void stchgr_media_init_slot(struct st_slot * slot) {
	struct st_media * media = slot->media;
	if (media != NULL && media->changer_data == NULL) {
		struct stchgr_media * md = media->changer_data = malloc(sizeof(struct stchgr_media));
		md->peer = NULL;
	}
}

