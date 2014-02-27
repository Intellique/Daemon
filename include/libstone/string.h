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

#ifndef __LIBSTONE_STRING_H__
#define __LIBSTONE_STRING_H__

struct st_value;

// bool
#include <stdbool.h>

/**
 * \brief Check if \a string is a valid utf8 string
 *
 * \param[in] string : a utf8 string
 * \returns \b 1 if ok else 0
 */
bool st_string_check_valid_utf8(const char * string);

/**
 * \brief Compute hash of key
 *
 * \param[in] key : a c string
 * \returns computed hash
 *
 * \see st_hashtable_new
 */
unsigned long long st_string_compute_hash(const struct st_value * value);

bool st_string_convert_unicode_to_utf8(unsigned int unicode, char * string, size_t length, bool end_string);

size_t st_string_unicode_length(unsigned int unicode);

#endif

