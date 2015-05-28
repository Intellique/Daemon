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

#ifndef __SO_TAPEDRIVE_MEDIA_H__
#define __SO_TAPEDRIVE_MEDIA_H__

// bool
#include <stdbool.h>

#include <libstoriqone/media.h>

struct so_database_connection;
struct so_drive;
struct so_media;

struct sodr_tape_drive_media {
	enum sodr_tape_drive_media_format {
		sodr_tape_drive_media_storiq_one,
		sodr_tape_drive_media_ltfs,
		sodr_tape_drive_media_unknown,
	} format;

	union {
		struct {
		} storiq_one;

		struct {
			struct so_format_file * files;
			unsigned int nb_files;
		} ltfs;
	} data;
};

bool sodr_tape_drive_media_check_header(struct so_media * media, const char * buffer);
void sodr_tape_drive_media_free(struct sodr_tape_drive_media * media_data);
struct sodr_tape_drive_media * sodr_tape_drive_media_new(enum sodr_tape_drive_media_format format);
enum sodr_tape_drive_media_format sodr_tape_drive_parse_label(const char * buffer);
int sodr_tape_drive_media_parse_ltfs_index(struct so_drive * drive, struct so_database_connection * db_connect);
int sodr_tape_drive_media_parse_ltfs_label(struct so_drive * drive, struct so_database_connection * db_connect);

#endif

