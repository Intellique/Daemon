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

#ifndef __LIBSTORIQONE_JOB_MEDIA_H__
#define __LIBSTORIQONE_JOB_MEDIA_H__

#include <libstoriqone/media.h>

// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>

struct so_database_connection;
struct so_pool;

struct so_drive * soj_media_find_and_load(struct so_media * media, bool no_wait, size_t size_need, bool * error, struct so_database_connection * db_connection);
struct so_drive * soj_media_find_and_load2(struct so_media * media, struct so_pool * pool, bool no_wait, size_t size_need, bool * error, struct so_database_connection * db_connection);
struct so_drive * soj_media_find_and_load_next(struct so_pool * pool, bool no_wait, bool * error, struct so_database_connection * db_connection);
ssize_t soj_media_prepare(struct so_pool * pool, ssize_t size_needed, const char * archive_uuid, struct so_database_connection * db_connection);
void soj_media_release_all_medias(struct so_pool * pool);

#endif

