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

#ifndef __LIBSTONE_JSON_H__
#define __LIBSTONE_JSON_H__

// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>

struct st_value;

ssize_t st_json_encode_to_fd(struct st_value * value, int fd, bool use_buffer);
ssize_t st_json_encode_to_file(struct st_value * value, const char * filename);
char * st_json_encode_to_string(struct st_value * value);
struct st_value * st_json_parse_fd(int fd, int timeout);
struct st_value * st_json_parse_file(const char * file);
struct st_value * st_json_parse_string(const char * json);

#endif
