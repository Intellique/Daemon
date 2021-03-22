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

#ifndef __STORIQONE_CHANGER_VTL_DEVICE_H__
#define __STORIQONE_CHANGER_VTL_DEVICE_H__

struct so_database_connection;
struct so_drive;
struct so_media_format;
struct so_slot;

struct sochgr_vtl_drive {
	char * drive_dir;
	struct so_value * params;
};

struct sochgr_vtl_changer_slot {
	char * path;
	struct so_slot * origin;
	char * media_dir;
};

struct so_changer * sochgr_vtl_changer_get_device(void);

bool sochgr_vtl_drive_create(struct sochgr_vtl_drive * dr_p, struct so_drive * dr, const char * root_directory, long long index);
void sochgr_vtl_drive_delete(struct so_drive * drive);

struct so_media * sochgr_vtl_media_create(const char * root_directory, const char * prefix, long long index, struct so_media_format * format, struct so_database_connection * db_connection);
void sochgr_vtl_media_update_capacity(struct so_changer * changer, struct so_media * media, unsigned long long new_capacity);

bool sochgr_vtl_slot_create(struct so_slot * slot, const char * root_directory, const char * prefix, long long index);
void sochgr_vtl_slot_delete(struct so_slot * slot);

bool sochgr_vtl_drive_slot_create(struct so_drive * dr, struct so_slot * slot, const char * root_directory, long long index);

#endif

