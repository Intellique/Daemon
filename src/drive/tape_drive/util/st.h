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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __SO_TAPEDRIVE_UTIL_ST_H__
#define __SO_TAPEDRIVE_UTIL_ST_H__

// bool
#include <stdbool.h>

struct so_drive;
struct so_database_connection;

struct mtget;

int sodr_tape_drive_st_get_position(struct so_drive * drive, int fd, struct so_database_connection * db);
int sodr_tape_drive_st_get_status(struct so_drive * drive, int fd, struct mtget * status, struct so_database_connection * db);
int sodr_tape_drive_st_mk_1_partition(struct so_drive * drive, int fd);
int sodr_tape_drive_st_rewind(struct so_drive * drive, int fd, struct so_database_connection * db);
int sodr_tape_drive_st_set_can_partition(struct so_drive * drive, int fd, struct so_database_connection * db);
int sodr_tape_drive_st_set_position(struct so_drive * drive, int fd, unsigned int partition, int file_number, bool force, struct so_database_connection * db);
int sodr_tape_drive_st_write_end_of_file(struct so_drive * drive, int fd);

#endif

