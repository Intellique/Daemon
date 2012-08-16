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
*  Last modified: Tue, 14 Aug 2012 10:58:47 +0200                         *
\*************************************************************************/

#ifndef __STONE_LIBRARY_BACKUPDB_H__
#define __STONE_LIBRARY_BACKUPDB_H__

// time_t
#include <sys/time.h>

struct st_backupdb_volume;

struct st_backupdb {
	long id;

	time_t ctime;
	struct st_backupdb_volume * volumes;
	unsigned int nb_volumes;
};

struct st_backupdb_volume {
	long id;

	long sequence;
	struct st_tape * tape;
	long tape_position;
};

void st_backupdb_free(struct st_backupdb * backup);
struct st_backupdb * st_backupdb_new(void);
struct st_backupdb_volume * st_backupdb_volume_new(struct st_backupdb * backup, struct st_tape * tape, long tape_position);

#endif

