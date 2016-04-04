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

#ifndef __SO_TAPEDRIVE_FORMAT_LTFS_H__
#define __SO_TAPEDRIVE_FORMAT_LTFS_H__

// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>
// time_t
#include <time.h>

struct so_archive;
struct so_database_connection;
struct so_drive;
struct so_media;
struct so_pool;
struct so_value;
struct sodr_tape_drive_ltfs_volume_coherency;
struct sodr_tape_drive_media;
struct sodr_tape_drive_scsi_position;

struct so_value * sodr_tape_drive_format_ltfs_convert_index(struct so_media * media, const char * previous_partition, struct sodr_tape_drive_ltfs_volume_coherency * previous_vcr, struct sodr_tape_drive_scsi_position * current_position);
unsigned int sodr_tape_drive_format_ltfs_count_archives(struct so_media * media);
unsigned int sodr_tape_drive_format_ltfs_count_files(struct so_value * index);
struct so_value * sodr_tape_drive_format_ltfs_create_label(time_t time, const char * uuid, size_t block_size, const char * partition);
struct so_value * sodr_tape_drive_format_ltfs_create_empty_fs(time_t time, const char * uuid, const char * partition, unsigned long long position, const char * previous_partiton, unsigned long long previous_position);
int sodr_tape_drive_format_ltfs_format_media(struct so_drive * drive, int fd, int scsi_fd, struct so_pool * pool, struct so_value * option, struct so_database_connection * db);
struct so_format_reader * sodr_tape_drive_format_ltfs_new_reader(struct so_drive * drive, int fd, int scsi_fd);
struct so_format_writer * sodr_tape_drive_format_ltfs_new_writer(struct so_drive * drive, int fd, int scsi_fd);
struct so_archive * sodr_tape_drive_format_ltfs_parse_archive(struct so_drive * drive, const bool * const disconnected, struct so_value * checksums, struct so_database_connection * db);
void sodr_tape_drive_format_ltfs_parse_index(struct sodr_tape_drive_media * mp, struct so_value * index, struct so_archive * archive, struct so_database_connection * db_connect);
time_t sodr_tape_drive_format_ltfs_parse_time(const char * date);
int sodr_tape_drive_format_ltfs_update_mam(int scsi_fd, struct so_drive * drive, struct so_database_connection * db);
int sodr_tape_drive_format_ltfs_remove_mam(int scsi_fd, struct so_drive * drive, struct so_database_connection * db);
int sodr_tape_drive_format_ltfs_update_volume_coherency_info(int scsi_fd, struct so_drive * drive, const char * uuid, unsigned int part, struct sodr_tape_drive_ltfs_volume_coherency * vol_coherency, struct so_database_connection * db);

#endif

