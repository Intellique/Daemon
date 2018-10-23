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
*  Copyright (C) 2013-2018, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __SO_TAPEDRIVE_IO_IO_H__
#define __SO_TAPEDRIVE_IO_IO_H__

// bool
#include <stdbool.h>

#include <libstoriqone/io.h>

struct so_drive;
struct so_database_connection;
struct sodr_tape_drive_scsi_position;

int sodr_tape_drive_reader_close2(struct so_stream_reader * sr, bool close_fd);
int sodr_tape_drive_writer_close2(struct so_stream_writer * sw, bool close_fd);
ssize_t sodr_tape_drive_get_block_size(struct so_database_connection * db);
ssize_t sodr_tape_drive_writer_flush(struct so_stream_writer * sw);

struct so_stream_reader * sodr_tape_drive_reader_get_raw_reader(struct so_drive * drive, int fd, int partition, int file_position, struct so_database_connection * db);
struct so_stream_reader * sodr_tape_drive_reader_get_raw_reader2(struct so_drive * drive, int fd, int scsi_fd, struct sodr_tape_drive_scsi_position * position);
struct so_stream_writer * sodr_tape_drive_writer_get_raw_writer(struct so_drive * drive, int fd, int partition, int file_position, struct so_database_connection * db);
struct so_stream_writer * sodr_tape_drive_writer_get_raw_writer2(struct so_drive * drive, int fd, int partition, int file_position, bool fill_last_block, struct so_database_connection * db);

#endif
