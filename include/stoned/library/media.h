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
*  Last modified: Tue, 31 Jul 2012 22:42:16 +0200                         *
\*************************************************************************/

#ifndef __STONED_LIBRARY_MEDIA_H__
#define __STONED_LIBRARY_MEDIA_H__

#include <libstone/library/media.h>

struct st_media * st_media_get_by_label(const char * label);
struct st_media * st_media_get_by_medium_serial_number(const char * medium_serial_number);
struct st_media * st_media_get_by_uuid(const char * uuid);
struct st_media * st_media_new(struct st_drive * dr);
int st_media_write_header(struct st_drive * dr, struct st_pool * pool);

struct st_media_format * st_media_format_get_by_density_code(unsigned char density_code, enum st_media_format_mode mode);

struct st_pool * st_pool_get_by_id(long id);
struct st_pool * st_pool_get_by_uuid(const char * uuid);
int st_pool_sync(struct st_pool * pool);

#endif

