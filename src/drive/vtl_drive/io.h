/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

#ifndef __ST_VTLDRIVE_IO_H__
#define __ST_VTLDRIVE_IO_H__

#include <libstone/io.h>

struct st_drive;

struct vtl_drive_io {
	int fd;

	char * buffer;
	ssize_t buffer_used;
	ssize_t buffer_length;

	ssize_t position;
	int last_errno;

	struct st_media * media;
};

struct st_stream_reader * vtl_drive_reader_get_raw_reader(struct st_drive * drive, int fd);
struct st_stream_writer * vtl_drive_writer_get_raw_writer(struct st_drive * drive, const char * filename);

#endif
