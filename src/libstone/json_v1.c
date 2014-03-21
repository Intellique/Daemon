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
// poll
#include <poll.h>
// dprintf, snprintf, sscanf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strspn, strncmp
#include <string.h>
// fstat, open
#include <sys/stat.h>
// fstat, open
#include <sys/types.h>
// access, close, fstat, read
#include <unistd.h>

#include "json_v1.h"
#include "string_v1.h"

static size_t st_json_compute_length(struct st_value * val);
static size_t st_json_encode_to_string_inner(struct st_value * val, char * buffer, size_t length);
static struct st_value * st_json_parse_string_inner(const char ** json);
static void st_json_parse_string_strip(const char ** json);


static size_t st_json_compute_length(struct st_value * val) {
	size_t nb_write = 0;
	switch (val->type) {
		case st_value_array:
		case st_value_linked_list: {
				nb_write = 1;

				struct st_value_iterator * iter = st_value_list_get_iterator_v1(val);
				while (st_value_iterator_has_next_v1(iter)) {
					struct st_value * elt = st_value_iterator_get_value_v1(iter, true);
					nb_write += st_json_compute_length(elt);
					st_value_free_v1(elt);

					if (st_value_iterator_has_next_v1(iter))
						nb_write++;
				}
				nb_write++;
				st_value_iterator_free_v1(iter);
			}
			break;

		case st_value_boolean:
			if (val->value.boolean)
				nb_write = 4;
			else
				nb_write = 5;
			break;

		case st_value_float: {
				char tmp[24];
				int length;
				snprintf(tmp, 24, "%g%n", val->value.floating, &length);
				nb_write = length;
			}
			break;

		case st_value_hashtable: {
				nb_write = 1;

				struct st_value_iterator * iter = st_value_hashtable_get_iterator_v1(val);
				while (st_value_iterator_has_next_v1(iter)) {
					struct st_value * key = st_value_iterator_get_key_v1(iter, false, true);
					struct st_value * elt = st_value_iterator_get_value_v1(iter, true);

					if (key->type == st_value_string) {
						nb_write += st_json_compute_length(key);
						nb_write++;
						nb_write += st_json_compute_length(elt);

						if (st_value_iterator_has_next_v1(iter))
							nb_write++;
					}

					st_value_free_v1(key);
					st_value_free_v1(elt);
				}
				nb_write++;
				st_value_iterator_free_v1(iter);
			}
			break;

		case st_value_integer: {
				char tmp[24];
				int length;
				snprintf(tmp, 24, "%Ld%n", val->value.integer, &length);
				nb_write = length;
			}
			break;

		case st_value_null:
			nb_write = 4;
			break;

		case st_value_string: {
				nb_write = 1;
				char * str = val->value.string;

				while (*str != 0) {
					int nb_printable = strcspn(str, "\"\\/\b\f\n\r\t");
					if (nb_printable > 0) {
						nb_write += nb_printable;
						str += nb_printable;

						if (*str == '\0')
							break;
					}

					switch (*str) {
						case '"':
						case '\\':
						case '/':
						case '\b':
						case '\f':
						case '\n':
						case '\r':
						case '\t':
							nb_write += 2;
							str++;
							break;
					}
				}

				nb_write++;
			}
			break;

		default:
			break;
	}

	return nb_write;
}

__asm__(".symver st_json_encode_to_fd_v1, st_json_encode_to_fd@@LIBSTONE_1.0");
size_t st_json_encode_to_fd_v1(struct st_value * val, int fd) {
	if (val == NULL)
		return 0;

	size_t nb_write = 0;
	switch (val->type) {
		case st_value_array:
		case st_value_linked_list: {
				nb_write = dprintf(fd, "[");

				struct st_value_iterator * iter = st_value_list_get_iterator_v1(val);
				while (st_value_iterator_has_next_v1(iter)) {
					struct st_value * elt = st_value_iterator_get_value_v1(iter, true);
					nb_write += st_json_encode_to_fd_v1(elt, fd);
					st_value_free_v1(elt);

					if (st_value_iterator_has_next_v1(iter))
						nb_write += dprintf(fd, ",");
				}
				nb_write += dprintf(fd, "]");
				st_value_iterator_free_v1(iter);
			}
			break;

		case st_value_boolean:
			if (val->value.boolean)
				nb_write = dprintf(fd, "true");
			else
				nb_write = dprintf(fd, "false");
			break;

		case st_value_float:
			nb_write = dprintf(fd, "%g", val->value.floating);
			break;

		case st_value_hashtable: {
				nb_write = dprintf(fd, "{");

				struct st_value_iterator * iter = st_value_hashtable_get_iterator_v1(val);
				while (st_value_iterator_has_next_v1(iter)) {
					struct st_value * key = st_value_iterator_get_key_v1(iter, false, true);
					struct st_value * elt = st_value_iterator_get_value_v1(iter, true);

					if (key->type == st_value_string) {
						nb_write += st_json_encode_to_fd_v1(key, fd);
						nb_write += dprintf(fd, ":");
						nb_write += st_json_encode_to_fd_v1(elt, fd);

						if (st_value_iterator_has_next_v1(iter))
							nb_write += dprintf(fd, ",");
					}

					st_value_free_v1(key);
					st_value_free_v1(elt);
				}
				nb_write += dprintf(fd, "}");
				st_value_iterator_free_v1(iter);
			}
			break;

		case st_value_integer:
			nb_write = dprintf(fd, "%Ld", val->value.integer);
			break;

		case st_value_null:
			nb_write = dprintf(fd, "null");
			break;

		case st_value_string: {
				nb_write = dprintf(fd, "\"");
				char * str = val->value.string;

				while (*str != 0) {
					int nb_printable = strcspn(str, "\"\\/\b\f\n\r\t");
					if (nb_printable > 0) {
						nb_write += dprintf(fd, "%.*s", nb_printable, str);
						str += nb_printable;

						if (*str == '\0')
							break;
					}

					switch (*str) {
						case '"':
						case '\\':
						case '/':
							nb_write += dprintf(fd, "\\%c", *str);
							break;

						case '\b':
							nb_write += dprintf(fd, "\\b");
							break;

						case '\f':
							nb_write += dprintf(fd, "\\f");
							break;

						case '\n':
							nb_write += dprintf(fd, "\\n");
							break;

						case '\r':
							nb_write += dprintf(fd, "\\r");
							break;

						case '\t':
							nb_write += dprintf(fd, "\\t");
							break;
					}

					str++;
				}

				nb_write += dprintf(fd, "\"");
			}
			break;

		default:
			break;
	}

	return nb_write;
}

__asm__(".symver st_json_encode_to_file_v1, st_json_encode_to_file@@LIBSTONE_1.0");
size_t st_json_encode_to_file_v1(struct st_value * value, const char * filename) {
	if (filename == NULL)
		return -1;

	int fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0744);
	size_t nb_write = st_json_encode_to_fd_v1(value, fd);
	close(fd);

	return nb_write;
}

static size_t st_json_encode_to_string_inner(struct st_value * val, char * buffer, size_t length) {
	size_t nb_write = 0;
	switch (val->type) {
		case st_value_array:
		case st_value_linked_list: {
				nb_write = snprintf(buffer, length, "[");

				struct st_value_iterator * iter = st_value_list_get_iterator_v1(val);
				while (st_value_iterator_has_next_v1(iter)) {
					struct st_value * elt = st_value_iterator_get_value_v1(iter, true);
					nb_write += st_json_encode_to_string_inner(elt, buffer + nb_write, length - nb_write);
					st_value_free_v1(elt);

					if (st_value_iterator_has_next_v1(iter))
						nb_write += snprintf(buffer + nb_write, length - nb_write, ",");
				}
				nb_write += snprintf(buffer + nb_write, length - nb_write, "]");
				st_value_iterator_free_v1(iter);
			}
			break;

		case st_value_boolean:
			if (val->value.boolean)
				nb_write = snprintf(buffer, length, "true");
			else
				nb_write = snprintf(buffer, length, "false");
			break;

		case st_value_float:
			nb_write = snprintf(buffer, length, "%g", val->value.floating);
			break;

		case st_value_hashtable: {
				nb_write = snprintf(buffer, length, "{");

				struct st_value_iterator * iter = st_value_hashtable_get_iterator_v1(val);
				while (st_value_iterator_has_next_v1(iter)) {
					struct st_value * key = st_value_iterator_get_key_v1(iter, false, true);
					struct st_value * elt = st_value_iterator_get_value_v1(iter, true);

					if (key->type == st_value_string) {
						nb_write += st_json_encode_to_string_inner(key, buffer + nb_write, length - nb_write);
						nb_write += snprintf(buffer + nb_write, length - nb_write, ":");
						nb_write += st_json_encode_to_string_inner(elt, buffer + nb_write, length - nb_write);

						if (st_value_iterator_has_next_v1(iter))
							nb_write += snprintf(buffer + nb_write, length - nb_write, ",");
					}

					st_value_free_v1(key);
					st_value_free_v1(elt);
				}
				nb_write += snprintf(buffer + nb_write, length - nb_write, "}");
				st_value_iterator_free_v1(iter);
			}
			break;

		case st_value_integer:
			nb_write = snprintf(buffer, length, "%Ld", val->value.integer);
			break;

		case st_value_null:
			nb_write = snprintf(buffer, length, "null");
			break;

		case st_value_string: {
				nb_write = snprintf(buffer, length, "\"");
				char * str = val->value.string;

				while (*str != 0) {
					int nb_printable = strcspn(str, "\"\\/\b\f\n\r\t");
					if (nb_printable > 0) {
						nb_write += snprintf(buffer + nb_write, length - nb_write, "%.*s", nb_printable, str);
						str += nb_printable;

						if (*str == '\0')
							break;
					}

					switch (*str) {
						case '"':
						case '\\':
						case '/':
							nb_write += snprintf(buffer + nb_write, length - nb_write, "\\%c", *str);
							break;

						case '\b':
							nb_write += snprintf(buffer + nb_write, length - nb_write, "\\b");
							break;

						case '\f':
							nb_write += snprintf(buffer + nb_write, length - nb_write, "\\f");
							break;

						case '\n':
							nb_write += snprintf(buffer + nb_write, length - nb_write, "\\n");
							break;

						case '\r':
							nb_write += snprintf(buffer + nb_write, length - nb_write, "\\r");
							break;

						case '\t':
							nb_write += snprintf(buffer + nb_write, length - nb_write, "\\t");
							break;
					}

					str++;
				}

				nb_write += snprintf(buffer + nb_write, length - nb_write, "\"");
			}
			break;

		default:
			break;
	}

	return nb_write;
}

__asm__(".symver st_json_encode_to_string_v1, st_json_encode_to_string@@LIBSTONE_1.0");
char * st_json_encode_to_string_v1(struct st_value * value) {
	size_t length = st_json_compute_length(value);
	if (length == 0)
		return NULL;

	char * buffer = malloc(length + 1);
	buffer[length] = '\0';

	st_json_encode_to_string_inner(value, buffer, length + 1);

	return buffer;
}

__asm__(".symver st_json_parse_fd_v1, st_json_parse_fd@@LIBSTONE_1.0");
struct st_value * st_json_parse_fd_v1(int fd, int timeout) {
	ssize_t buffer_size = 4096, nb_total_read = 0, size;
	char * buffer = malloc(buffer_size + 1);
	buffer[0] = '\0';

	struct st_value * ret_val = NULL;

	while (size = read(fd, buffer + nb_total_read, buffer_size - nb_total_read), size > 0) {
		nb_total_read += size;
		buffer[nb_total_read] = '\0';

		ret_val = st_json_parse_string_v1(buffer);
		if (ret_val != NULL)
			break;

		struct pollfd pfd = {
			.fd = fd,
			.events = POLLIN | POLLHUP,
			.revents = 0,
		};
		if (poll(&pfd, 1, timeout) == 0)
			break;
		if (pfd.revents & POLLHUP)
			break;

		buffer_size += 4096;
		void * addr = realloc(buffer, buffer_size + 1);
		if (addr == NULL) {
			free(buffer);
			return NULL;
		}

		buffer = addr;
	}

	free(buffer);
	return ret_val;
}

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
	return st_json_parse_string_inner(&json);
}

static struct st_value * st_json_parse_string_inner(const char ** json) {
	st_json_parse_string_strip(json);

	struct st_value * ret_val = NULL;
	switch (**json) {
		case '{':
			ret_val = st_value_new_hashtable2_v1();
			(*json)++;

			st_json_parse_string_strip(json);

			if (**json == '}') {
				(*json)++;
				return ret_val;
			}

			for (;;) {
				if (**json != '"') {
					st_value_free_v1(ret_val);
					return NULL;
				}

				struct st_value * key = st_json_parse_string_inner(json);
				if (key == NULL || key->type != st_value_string) {
					if (key != NULL)
						st_value_free_v1(key);
					st_value_free_v1(ret_val);
					return NULL;
				}

				st_json_parse_string_strip(json);

				if (**json != ':') {
					st_value_free_v1(key);
					st_value_free_v1(ret_val);
					return NULL;
				}

				(*json)++;
				st_json_parse_string_strip(json);

				struct st_value * value = st_json_parse_string_inner(json);
				if (value == NULL) {
					st_value_free_v1(ret_val);
					st_value_free_v1(key);
					return NULL;
				}

				st_value_hashtable_put_v1(ret_val, key, true, value, true);

				st_json_parse_string_strip(json);

				if (**json == '}') {
					(*json)++;
					return ret_val;
				}

				if (**json != ',') {
					st_value_free_v1(ret_val);
					return NULL;
				}

				(*json)++;
				st_json_parse_string_strip(json);
			}
			break;

		case '[':
			ret_val = st_value_new_linked_list_v1();

			(*json)++;
			st_json_parse_string_strip(json);

			if (**json == ']') {
				(*json)++;
				return ret_val;
			}

			for (;;) {
				struct st_value * value = st_json_parse_string_inner(json);
				if (value == NULL) {
					st_value_free_v1(ret_val);
					return NULL;
				}

				st_value_list_push_v1(ret_val, value, true);

				st_json_parse_string_strip(json);

				if (**json == ']') {
					(*json)++;
					return ret_val;
				} else if (**json != ',') {
					st_value_free_v1(ret_val);
					return NULL;
				}

				(*json)++;
				st_json_parse_string_strip(json);
			}
			break;

		case '"': {
				(*json)++;
				const char * string = *json;

				size_t length;
				unsigned int unicode;
				for (length = 0; **json != '"'; (*json)++, length++) {
					if (**json == '\0')
						return NULL;

					if (**json == '\\') {
						(*json)++;

						if (strspn(*json, "\"\\/bfnrtu") == 0)
							return NULL;

						if (**json == 'u') {
							(*json)++;

							if (sscanf(*json, "%4x", &unicode) < 1)
								return NULL;
							size_t char_length = st_string_unicode_length_v1(unicode);

							if (char_length == 0)
								return NULL;

							(*json) += 3;
							length += char_length - 1;
						}
					}
				}
				(*json)++;

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

				sscanf(*json, "%le%n", &val_d, &nb_parsed_d);
				sscanf(*json, "%Ld%n", &val_i, &nb_parsed_i);

				if (nb_parsed_d == nb_parsed_i)
					ret_val = st_value_new_integer_v1(val_i);
				else
					ret_val = st_value_new_float_v1(val_d);

				(*json) += nb_parsed_d;

				return ret_val;
			}

		case 't':
			if (strncmp(*json, "true", 4) == 0) {
				(*json) += 4;
				return st_value_new_boolean_v1(true);
			}
			break;

		case 'f':
			if (strncmp(*json, "false", 5) == 0) {
				(*json) += 5;
				return st_value_new_boolean_v1(false);
			}
			break;

		case 'n':
			if (strncmp(*json, "null", 4) == 0) {
				(*json) += 4;
				return st_value_new_null_v1();
			}
			break;
	}

	return NULL;
}

static void st_json_parse_string_strip(const char ** json) {
	size_t length = strspn(*json, " \t\n");
	if (length > 0)
		(*json) += length;
}

