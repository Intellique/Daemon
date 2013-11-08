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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Fri, 08 Nov 2013 16:21:36 +0100                            *
\****************************************************************************/

// malloc, free, realloc
#include <stdlib.h>
// bzero
#include <strings.h>

#include <libstone/backup.h>
#include <libstone/log.h>

void st_backup_add_volume(struct st_backup * backup, struct st_media * media, unsigned int position) {
	void * addr = realloc(backup->volumes, (backup->nb_volumes + 1) * sizeof(struct st_backup_volume));
	if (addr == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "failed to add volume to backup, because %m");
		return;
	}

	backup->volumes = addr;
	struct st_backup_volume * bv = backup->volumes + backup->nb_volumes;
	backup->nb_volumes++;
	bv->media = media;
	bv->position = position;
}

void st_backup_free(struct st_backup * backup) {
	if (backup == NULL)
		return;

	free(backup->volumes);
	free(backup);
}

struct st_backup * st_backup_new() {
	struct st_backup * bck = malloc(sizeof(struct st_backup));
	bzero(bck, sizeof(struct st_backup));
	return bck;
}

