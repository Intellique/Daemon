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

#ifndef __LIBSTONE_STRING_P_H__
#define __LIBSTONE_STRING_P_H__

#include <libstone/string.h>

#include "value.h"

bool st_string_check_valid_utf8_v1(const char * string);
unsigned long long st_string_compute_hash_v1(const struct st_value_v1 * value);
bool st_string_convert_unicode_to_utf8_v1(unsigned int unicode, char * string, size_t length, bool end_string);
void st_string_delete_double_char_v1(char * str, char delete_char);
void st_string_middle_elipsis_v1(char * string, size_t length);
void st_string_rtrim_v1(char * str, char trim);
void st_string_trim_v1(char * str, char trim);
size_t st_string_unicode_length_v1(unsigned int unicode);

#endif

