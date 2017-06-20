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

#ifndef __LIBSTORIQONE_JOB_IO_H__
#define __LIBSTORIQONE_JOB_IO_H__

#include <libstoriqone/io.h>

struct so_format_reader * soj_io_filesystem_reader(const char * path);
struct so_format_writer * soj_io_filesystem_writer(const char * path);

void soj_format_writer_write_async(struct so_format_writer * fw, const void * buffer, ssize_t length);
ssize_t soj_format_writer_write_return(struct so_format_writer * sw);
void soj_stream_writer_write_async(struct so_stream_writer * sw, const void * buffer, ssize_t length);
ssize_t soj_stream_writer_write_return(struct so_stream_writer * sw);

#endif
