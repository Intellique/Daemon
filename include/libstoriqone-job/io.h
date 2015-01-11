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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

#ifndef __LIBSTORIQONE_JOB_IO_H__
#define __LIBSTORIQONE_JOB_IO_H__

struct so_value;

#include <libstoriqone/io.h>

struct so_stream_reader * soj_checksum_reader_new(struct so_stream_reader * stream, struct so_value * checksums, bool thread_helper);
struct so_stream_writer * soj_checksum_writer_new(struct so_stream_writer * stream, struct so_value * checksums, bool thread_helper);
struct so_value * soj_checksum_reader_get_checksums(struct so_stream_reader * stream);
struct so_value * soj_checksum_writer_get_checksums(struct so_stream_writer * stream);

#endif

