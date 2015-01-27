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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

// free, malloc, realloc
#include <stdlib.h>
// bzero
#include <strings.h>
// time
#include <time.h>

#include <libstoriqone/value.h>
#include <libstoriqone-job/backup.h>

void soj_backup_add_volume(struct so_backup * backup, struct so_media * media, ssize_t size, unsigned int position, struct so_value * digests) {
	void * addr = realloc(backup->volumes, (backup->nb_volumes + 1) * sizeof(struct so_backup_volume));
	if (addr == NULL)
		return;

	backup->volumes = addr;
	struct so_backup_volume * vol = backup->volumes + backup->nb_volumes;
	bzero(vol, sizeof(struct so_backup_volume));
	vol->media = media;
	vol->size = size;
	vol->position = position;
	vol->digests = digests;
	backup->nb_volumes++;
}

void soj_backup_free(struct so_backup * backup) {
	if (backup == NULL)
		return;

	unsigned int i;
	for (i = 0; i < backup->nb_volumes; i++) {
		struct so_backup_volume * vol = backup->volumes + i;
		so_value_free(vol->digests);
		so_value_free(vol->db_data);
	}

	free(backup->volumes);
	so_value_free(backup->db_data);
	free(backup);
}

struct so_backup * soj_backup_new(struct so_job * job) {
	struct so_backup * backup = malloc(sizeof(struct so_backup));
	bzero(backup, sizeof(struct so_backup));

	backup->timestamp = time(NULL);
	backup->job = job;

	return backup;
}

