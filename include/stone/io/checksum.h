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
*  Last modified: Mon, 09 Jan 2012 13:13:37 +0100                         *
\*************************************************************************/

#ifndef __STONE_IO_CHECKSUM_H__
#define __STONE_IO_CHECKSUM_H__

#include <stone/io.h>

char ** st_checksum_get_digest_from_writer(struct st_stream_writer * writer);
struct st_stream_reader * st_checksum_get_steam_reader(const char ** checksums, unsigned int nb_checksums, struct st_stream_reader * reader);
struct st_stream_writer * st_checksum_get_steam_writer(const char ** checksums, unsigned int nb_checksums, struct st_stream_writer * writer);

#endif

