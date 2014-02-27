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

// NULL
#include <stddef.h>
// strlen
#include <string.h>

#include "string_v1.h"
#include "value_v1.h"


/**
 * \brief Compute size of a UTF8 character
 *
 * \param[in] string : a string which contains one or more UTF8 characters
 * \returns size of first character or \b -1
 */
static int st_string_valid_utf8_char(const char * string);

__asm__(".symver st_string_check_valid_utf8_v1, st_string_check_valid_utf8@@LIBSTONE_1.0");
bool st_string_check_valid_utf8_v1(const char * string) {
	if (string == NULL)
		return false;

	const char * ptr = string;
	while (*ptr) {
		int size = st_string_valid_utf8_char(ptr);

		if (!size)
			return false;

		ptr += size;
	}

	return true;
}

__asm__(".symver st_string_compute_hash_v1, st_string_compute_hash@@LIBSTONE_1.0");
unsigned long long st_string_compute_hash_v1(const struct st_value * value) {
	if (value == NULL || value->type != st_value_string)
		return 0;

	unsigned long long hash = 0;
	int length = strlen(value->value.string), i;
	for (i = 0; i < length; i++)
		hash = value->value.string[i] + (hash << 6) + (hash << 16) - hash;
	return hash;
}

__asm__(".symver st_string_convert_unicode_to_utf8_v1, st_string_convert_unicode_to_utf8@@LIBSTONE_1.0");
bool st_string_convert_unicode_to_utf8_v1(unsigned int unicode, char * string, size_t length, bool end_string) {
	if (unicode < 0x80 && length > 1) {
		string[0] = unicode & 0x7F;
		if (end_string)
			string[1] = '\0';
		return true;
	} else if (unicode < 0x800 && length > 2) {
		string[0] = ((unicode & 0x7C0) >> 6) | 0xC0;
		string[1] = (unicode & 0x3F) | 0x80;
		if (end_string)
			string[2] = '\0';
		return true;
	} else if (unicode < 0x10000 && length > 3) {
		string[0] = ((unicode & 0xF000) >> 12) | 0xE0;
		string[1] = ((unicode & 0xFC0) >> 6) | 0x80;
		string[2] = (unicode & 0x3F) | 0x80;
		if (end_string)
			string[3] = '\0';
		return true;
	} else if (unicode < 0x200000 && length > 4) {
		string[0] = ((unicode & 0x1C0000) >> 18) | 0xF0;
		string[1] = ((unicode & 0x3F000) >> 12) | 0x80;
		string[2] = ((unicode & 0xFC0) >> 6) | 0x80;
		string[3] = (unicode & 0x3F) | 0x80;
		if (end_string)
			string[4] = '\0';
		return true;
	}

	return false;
}

__asm__(".symver st_string_unicode_length_v1, st_string_unicode_length@@LIBSTONE_1.0");
size_t st_string_unicode_length_v1(unsigned int unicode) {
	if (unicode < 0x80)
		return 1;
	else if (unicode < 0x800)
		return 2;
	else if (unicode < 0x10000)
		return 3;
	else if (unicode < 0x200000)
		return 4;

	return 0;
}

static int st_string_valid_utf8_char(const char * string) {
	const unsigned char * ptr = (const unsigned char *) string;
	if ((*ptr & 0x7F) == *ptr) {
		return 1;
	} else if ((*ptr & 0xBF) == *ptr) {
		return 0;
	} else if ((*ptr & 0xDF) == *ptr) {
		ptr++;
		if ((*ptr & 0xBF) != *ptr || (*ptr & 0x80) != 0x80)
			return 0;

		return 2;
	} else if ((*ptr & 0xEF) == *ptr) {
		ptr++;
		if ((*ptr & 0xBF) != *ptr || (*ptr & 0x80) != 0x80)
			return 0;

		ptr++;
		if ((*ptr & 0xBF) != *ptr || (*ptr & 0x80) != 0x80)
			return 0;

		return 3;
	} else if ((*ptr & 0x7F) == *ptr) {
		ptr++;
		if ((*ptr & 0xBF) != *ptr || (*ptr & 0x80) != 0x80)
			return 0;

		ptr++;
		if ((*ptr & 0xBF) != *ptr || (*ptr & 0x80) != 0x80)
			return 0;

		ptr++;
		if ((*ptr & 0xBF) != *ptr || (*ptr & 0x80) != 0x80)
			return 0;

		return 4;
	} else {
		return 0;
	}
}

