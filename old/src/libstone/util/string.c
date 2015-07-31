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

