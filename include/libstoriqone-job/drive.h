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

#ifndef __LIBSTORIQONE_JOB_DRIVE_H__
#define __LIBSTORIQONE_JOB_DRIVE_H__

#include <libstoriqone/drive.h>

struct so_drive;
struct so_pool;
struct so_value;

struct so_drive_ops {
	bool (*check_header)(struct so_drive * drive);
	bool (*check_support)(struct so_drive * drive, struct so_media_format * format, bool for_writing);
	int (*erase_media)(struct so_drive * drive, bool quick_mode);
	ssize_t (*find_best_block_size)(struct so_drive * drive);
	int (*format_media)(struct so_drive * drive, struct so_pool * pool);
	struct so_stream_reader * (*get_raw_reader)(struct so_drive * drive, int file_position);
	struct so_stream_writer * (*get_raw_writer)(struct so_drive * drive);
	struct so_format_reader * (*get_reader)(struct so_drive * drive, int file_position, struct so_value * checksums);
	struct so_format_writer * (*get_writer)(struct so_drive * drive, struct so_value * checksums);
	void (*release)(struct so_drive * drive);
	int (*sync)(struct so_drive * drive);
};

#endif

