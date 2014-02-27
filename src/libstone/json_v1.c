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

// bool
#include <stdbool.h>
// open
#include <fcntl.h>
// sscanf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strspn, strncmp
#include <string.h>
// fstat, open
#include <sys/stat.h>
// fstat, open
#include <sys/types.h>
// access, close, fstat, read, readlink, unlink, write
#include <unistd.h>

#include "json_v1.h"
#include "string_v1.h"

struct st_json_parser {
	const char * json;
	const char * position;
};

static struct st_value * st_json_parse_string_inner(struct st_json_parser * parser);
static void st_json_parse_string_strip(struct st_json_parser * parser);


__asm__(".symver st_json_parse_file_v1, st_json_parse_file@@LIBSTONE_1.0");
struct st_value * st_json_parse_file_v1(const char * file) {
	if (file == NULL)
		return NULL;

	if (access(file, R_OK))
		return NULL;

	int fd = open(file, O_RDONLY);

	struct stat st;
	fstat(fd, &st);

	char * buffer = malloc(st.st_size + 1);
	ssize_t nb_read = read(fd, buffer, st.st_size);
	close(fd);
	buffer[nb_read] = '\0';

	if (nb_read < 0) {
		free(buffer);
		return NULL;
	}

	struct st_value * ret_value = st_json_parse_string_v1(buffer);
	free(buffer);

	return ret_value;
}

__asm__(".symver st_json_parse_string_v1, st_json_parse_string@@LIBSTONE_1.0");
struct st_value * st_json_parse_string_v1(const char * json) {
	struct st_json_parser parser = { .json = json, .position = json };
	return st_json_parse_string_inner(&parser);
}

static struct st_value * st_json_parse_string_inner(struct st_json_parser * parser) {
	st_json_parse_string_strip(parser);

	struct st_value * ret_val = NULL;
	switch (*parser->position) {
		case '{':
			parser->position++;
			ret_val = st_value_new_hashtable_v1(st_string_compute_hash_v1);

			st_json_parse_string_strip(parser);
			if (*parser->position == '}') {
				parser->position++;
				return ret_val;
			}

			for (;;) {
				if (*parser->position != '"') {
					st_value_free_v1(ret_val);
					return NULL;
				}

				struct st_value * key = st_json_parse_string_inner(parser);
				if (key == NULL || key->type != st_value_string) {
					if (key != NULL)
						st_value_free_v1(key);
					st_value_free_v1(ret_val);
					return NULL;
				}

				st_json_parse_string_strip(parser);

				if (*parser->position != ':') {
					st_value_free_v1(ret_val);
					st_value_free_v1(key);
					return NULL;
				} else
					parser->position++;

				st_json_parse_string_strip(parser);

				struct st_value * value = st_json_parse_string_inner(parser);
				if (value == NULL) {
					st_value_free_v1(ret_val);
					st_value_free_v1(key);
					return NULL;
				}

				st_value_hashtable_put_v1(ret_val, key, true, value, true);

				st_json_parse_string_strip(parser);

				if (*parser->position == '}') {
					parser->position++;
					return ret_val;
				} else if (*parser->position != ',') {
					st_value_free_v1(ret_val);
					return NULL;
				}

				parser->position++;
				st_json_parse_string_strip(parser);
			}
			break;

		case '[':
			parser->position++;
			ret_val = st_value_new_linked_list_v1();

			st_json_parse_string_strip(parser);
			if (*parser->position == ']') {
				parser->position++;
				return ret_val;
			}

			for (;;) {
				struct st_value * value = st_json_parse_string_inner(parser);
				if (value == NULL) {
					st_value_free_v1(ret_val);
					return NULL;
				}

				st_value_list_push_v1(ret_val, value, true);

				st_json_parse_string_strip(parser);

				if (*parser->position == ']') {
					parser->position++;
					return ret_val;
				} else if (*parser->position != ',') {
					st_value_free_v1(ret_val);
					return NULL;
				}

				parser->position++;
				st_json_parse_string_strip(parser);
			}
			break;

		case '"': {
				parser->position++;
				const char * string = parser->position;

				size_t length;
				unsigned int unicode;
				for (length = 0; *parser->position != '"'; parser->position++, length++) {
					if (*parser->position == '\\') {
						parser->position++;

						if (strspn(parser->position, "\"\\/bfnrtu") == 0)
							return NULL;

						if (*parser->position == 'u') {
							parser->position++;

							if (sscanf(parser->position, "%4x", &unicode) < 1)
								return NULL;
							size_t char_length = st_string_unicode_length_v1(unicode);

							if (char_length == 0)
								return NULL;

							parser->position += 3;
							length += char_length - 1;
						}
					}
				}
				parser->position++;

				char * tmp_string = malloc(length + 1);
				size_t from, to;
				for (from = 0, to = 0; to < length; from++, to++) {
					if (string[from] == '\\') {
						from++;
						switch (string[from]) {
							case '"':
							case '\\':
							case '/':
								tmp_string[to] = string[from];
								break;

							case 'b':
								tmp_string[to] = '\b';
								break;

							case 'f':
								tmp_string[to] = '\f';
								break;

							case 'n':
								tmp_string[to] = '\n';
								break;

							case 'r':
								tmp_string[to] = '\r';
								break;

							case 't':
								tmp_string[to] = '\t';
								break;

							case 'u':
								from++;
								sscanf(string + from, "%4x", &unicode);
								from += 3;

								st_string_convert_unicode_to_utf8_v1(unicode, tmp_string + to, length - to, false);
								to += st_string_unicode_length_v1(unicode) - 1;
								break;
						}
					} else
						tmp_string[to] = string[from];
				}
				tmp_string[to] = '\0';

				ret_val = st_value_new_string_v1(tmp_string);
				free(tmp_string);
				return ret_val;
			}
			break;

		case '-':
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9': {
				double val_d;
				long long int val_i;
				int nb_parsed_d, nb_parsed_i;

				sscanf(parser->position, "%le%n", &val_d, &nb_parsed_d);
				sscanf(parser->position, "%Ld%n", &val_i, &nb_parsed_i);

				if (nb_parsed_d == nb_parsed_i)
					ret_val = st_value_new_integer_v1(val_i);
				else
					ret_val = st_value_new_float_v1(val_d);

				parser->position += nb_parsed_d;

				return ret_val;
			}

		case 't':
			if (strncmp(parser->position, "true", 4) == 0) {
				parser->position += 4;
				return st_value_new_boolean_v1(true);
			}
			break;

		case 'f':
			if (strncmp(parser->position, "false", 5) == 0) {
				parser->position += 5;
				return st_value_new_boolean_v1(false);
			}
			break;

		case 'n':
			if (strncmp(parser->position, "null", 4) == 0) {
				parser->position += 4;
				return st_value_new_null_v1();
			}
			break;
	}

	return NULL;
}

static void st_json_parse_string_strip(struct st_json_parser * parser) {
	size_t length = strspn(parser->position, " \t\n");
	if (length > 0)
		parser->position += length;
}

