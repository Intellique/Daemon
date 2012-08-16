/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Tue, 14 Aug 2012 11:06:48 +0200                         *
\*************************************************************************/

// free, malloc
#include <stdlib.h>
// time
#include <time.h>

#include <stone/library/backup-db.h>


void st_backupdb_free(struct st_backupdb * backup) {
	if (!backup)
		return;

	free(backup->volumes);
	free(backup);
}

struct st_backupdb * st_backupdb_new() {
	struct st_backupdb * backup = malloc(sizeof(struct st_backupdb));
	backup->id = -1;
	backup->ctime = time(0);
	backup->volumes = 0;
	backup->nb_volumes = 0;
	return backup;
}

struct st_backupdb_volume * st_backupdb_volume_new(struct st_backupdb * backup, struct st_tape * tape, long tape_position) {
	if (!backup || !tape)
		return 0;

	void * new_addr = realloc(backup->volumes, (backup->nb_volumes + 1) * sizeof(struct st_backupdb_volume));
	if (!new_addr)
		return 0;

	backup->volumes = new_addr;
	struct st_backupdb_volume * volume = backup->volumes + backup->nb_volumes;
	backup->nb_volumes++;

	volume->id = -1;
	volume->sequence = backup->volumes - volume;
	volume->tape = tape;
	volume->tape_position = tape_position;

	return volume;
}

