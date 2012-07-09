/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Fri, 06 Jul 2012 10:26:04 +0200                         *
\*************************************************************************/

#ifndef __STONE_IO_JSON_H__
#define __STONE_IO_JSON_H__

#include <stone/io.h>

struct st_archive;
struct st_archive_file;
struct st_archive_volume;
struct st_hashtable;
struct st_io_json;

void st_io_json_add_file(struct st_io_json * js, struct st_archive_file * file);
void st_io_json_add_metadata(struct st_io_json * js, struct st_hashtable * metadatas);
void st_io_json_add_volume(struct st_io_json * js, struct st_archive_volume * volume);
void st_io_json_free(struct st_io_json * js);
struct st_io_json * st_io_json_new(struct st_archive * archive);
void st_io_json_update_archive(struct st_io_json * js, struct st_archive * archive);
void st_io_json_update_volume(struct st_io_json * js, struct st_archive_volume * volume);
ssize_t st_io_json_write_to(struct st_io_json * js, struct st_stream_writer * writer);

#endif

