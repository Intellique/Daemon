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

#ifndef __LIBSTONE_FILE_P_H__
#define __LIBSTONE_FILE_P_H__

#include <libstone/file.h>

int st_file_basic_scandir_filter_v1(const struct dirent * d);
bool st_file_check_link_v1(const char * file) __attribute__((nonnull));
void st_file_convert_mode_v1(char * buffer, mode_t mode) __attribute__((nonnull));
void st_file_convert_size_to_string_v1(size_t size, char * str, ssize_t str_len) __attribute__((nonnull));
int st_file_cp_v1(const char * src, const char * dst) __attribute__((nonnull));
int st_file_mkdir_v1(const char * dirname, mode_t mode) __attribute__((nonnull));
int st_file_mv_v1(const char * src, const char * dst) __attribute__((nonnull));
char * st_file_read_all_from_v1(const char * filename) __attribute__((nonnull));
char * st_file_rename_v1(const char * filename) __attribute__((nonnull));
int st_file_rm_v1(const char * path) __attribute__((nonnull));

#endif

