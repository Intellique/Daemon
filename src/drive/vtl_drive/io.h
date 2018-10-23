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

#ifndef __SO_VTLDRIVE_IO_H__
#define __SO_VTLDRIVE_IO_H__

#include <libstoriqone/io.h>

struct so_drive;

struct sodr_vtl_drive_io {
	int fd;

	char * buffer;
	ssize_t buffer_used;
	ssize_t buffer_length;

	ssize_t position;
	int file_position;
	ssize_t file_size;
	int last_errno;

	struct so_media * media;
};

struct so_stream_reader * sodr_vtl_drive_reader_get_raw_reader(int fd, int file_position);
struct so_stream_writer * sodr_vtl_drive_writer_get_raw_writer(const char * filename, int file_position);

int sodr_vtl_drive_io_close(struct sodr_vtl_drive_io * io);
void sodr_vtl_drive_io_free(struct sodr_vtl_drive_io * io);

#endif
