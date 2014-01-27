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
*  Last modified: Fri, 22 Feb 2013 11:54:24 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// asprintf, sscanf
#include <stdio.h>
// free
#include <stdlib.h>
// strcmp
#include <string.h>
// readlink
#include <unistd.h>

#include <libstone/library/changer.h>
#include <libstone/library/media.h>
#include <libstone/library/slot.h>

#include "common.h"

struct st_media * st_vtl_slot_get_media(struct st_changer * changer, const char * base_dir) {
	char * media_link;
	asprintf(&media_link, "%s/media", base_dir);

	char link[64];
	ssize_t length = readlink(media_link, link, 64);

	free(media_link);

	if (length < 0)
		return NULL;

	link[length] = '\0';

	char label[7];
	if (sscanf(link, "../../medias/%6c", label) < 1)
		return NULL;

	label[6] = '\0';

	struct st_vtl_changer * ch_data = changer->data;
	unsigned int i;
	for (i = 0; i < ch_data->nb_medias; i++) {
		struct st_media * md = ch_data->medias[i];
		struct st_vtl_media * vtl_md = md->data;

		if (!vtl_md->used && !strcmp(label, md->label)) {
			vtl_md->used = true;
			return md;
		}
	}

	return NULL;
}

void st_vtl_slot_init(struct st_slot * sl) {
	sl->media = NULL;

	sl->volume_name = NULL;
	sl->full = false;
	sl->enable = true;

	sl->data = NULL;
	sl->db_data = NULL;
}

