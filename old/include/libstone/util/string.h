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
*  Last modified: Sun, 09 Jun 2013 10:44:37 +0200                            *
\****************************************************************************/

#ifndef __STONE_UTIL_STRING_H__
#define __STONE_UTIL_STRING_H__

// bool
#include <stdbool.h>
// uint64_t
#include <stdint.h>

/**
 * \brief Fix a UTF8 string by removing invalid character
 *
 * \param[in,out] string : a (in)valid UTF8 string
 */
void st_util_string_fix_invalid_utf8(char * string);

void st_util_string_middle_elipsis(char * string, size_t length);

/**
 * \brief Compute length in characters (not in bytes)
 * \param[in] str valid utf8 string
 * \return length in characters
 * \note to compute length in bytes, use strlen
 */
size_t st_util_string_strlen(const char * str);

#endif

