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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Sat, 08 Dec 2012 15:13:27 +0100                            *
\****************************************************************************/

#ifndef __STONE_UTIL_STRING_H__
#define __STONE_UTIL_STRING_H__

// bool
#include <stdbool.h>
// uint64_t
#include <stdint.h>

/**
 * \brief Check if \a string is a valid utf8 string
 *
 * \param[in] string : a utf8 string
 * \returns \b 1 if ok else 0
 */
bool st_util_string_check_valid_utf8(const char * string);

/**
 * \brief Compute hash of key
 *
 * \param[in] key : a c string
 * \returns computed hash
 *
 * \see st_hashtable_new
 */
uint64_t st_util_string_compute_hash(const void * key);

/**
 * \brief Remove from \a str a sequence of two or more of character \a delete_char
 *
 * \param[in,out] str : a string
 * \param[in] delete_char : a character
 */
void st_util_string_delete_double_char(char * str, char delete_char);

/**
 * \brief Fix a UTF8 string by removing invalid character
 *
 * \param[in,out] string : a (in)valid UTF8 string
 */
void st_util_string_fix_invalid_utf8(char * string);

/**
 * \brief Remove characters \a trim at the beginning and at the end of \a str
 *
 * \param[in,out] str : a string
 * \param[in] trim : a character
 */
void st_util_string_trim(char * str, char trim);

/**
 * \brief Remove characters \a trim at the end of \a str
 *
 * \param[in,out] str : a string
 * \param[in] trim : a character
 *
 * \see st_util_string_trim
 */
void st_util_string_rtrim(char * str, char trim);

#endif

