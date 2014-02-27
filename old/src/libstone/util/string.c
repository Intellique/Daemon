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
*  Last modified: Sun, 09 Jun 2013 16:21:20 +0200                            *
\****************************************************************************/

// free, realloc
#include <stdlib.h>
// strlen, strspn, strstr
#include <string.h>

#include <libstone/util/string.h>

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

void st_util_string_middle_elipsis(char * string, size_t length) {
	size_t str_length = strlen(string);
	if (str_length <= length)
		return;

	length--;

	size_t used = 0;
	char * ptrA = string;
	char * ptrB = string + str_length;
	while (used < length) {
		int char_length = st_util_string_valid_utf8_char(ptrA);
		if (char_length == 0)
			return;

		if (used + char_length > length)
			break;

		used += char_length;
		ptrA += char_length;

		int offset = 1;
		while (char_length = st_util_string_valid_utf8_char(ptrB - offset), ptrA < ptrB - offset && char_length == 0)
			offset++;

		if (char_length == 0)
			return;

		if (used + char_length > length)
			break;

		used += char_length;
		ptrB -= char_length;
	}

	*ptrA = '~';
	memmove(ptrA + 1, ptrB, strlen(ptrB) + 1);
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

size_t st_util_string_strlen(const char * str) {
	size_t length = 0;
	size_t offset = 0;

	while (str[offset] != '\0') {
		int char_length = st_util_string_valid_utf8_char(str + offset);
		if (char_length == 0)
			break;

		length++;
		offset += char_length;
	}

	return length;
}

