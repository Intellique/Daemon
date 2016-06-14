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

#ifndef __LIBSTORIQONE_CHANGER_DRIVE_H__
#define __LIBSTORIQONE_CHANGER_DRIVE_H__

#include <libstoriqone/drive.h>

struct so_value;

struct so_drive_ops {
	bool (*check_format)(struct so_drive * drive, struct so_media * media, struct so_pool * pool, const char * archive_uuid);
	bool (*check_support)(struct so_drive * drive, struct so_media_format * format, bool for_writing);
	void (*free)(struct so_drive * drive);
	bool (*is_free)(struct so_drive * drive);
	int (*load_media)(struct so_drive * drive, struct so_media * media);
	int (*lock)(struct so_drive * drive, const char * job_id);
	int (*reset)(struct so_drive * drive);
	int (*stop)(struct so_drive * drive);
	int (*update_status)(struct so_drive * drive);
};

void sochgr_drive_register(struct so_drive * drive, struct so_value * config, const char * process_name);

#endif

