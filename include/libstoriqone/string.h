/****************************************************************************\
*                    ______           _      ____                            *
*                   / __/ /____  ____(_)__ _/ __ \___  ___                   *
*                  _\ \/ __/ _ \/ __/ / _ `/ /_/ / _ \/ -_)                  *
*                 /___/\__/\___/_/ /_/\_, /\____/_//_/\__/                   *
*                                      /_/                                   *
*  ------------------------------------------------------------------------  *
*  This file is a part of Storiq One                                         *
*                                                                            *
*  Storiq One is free software; you can redistribute it and/or modify        *
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

#ifndef __LIBSTORIQONE_STRING_H__
#define __LIBSTORIQONE_STRING_H__

struct so_value;

// bool
#include <stdbool.h>
// size_t
#include <sys/types.h>

/**
 * \brief Check if \a string is a valid utf8 string
 *
 * \param[in] string : a utf8 string
 * \returns \b 1 if ok else 0
 */
bool so_string_check_valid_utf8(const char * string);

/**
 * \brief Compute hash of key
 *
 * \param[in] key : a c string
 * \returns computed hash
 *
 * \see so_hashtable_new
 */
unsigned long long so_string_compute_hash(const struct so_value * value);
unsigned long long so_string_compute_hash2(const char * value);

/**
 * \brief Remove from \a str a sequence of two or more of character \a delete_char
 *
 * \param[in,out] str : a string
 * \param[in] delete_char : a character
 */
void so_string_delete_double_char(char * str, char delete_char);

bool so_string_convert_unicode_to_utf8(unsigned int unicode, char * string, size_t length, bool end_string);

void so_string_middle_elipsis(char * string, size_t length);

/**
 * \brief Remove characters \a trim at the end of \a str
 *
 * \param[in,out] str : a string
 * \param[in] trim : a character
 *
 * \see so_util_string_trim
 */
void so_string_rtrim(char * str, char trim);

/**
 * \brief Remove characters \a trim at the beginning and at the end of \a str
 *
 * \param[in,out] str : a string
 * \param[in] trim : a character
 */
void so_string_trim(char * str, char trim);

size_t so_string_unicode_length(unsigned int unicode);

#endif

