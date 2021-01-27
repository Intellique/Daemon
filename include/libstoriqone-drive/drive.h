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

#ifndef __LIBSTORIQONE_DRIVE_DRIVE_H__
#define __LIBSTORIQONE_DRIVE_DRIVE_H__

// bool
#include <stdbool.h>

#include <libstoriqone/drive.h>

struct so_archive_volume;
struct so_database_connection;
struct so_format_reader;
struct so_format_writer;
struct so_pool;
struct so_stream_reader;
struct so_stream_writer;
struct so_value;

struct so_drive_driver {
	const char * name;

	struct so_drive * device;
	int (*configure_device)(struct so_value * config);

	const char * src_checksum;
};

struct so_drive_ops {
	bool (*check_format)(struct so_media * media, struct so_pool * pool, const char * archive_uuid, struct so_database_connection * db);
	bool (*check_header)(struct so_database_connection * db);
	bool (*check_support)(struct so_media_format * format, bool for_writing, struct so_database_connection * db);
	unsigned int (*count_archives)(const bool * const disconnected, struct so_database_connection * db);
	struct so_format_writer * (*create_archive_volume)(struct so_archive_volume * volume, struct so_value * checksums, struct so_database_connection * db);
	int (*eject)(struct so_database_connection * db);
	int (*erase_media)(bool quick_mode, struct so_database_connection * db);
	int (*format_media)(struct so_pool * pool, struct so_value * option, struct so_database_connection * db);
	struct so_stream_reader * (*get_raw_reader)(int file_position, struct so_database_connection * db);
	struct so_stream_writer * (*get_raw_writer)(struct so_database_connection * db);
	struct so_format_reader * (*get_reader)(int file_position, struct so_value * checksums, struct so_database_connection * db);
	struct so_format_writer * (*get_writer)(struct so_value * checksums, struct so_database_connection * db);
	int (*init)(struct so_value * config, struct so_database_connection * db_connect);
	struct so_format_reader * (*open_archive_volume)(struct so_archive_volume * volume, struct so_value * checksums, struct so_database_connection * db);
	struct so_archive * (*parse_archive)(const bool * const disconnected, unsigned int archive_position, struct so_value * checksums, struct so_database_connection * db);
	int (*reset)(struct so_database_connection * db);
	int (*update_status)(struct so_database_connection * db);
};

bool sodr_drive_is_locked(void);
void sodr_drive_register(struct so_drive_driver * dr);

#endif

