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
// off_t
#include <sys/types.h>

#include <libstoriqone/format.h>
#include <libstoriqone/media.h>

struct so_database_connection;
struct so_drive;
struct so_media;

struct sodr_tape_drive_media {
	enum sodr_tape_drive_media_format {
		sodr_tape_drive_media_storiq_one = 0x1,
		sodr_tape_drive_media_ltfs       = 0x2,

		sodr_tape_drive_media_unknown = 0x0,
	} format;

	union {
		struct {
		} storiq_one;

		struct sodr_tape_drive_format_ltfs {
			char owner_identifier[15];

			struct sodr_tape_drive_format_ltfs_file {
				struct so_format_file file;
				struct sodr_tape_drive_format_ltfs_extent {
					/**
					 * \brief Partition that contains the data extent comprising this extent.
					 */
					unsigned int partition;
					/**
					 * \brief Start block number
					 *
					 * Block number within the data extent where the content for this extent begins.
					 */
					off_t start_block;
					/**
					 * \brief Offset to first valid byte
					 *
					 * Number of bytes from the beginning of the start block to the beginning of file data for this extent.
					 * This value shall be strictly less than the size of the start block.
					 */
					off_t byte_offset;
					/**
					 * \brief Number of bytes of file content in this data extent.
					 */
					size_t byte_count;
					/**
					 * \brief Number of bytes from the beginning of the file to the beginning of the file data recorded in this extent.
					 */
					off_t file_offset;
				} * extents;
				unsigned int nb_extents;

				struct sodr_tape_drive_format_ltfs_file * next;
			} * first_file, * last_file;
			unsigned int nb_files;
		} ltfs;
	} data;
};

bool sodr_tape_drive_media_check_header(struct so_media * media, const char * buffer);
void sodr_tape_drive_media_free(struct sodr_tape_drive_media * media_data);
void sodr_tape_drive_media_free2(void * private_data);
struct sodr_tape_drive_media * sodr_tape_drive_media_new(enum sodr_tape_drive_media_format format);
enum sodr_tape_drive_media_format sodr_tape_drive_parse_label(const char * buffer);
int sodr_tape_drive_media_parse_ltfs_index(struct so_drive * drive, struct so_database_connection * db_connect);
int sodr_tape_drive_media_parse_ltfs_label(struct so_drive * drive, struct so_database_connection * db_connect);

#endif

