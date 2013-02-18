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
*  Last modified: Sat, 08 Dec 2012 15:18:59 +0100                            *
\****************************************************************************/

// free, realloc
#include <stdlib.h>
// strlen, strspn, strstr
#include <string.h>

#include <libstone/util/string.h>

/**
 * \brief Compute size of a UTF8 character
 *
 * \param[in] string : a string which contains one or more UTF8 characters
 * \returns size of first character or \b -1
 */
static int st_util_string_valid_utf8_char(const char * string);

bool st_util_string_check_valid_utf8(const char * string) {
	if (string == NULL)
		return false;

	const char * ptr = string;
	while (*ptr) {
		int size = st_util_string_valid_utf8_char(ptr);

		if (!size)
			return false;

		ptr += size;
	}

	return true;
}

/**
 * sdbm function
 * http://www.cse.yorku.ca/~oz/hash.html
 **/
uint64_t st_util_string_compute_hash(const void * key) {
	const char * cstr = key;
	uint64_t hash = 0;
	int length = strlen(cstr), i;
	for (i = 0; i < length; i++)
		hash = cstr[i] + (hash << 6) + (hash << 16) - hash;
	return hash;
}

void st_util_string_delete_double_char(char * str, char delete_char) {
	char double_char[3] = { delete_char, delete_char, '\0' };

	char * ptr = strstr(str, double_char);
	while (ptr != NULL) {
		ptr++;

		size_t length = strlen(ptr);
		size_t delete_length = strspn(ptr, double_char + 1);

		memmove(ptr, ptr + delete_length, length - delete_length + 1);

		ptr = strstr(ptr, double_char);
	}
}

void st_util_string_fix_invalid_utf8(char * string) {
	if (string == NULL)
		return;

	char * ptr = string;
	while (*ptr) {
		int size = st_util_string_valid_utf8_char(ptr);

		if (size > 0) {
			ptr += size;
			continue;
		}

		char * ptr_end = ptr + 1;
		while (*ptr_end && st_util_string_valid_utf8_char(ptr_end) == 0)
			ptr_end++;

		if (*ptr_end)
			memmove(ptr, ptr_end, strlen(ptr_end) + 1);
		else
			*ptr = '\0';
	}
}

void st_util_string_trim(char * str, char trim) {
	size_t length = strlen(str);

	char * ptr;
	for (ptr = str; *ptr == trim; ptr++);

	if (str < ptr) {
		length -= ptr - str;
		if (length == 0) {
			*str = '\0';
			return;
		}
		memmove(str, ptr, length + 1);
	}

	for (ptr = str + (length - 1); *ptr == trim && ptr > str; ptr--);

	if (ptr[1] != '\0')
		ptr[1] = '\0';
}

void st_util_string_rtrim(char * str, char trim) {
	size_t length = strlen(str);

	char * ptr;
	for (ptr = str + (length - 1); *ptr == trim && ptr > str; ptr--);

	if (ptr[1] != '\0')
		ptr[1] = '\0';
}

static int st_util_string_valid_utf8_char(const char * string) {
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

