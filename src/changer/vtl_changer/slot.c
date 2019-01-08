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

#define _GNU_SOURCE
// asprintf
#include <stdio.h>
// malloc
#include <stdlib.h>
// bzero
#include <strings.h>
// symlink
#include <unistd.h>

#include <libstoriqone/drive.h>
#include <libstoriqone/file.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>

#include "device.h"

bool sochgr_vtl_slot_create(struct so_slot * slot, const char * root_directory, const char * prefix, long long index) {
	int size = asprintf(&slot->volume_name, "%s%03Ld", prefix, index);
	if (size < 0)
		return false;

	slot->full = true;

	struct sochgr_vtl_changer_slot * vtl_sl = slot->data = malloc(sizeof(struct sochgr_vtl_changer_slot));
	bzero(vtl_sl, sizeof(struct sochgr_vtl_changer_slot));
	size = asprintf(&vtl_sl->path, "%s/slots/%Ld", root_directory, index);
	if (size < 0)
		return false;

	size = asprintf(&vtl_sl->media_dir, "%s/medias/%s%03Ld", root_directory, prefix, index);
	if (size < 0)
		return false;

	so_file_mkdir(vtl_sl->path, 0700);

	char * media_link, * link;
	size = asprintf(&media_link, "../../medias/%s%03Ld", prefix, index);
	if (size < 0)
		return false;

	size = asprintf(&link, "%s/slots/%Ld/media", root_directory, index);
	if (size < 0)
		return false;

	if (so_file_rm(link) != 0)
		return false;

	int failed = symlink(media_link, link);
	if (failed != 0)
		return false;

	free(link);
	free(media_link);

	return true;
}

void sochgr_vtl_slot_delete(struct so_slot * slot) {
	free(slot->volume_name);

	struct sochgr_vtl_changer_slot * vtl_sl = slot->data;
	so_file_rm(vtl_sl->path);
	so_file_rm(vtl_sl->media_dir);

	free(vtl_sl->path);
	free(vtl_sl->media_dir);
	free(vtl_sl);

	slot->data = NULL;
	so_value_free(slot->db_data);
}


bool sochgr_vtl_drive_slot_create(struct so_drive * dr, struct so_slot * sl, const char * root_directory, long long index) {
	sl->index = index;
	sl->drive = dr;
	dr->slot = sl;

	sl->full = false;
	sl->enable = true;

	struct sochgr_vtl_changer_slot * vtl_sl = sl->data = malloc(sizeof(struct sochgr_vtl_changer_slot));
	bzero(vtl_sl, sizeof(struct sochgr_vtl_changer_slot));
	return asprintf(&vtl_sl->path, "%s/drives/%Ld", root_directory, index) >= 0;
}
