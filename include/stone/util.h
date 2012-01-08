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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sun, 08 Jan 2012 12:05:12 +0100                         *
\*************************************************************************/

#ifndef __STONE_UTIL_H__
#define __STONE_UTIL_H__

// ssize_t
#include <sys/types.h>

void st_util_convert_size_to_string(ssize_t size, char * str, ssize_t str_len);
unsigned long long st_util_compute_hash_string(const void * key);
void st_util_basic_free(void * key, void * value);
void st_util_string_delete_double_char(char * str, char delete_char);
char ** st_util_string_justified(const char * str, unsigned int width, unsigned int * nb_lines);
void st_util_string_trim(char * str, char trim);
void st_util_gid2name(char * name, ssize_t length, gid_t gid);
void st_util_uid2name(char * name, ssize_t length, uid_t uid);

#endif

