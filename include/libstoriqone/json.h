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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_JSON_H__
#define __LIBSTORIQONE_JSON_H__

// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>

struct so_stream_reader;
struct so_value;

ssize_t so_json_encode_to_fd(struct so_value * value, int fd, bool use_buffer);
ssize_t so_json_encode_to_file(struct so_value * value, const char * filename);
char * so_json_encode_to_string(struct so_value * value);
struct so_value * so_json_parse_fd(int fd, int timeout);
struct so_value * so_json_parse_file(const char * file);
struct so_value * so_json_parse_stream(struct so_stream_reader * reader);
struct so_value * so_json_parse_string(const char * json);

#endif
