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
*  Copyright (C) 2013-2019, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_JOB_IO2_H__
#define __LIBSTORIQONE_JOB_IO2_H__

#include <libstoriqone-job/io.h>

struct so_drive;
struct so_value;

struct so_format_reader * soj_format_new_reader(struct so_drive * drive, struct so_value * config);
struct so_format_reader * soj_format_new_reader2(struct so_drive * drive, int fd_command, int fd_data, ssize_t block_size);
struct so_format_writer * soj_format_new_writer(struct so_drive * drive, struct so_value * config);

struct so_stream_reader * soj_stream_new_reader(struct so_drive * drive, struct so_value * config);
struct so_stream_reader * soj_stream_new_reader2(struct so_drive * drive, int fd_command, int fd_data, ssize_t block_size);
struct so_stream_writer * soj_stream_new_writer(struct so_drive * drive, struct so_value * config);

#endif
