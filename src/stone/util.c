/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Wed, 04 Jan 2012 12:25:44 +0100                         *
\*************************************************************************/

// free
#include <stdlib.h>
// snprintf
#include <stdio.h>
// strlen, strspn, strstr
#include <string.h>

#include <stone/util.h>

void st_util_convert_size_to_string(ssize_t size, char * str, ssize_t str_len) {
	unsigned short mult = 0;
	double tsize = size;

	while (tsize >= 1024 && mult < 4) {
		tsize /= 1024;
		mult++;
	}

	switch (mult) {
		case 0:
			snprintf(str, str_len, "%zd Bytes", size);
			break;
		case 1:
			snprintf(str, str_len, "%.1f KBytes", tsize);
			break;
		case 2:
			snprintf(str, str_len, "%.2f MBytes", tsize);
			break;
		case 3:
			snprintf(str, str_len, "%.3f GBytes", tsize);
			break;
		default:
			snprintf(str, str_len, "%.4f TBytes", tsize);
	}

	if (strchr(str, '.')) {
		char * ptrEnd = strchr(str, ' ');
		char * ptrBegin = ptrEnd - 1;
		while (*ptrBegin == '0')
			ptrBegin--;
		if (*ptrBegin == '.')
			ptrBegin--;

		if (ptrBegin + 1 < ptrEnd)
			memmove(ptrBegin + 1, ptrEnd, strlen(ptrEnd) + 1);
	}
}

/**
 * sdbm function
 **/
unsigned long long st_util_compute_hash_string(const void * key) {
	const char * cstr = key;
	unsigned long long int hash = 0;
	int length = strlen(cstr), i;
	for (i = 0; i < length; i++)
		hash = cstr[i] + (hash << 6) + (hash << 16) - hash;
	return hash;
}

void st_util_basic_free(void * key, void * value) {
	if (key && key == value) {
		free(key);
		return;
	}

	if (key)
		free(key);
	if (value)
		free(value);
}

void st_util_string_delete_double_char(char * str, char delete_char) {
	char double_char[3] = { delete_char, delete_char, '\0' };

	char * ptr = strstr(str, double_char);
	while (ptr) {
		ptr++;

		size_t length = strlen(ptr);
		size_t delete_length = strspn(ptr, double_char + 1);

		memmove(ptr, ptr + delete_length, length - delete_length);

		ptr = strstr(ptr, double_char);
	}
}

char ** st_util_string_justified(const char * str, unsigned int width, unsigned int * nb_lines) {
	char * str_dup = strdup(str);

	char ** words = 0;
	unsigned int nb_words = 0;

	char * ptr = strtok(str_dup, " ");
	while (ptr) {
		words = realloc(words, (nb_words + 1) * sizeof(char *));
		words[nb_words] = ptr;
		nb_words++;

		ptr = strtok(0, " ");
	}

	char ** lines = 0;
	*nb_lines = 0;
	unsigned int index_words = 0, index_words2 = 0;
	while (index_words < nb_words) {
		size_t current_length_line = 0;

		unsigned int nb_space = 0;
		while (index_words < nb_words) {
			size_t word_length = strlen(words[index_words]);

			if (current_length_line > 0)
				nb_space++;

			if (current_length_line + word_length + nb_space > width)
				break;

			current_length_line += word_length;
			index_words++;
		}

		double small_space = 1;
		if (current_length_line + nb_space > width * 0.82 || index_words < nb_words) {
			small_space = width - current_length_line;
			if (index_words - index_words2 > 2)
				small_space /= index_words - index_words2 - 1;
		}

		char * line = malloc(width + 1);
		strcpy(line, words[index_words2]);
		index_words2++;

		int total_spaces = 0;
		unsigned int i = 1;
		while (index_words2 < index_words) {
			int nb_space = i * small_space - total_spaces;
			total_spaces += nb_space;

			size_t current_line_length = strlen(line);
			memset(line + current_line_length, ' ', nb_space);
			line[current_line_length + nb_space] = '\0';

			strcat(line, words[index_words2]);

			index_words2++, i++;
		}

		lines = realloc(lines, (*nb_lines + 1) * sizeof(char *));
		lines[*nb_lines] = line;
		(*nb_lines)++;
	}

	free(str_dup);
	free(words);

	return lines;
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

