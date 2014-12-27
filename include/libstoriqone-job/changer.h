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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

#ifndef __LIBSTORIQONE_JOB_CHANGER_H__
#define __LIBSTORIQONE_JOB_CHANGER_H__

// ssize_t
#include <sys/types.h>

#include <libstoriqone/changer.h>

struct so_database_connection;
struct so_drive;
struct so_job;
struct so_media;
struct so_media_format;
enum so_pool_unbreakable_level;
struct so_slot;
struct so_value;

struct so_changer_ops {
	struct so_drive * (*get_media)(struct so_changer * changer, struct so_slot * slot);
	int (*release_media)(struct so_changer * changer, struct so_slot * slot);
	ssize_t (*reserve_media)(struct so_changer * changer, struct so_slot * slot, size_t size_need, enum so_pool_unbreakable_level unbreakable_level);
	int (*sync)(struct so_changer * changer);

	int (*release_all_media)(struct so_changer * changer);
	int (*reserve_medias)(struct so_changer * changer, struct so_value * medias, bool for_writing);

	struct so_drive * (*find_free_drive)(struct so_changer * changer, struct so_media_format * format, bool for_writing);
	int (*load)(struct so_changer * changer, struct so_slot * from, struct so_drive * to);
	int (*unload)(struct so_changer * changer, struct so_drive * from);
};

struct so_slot * soj_changer_find_media_by_job(struct so_job * job, struct so_database_connection * db_connection) __attribute__((nonnull));
struct so_slot * soj_changer_find_slot(struct so_media * media) __attribute__((nonnull,warn_unused_result));
void soj_changer_set_config(struct so_value * config) __attribute__((nonnull));
int soj_changer_sync_all(void);

bool soj_changer_has_apt_drive(struct so_media_format * format, bool for_writing);

#endif

