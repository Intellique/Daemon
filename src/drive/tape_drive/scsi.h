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

#ifndef __SO_TAPEDRIVE_SCSI_H__
#define __SO_TAPEDRIVE_SCSI_H__

// bool
#include <stdbool.h>
// off_t
#include <sys/types.h>

struct so_drive;
struct so_media_format;

struct sodr_tape_drive_scsi_position {
	unsigned int partition;
	off_t block_position;
	bool end_of_partition;
};

bool sodr_tape_drive_scsi_check_drive(struct so_drive * drive, const char * path);
bool sodr_tape_drive_scsi_check_support(struct so_media_format * format, bool for_writing, const char * path);
int sodr_tape_drive_scsi_erase_media(const char * path, bool quick_mode);
/**
 * \brief Set position on tape
 * \remark Require LTO-4 drive at least
 */
int sodr_tape_drive_scsi_locate16(int fd, struct sodr_tape_drive_scsi_position * position);
int sodr_tape_drive_scsi_read_density(struct so_drive * drive, const char * path);
int sodr_tape_drive_scsi_read_position(int fd, struct sodr_tape_drive_scsi_position * position);
int sodr_tape_drive_scsi_read_medium_serial_number(int fd, char * medium_serial_number, size_t length);
int sodr_tape_drive_scsi_read_mam(int fd, struct so_media * media);
int sodr_tape_drive_scsi_rewind(int fd);
int sodr_tape_drive_scsi_size_available(int fd, struct so_media * media);

#endif

