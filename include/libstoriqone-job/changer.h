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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_JOB_CHANGER_H__
#define __LIBSTORIQONE_JOB_CHANGER_H__

// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>

#include <libstoriqone/changer.h>

struct so_database_connection;
struct so_drive;
struct so_job;
struct so_media;
struct so_media_format;
struct so_pool;
struct so_slot;
struct so_value;

struct so_changer_ops {
	struct so_drive * (*get_media)(struct so_changer * changer, struct so_media * media, bool no_wait, bool * error, bool * media_refused);
	int (*release_media)(struct so_changer * changer, struct so_media * media);
	ssize_t (*reserve_media)(struct so_changer * changer, struct so_media * media, size_t size_need, bool append, const char * archive_uuid, struct so_pool * pool);
	int (*sync)(struct so_changer * changer);
};

struct so_slot * soj_changer_find_media_by_job(struct so_job * job, struct so_database_connection * db_connection);
struct so_slot * soj_changer_find_slot(struct so_media * media) __attribute__((warn_unused_result));
bool soj_changer_has_apt_drive(struct so_media_format * format, bool for_writing);
unsigned int soj_changer_nb_totals_drives(void);
void soj_changer_set_config(struct so_value * config);
int soj_changer_sync_all(void);

#endif
