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

#ifndef __LIBSTORIQONE_DRIVE_MEDIA_H__
#define __LIBSTORIQONE_DRIVE_MEDIA_H__

// bool
#include <stdbool.h>
// size_t
#include <sys/types.h>

#include <libstoriqone/media.h>

struct so_database_connection;
struct so_drive;
struct so_media;

bool sodr_media_check_header(struct so_media * media, const char * buffer, bool restore_data, struct so_database_connection * db);
bool sodr_media_write_header(struct so_media * media, struct so_pool * pool, char * buffer, size_t length);

unsigned int sodr_media_storiqone_count_files(struct so_drive * drive, const bool * const disconnected, struct so_database_connection * db_connection);
struct so_archive * sodr_media_storiqone_parse_archive(struct so_drive * drive, const bool * const disconnected, unsigned int archive_position, struct so_database_connection * db_connection);

#endif
