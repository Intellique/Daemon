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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_JOB_MEDIA_H__
#define __LIBSTORIQONE_JOB_MEDIA_H__

// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>

struct so_database_connection;
struct so_drive;
struct so_media_format;
struct so_pool;
enum so_pool_unbreakable_level;
struct so_value;

struct so_value_iterator * soj_media_get_iterator(struct so_pool * pool);
struct so_drive * soj_media_load(struct so_media * media, bool no_wait);
ssize_t soj_media_prepare(struct so_pool * pool, ssize_t size_needed, struct so_database_connection * db_connection);
ssize_t soj_media_prepare_unformatted(struct so_pool * pool, bool online, struct so_database_connection * db_connection);
void soj_media_release_all_medias(struct so_pool * pool);
struct so_value * soj_media_reserve(struct so_pool * pool, size_t space_need, enum so_pool_unbreakable_level unbreakable_level);

#endif
