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

// memmove, strlen, strspn, strstr
#include <string.h>

#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

#include "../../build/libstoriqone/unicode.h"
static unsigned int so_string_nb_top_characters = sizeof(so_string_characters) / sizeof(*so_string_characters);

static const struct so_string_character so_string_unknow_character = {
	NULL, NULL, so_string_character_category_other, so_string_character_subcategory_other_not_assigned, so_string_bidi_class_other_neutrals, false, 0
};


/**
 * \brief Compute size of a UTF8 character
 *
 * \param[in] string : a string which contains one or more UTF8 characters
 * \returns size of first character or \b -1
 */
static int so_string_valid_utf8_char(const char * string);
static int so_string_valid_utf8_char2(const unsigned char * ptr, unsigned short length);


bool so_string_check_valid_utf8(const char * string) {
	if (string == NULL)
		return false;

	const char * ptr = string;
	while (*ptr) {
		int size = so_string_valid_utf8_char(ptr);

		if (!size)
			return false;

		ptr += size;
	}

	return true;
}

unsigned long long so_string_compute_hash(const struct so_value * value) {
	if (value == NULL || value->type != so_value_string)
		return 0;

	return so_string_compute_hash2(so_value_string_get(value));
}

unsigned long long so_string_compute_hash2(const char * str) {
	if (str == NULL)
		return 0;

	unsigned long long hash = 0;
	int length = strlen(str), i;
	for (i = 0; i < length; i++)
		hash = str[i] + (hash << 6) + (hash << 16) - hash;
	return hash;
}

bool so_string_convert_unicode_to_utf8(unsigned int unicode, char * string, size_t length, bool end_string) {
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

unsigned int so_string_convert_utf8_to_unicode(const char * character) {
	size_t length = so_string_valid_utf8_char(character);
	if (length < 1)
		return 0;

	const unsigned char * uchar = (const unsigned char *) character;
	switch (length) {
		case 1:
			return uchar[0] & 0x7F;

		case 2:
			return ((uchar[0] & 0x1F) << 6) + (uchar[1] & 0x3F);

		case 3:
			return ((uchar[0] & 0x0F) << 12) + ((uchar[1] & 0x3F) << 6) + (uchar[2] & 0x3F);

		case 4:
			return ((uchar[0] & 0x07) << 18) + ((uchar[1] & 0x3F) << 12) + ((uchar[2] & 0x3F) << 6) + (uchar[3] & 0x3F);

		default:
			return 0;
	}
}

void so_string_delete_double_char(char * str, char delete_char) {
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

const struct so_string_character * so_string_get_character_info(unsigned int unicode_character) {
	unsigned int i0 = unicode_character / 0x8000;
	if (i0 >= so_string_nb_top_characters)
		return &so_string_unknow_character;

	const struct so_string_character *** c0 = so_string_characters[i0];
	if (c0 == NULL)
		return &so_string_unknow_character;

	unsigned int l0 = unicode_character & 0x7FFF;
	unsigned int i1 = l0 / 0x400;

	const struct so_string_character ** c1 = c0[i1];
	if (c1 == NULL)
		return &so_string_unknow_character;

	unsigned int l1 = l0 & 0x3FF;
	return c1[l1 / 0x20] + (l1 & 0x1F);
}

void so_string_middle_elipsis(char * string, size_t length) {
	size_t str_length = strlen(string);
	if (str_length <= length)
		return;

	length--;

	size_t used = 0;
	char * ptrA = string;
	char * ptrB = string + str_length;
	while (used < length) {
		int char_length = so_string_valid_utf8_char(ptrA);
		if (char_length == 0)
			return;

		if (used + char_length > length)
			break;

		used += char_length;
		ptrA += char_length;

		int offset = 1;
		while (char_length = so_string_valid_utf8_char(ptrB - offset), ptrA < ptrB - offset && char_length == 0)
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

void so_string_rtrim(char * str, char trim) {
	size_t length = strlen(str);

	char * ptr;
	for (ptr = str + (length - 1); *ptr == trim && ptr > str; ptr--);

	if (ptr[1] != '\0')
		ptr[1] = '\0';
}

void so_string_trim(char * str, char trim) {
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

void so_string_to_lowercase(char * str) {
	while (*str) {
		unsigned int unicode = so_string_convert_utf8_to_unicode(str);
		unsigned int length = so_string_unicode_length(unicode);
		const struct so_string_character * character = so_string_get_character_info(unicode);

		if (character->sub_category == so_string_character_subcategory_letter_uppercase) {
			unicode += character->offset;
			so_string_convert_unicode_to_utf8(unicode, str, length + 1, false);
		}

		if (length > 0)
			str += length;
		else
			break;
	}
}

void so_string_to_uppercase(char * str) {
	while (*str) {
		unsigned int unicode = so_string_convert_utf8_to_unicode(str);
		unsigned int length = so_string_unicode_length(unicode);
		const struct so_string_character * character = so_string_get_character_info(unicode);

		if (character->sub_category == so_string_character_subcategory_letter_lowercase) {
			unicode += character->offset;
			so_string_convert_unicode_to_utf8(unicode, str, length + 1, false);
		}

		if (length > 0)
			str += length;
		else
			break;
	}
}

size_t so_string_unicode_length(unsigned int unicode) {
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

static int so_string_valid_utf8_char(const char * string) {
	const unsigned char * ptr = (const unsigned char *) string;
	if ((*ptr & 0x7F) == *ptr)
		return 1;
	else if ((*ptr & 0xBF) == *ptr)
	  return 0;
	else if ((*ptr & 0xDF) == *ptr)
	  return ((ptr[1] & 0xBF) != ptr[1] || ((ptr[1] & 0x80) != 0x80)) ? 0 : 2;
	else if ((*ptr & 0xEF) == *ptr)
	  return so_string_valid_utf8_char2(ptr, 2) ? 0 : 3;
	else if ((*ptr & 0xF7) == *ptr)
	  return so_string_valid_utf8_char2(ptr, 3) ? 0 : 4;
	else
		return 0;
}

static int so_string_valid_utf8_char2(const unsigned char * ptr, unsigned short length) {
	unsigned short i;
	for (i = 1; i <= length; i++)
		if ((ptr[i] & 0xBF) != ptr[i] || ((ptr[i] & 0x80) != 0x80))
			return 1;
	return 0;
}

