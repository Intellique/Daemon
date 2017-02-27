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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// bool
#include <stdbool.h>
// open
#include <fcntl.h>
// SIOCINQ
#include <linux/sockios.h>
// freelocale, newlocale, uselocale
#include <locale.h>
// poll
#include <poll.h>
// dprintf, snprintf, sscanf
#include <stdio.h>
// free, malloc, realloc
#include <stdlib.h>
// memcpy, memmove, strcspn, strlen, strncmp, strspn
#include <string.h>
// ioctl
#include <sys/ioctl.h>
// mmap, munmap
#include <sys/mman.h>
// recv
#include <sys/socket.h>
// fstat, open
#include <sys/stat.h>
// fstat, open, recv
#include <sys/types.h>
// access, close, fstat, read, write
#include <unistd.h>

#include <libstoriqone/io.h>
#include <libstoriqone/json.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

struct so_json_parser {
	char * buffer;
	char * dynamic_buffer;
	char static_buffer[4097];
	size_t position, used, buffer_size, last_read, last_remain;

	struct pollfd pfd;
	int timeout;

	bool (*has_more_data)(struct so_json_parser * parser, bool wait);
	ssize_t (*read)(struct so_json_parser * parser, char * buffer, ssize_t length);
	bool (*read_more)(struct so_json_parser * parser);

	struct {
		int fd[2];
	} pipe;
	struct {
		struct so_stream_reader * reader;
	} stream;
};

static size_t so_json_compute_length(struct so_value * val);
static ssize_t so_json_encode_to_string_inner(struct so_value * val, char * buffer, size_t length);

static struct so_value * so_json_parse(struct so_json_parser * parser);
static bool so_json_parse_fd_has_more_data(struct so_json_parser * parser, bool wait);
static ssize_t so_json_parse_fd_read(struct so_json_parser * parser, char * buffer, ssize_t length);
static struct so_value * so_json_parse_inner(struct so_json_parser * parser);
static struct so_value * so_json_parse_pipe(int fd, int timeout);
static bool so_json_parse_pipe_read(struct so_json_parser * parser);
static bool so_json_parse_read_more(struct so_json_parser * parser);
static struct so_value * so_json_parse_reg_file(int fd, struct stat * info);
static bool so_json_parse_skip(struct so_json_parser * parser);
static struct so_value * so_json_parse_socket(int fd, int timeout);
static bool so_json_parse_socket_read(struct so_json_parser * parser);
static bool so_json_parse_stream_has_more_data(struct so_json_parser * parser, bool wait);
static ssize_t so_json_parse_stream_read(struct so_json_parser * parser, char * buffer, ssize_t length);
static bool so_json_parse_stream_read_more(struct so_json_parser * parser);


static size_t so_json_compute_length(struct so_value * val) {
	size_t nb_write = 0;
	switch (val->type) {
		case so_value_array:
		case so_value_linked_list: {
				nb_write = 1;

				struct so_value_iterator * iter = so_value_list_get_iterator(val);
				while (so_value_iterator_has_next(iter)) {
					struct so_value * elt = so_value_iterator_get_value(iter, true);
					nb_write += so_json_compute_length(elt);
					so_value_free(elt);

					if (so_value_iterator_has_next(iter))
						nb_write++;
				}
				nb_write++;
				so_value_iterator_free(iter);
			}
			break;

		case so_value_boolean:
			if (so_value_boolean_get(val))
				nb_write = 4;
			else
				nb_write = 5;
			break;

		case so_value_float:
			nb_write = snprintf(NULL, 0, "%g", so_value_float_get(val));
			break;

		case so_value_hashtable: {
				nb_write = 1;

				struct so_value_iterator * iter = so_value_hashtable_get_iterator(val);
				while (so_value_iterator_has_next(iter)) {
					struct so_value * key = so_value_iterator_get_key(iter, false, true);
					struct so_value * elt = so_value_iterator_get_value(iter, true);

					if (key->type == so_value_string) {
						nb_write += so_json_compute_length(key);
						nb_write++;
						nb_write += so_json_compute_length(elt);

						if (so_value_iterator_has_next(iter))
							nb_write++;
					}

					so_value_free(key);
					so_value_free(elt);
				}
				nb_write++;
				so_value_iterator_free(iter);
			}
			break;

		case so_value_integer:
			nb_write = snprintf(NULL, 0, "%Ld", so_value_integer_get(val));
			break;

		case so_value_null:
			nb_write = 4;
			break;

		case so_value_string: {
				nb_write = 1;
				const char * str = so_value_string_get(val);

				while (*str != '\0') {
					if (strchr("\"\\/\b\f\n\r\t", *str) != NULL) {
						nb_write += 2;
						str++;
					} else {
						short utf8_length = so_string_valid_utf8_char(str);
						unsigned int unicode_char = so_string_convert_utf8_to_unicode(str);

						const struct so_string_character * char_info = so_string_get_character_info(unicode_char);
						if (char_info->category == so_string_character_category_other)
							nb_write += 6;
						else
							nb_write += utf8_length;

						str += utf8_length;
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

ssize_t so_json_encode_to_fd(struct so_value * val, int fd, bool use_buffer) {
	if (val == NULL)
		return 0;

	ssize_t nb_write = 0;
	if (use_buffer) {
		char * buffer = so_json_encode_to_string(val);
		if (buffer == NULL)
			return -1;

		nb_write = write(fd, buffer, strlen(buffer));
		free(buffer);
	} else {
		locale_t c_locale = newlocale(LC_ALL, "C", NULL);
		locale_t current_locale = uselocale(c_locale);

		switch (val->type) {
			case so_value_array:
			case so_value_linked_list: {
					nb_write = dprintf(fd, "[");

					struct so_value_iterator * iter = so_value_list_get_iterator(val);
					while (so_value_iterator_has_next(iter)) {
						struct so_value * elt = so_value_iterator_get_value(iter, true);
						nb_write += so_json_encode_to_fd(elt, fd, false);
						so_value_free(elt);

						if (so_value_iterator_has_next(iter))
							nb_write += dprintf(fd, ",");
					}
					nb_write += dprintf(fd, "]");
					so_value_iterator_free(iter);
				}
				break;

			case so_value_boolean:
				if (so_value_boolean_get(val))
					nb_write = dprintf(fd, "true");
				else
					nb_write = dprintf(fd, "false");
				break;

			case so_value_float:
				nb_write = dprintf(fd, "%g", so_value_float_get(val));
				break;

			case so_value_hashtable: {
					nb_write = dprintf(fd, "{");

					struct so_value_iterator * iter = so_value_hashtable_get_iterator(val);
					while (so_value_iterator_has_next(iter)) {
						struct so_value * key = so_value_iterator_get_key(iter, false, true);
						struct so_value * elt = so_value_iterator_get_value(iter, true);

						if (key->type == so_value_string) {
							nb_write += so_json_encode_to_fd(key, fd, false);
							nb_write += dprintf(fd, ":");
							nb_write += so_json_encode_to_fd(elt, fd, false);

							if (so_value_iterator_has_next(iter))
								nb_write += dprintf(fd, ",");
						}

						so_value_free(key);
						so_value_free(elt);
					}
					nb_write += dprintf(fd, "}");
					so_value_iterator_free(iter);
				}
				break;

			case so_value_integer:
				nb_write = dprintf(fd, "%Ld", so_value_integer_get(val));
				break;

			case so_value_null:
				nb_write = dprintf(fd, "null");
				break;

			case so_value_string: {
					nb_write = dprintf(fd, "\"");
					const char * str = so_value_string_get(val);

					while (*str != '\0') {
						if (strchr("\"\\/\b\f\n\r\t", *str) != NULL) {
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
						} else {
							short utf8_length = so_string_valid_utf8_char(str);
							unsigned int unicode_char = so_string_convert_utf8_to_unicode(str);

							const struct so_string_character * char_info = so_string_get_character_info(unicode_char);
							if (char_info->category == so_string_character_category_other)
								nb_write += dprintf(fd, "\\u%04x", unicode_char);
							else
								nb_write += dprintf(fd, "%.*s", utf8_length, str);

							str += utf8_length;
						}
					}

					nb_write += dprintf(fd, "\"");
				}
				break;

			default:
				break;
		}

		freelocale(c_locale);
		uselocale(current_locale);
	}

	return nb_write;
}

ssize_t so_json_encode_to_file(struct so_value * value, const char * filename) {
	if (filename == NULL)
		return -1;

	int fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (fd < 0)
		return -1;

	size_t nb_write = so_json_encode_to_fd(value, fd, false);
	close(fd);

	return nb_write;
}

static ssize_t so_json_encode_to_string_inner(struct so_value * val, char * buffer, size_t length) {
	ssize_t nb_write = 0;
	switch (val->type) {
		case so_value_array:
		case so_value_linked_list: {
				nb_write = snprintf(buffer, length, "[");

				struct so_value_iterator * iter = so_value_list_get_iterator(val);
				while (so_value_iterator_has_next(iter)) {
					struct so_value * elt = so_value_iterator_get_value(iter, true);
					nb_write += so_json_encode_to_string_inner(elt, buffer + nb_write, length - nb_write);
					so_value_free(elt);

					if (so_value_iterator_has_next(iter))
						nb_write += snprintf(buffer + nb_write, length - nb_write, ",");
				}
				nb_write += snprintf(buffer + nb_write, length - nb_write, "]");
				so_value_iterator_free(iter);
			}
			break;

		case so_value_boolean:
			if (so_value_boolean_get(val))
				nb_write = snprintf(buffer, length, "true");
			else
				nb_write = snprintf(buffer, length, "false");
			break;

		case so_value_float:
			nb_write = snprintf(buffer, length, "%g", so_value_float_get(val));
			break;

		case so_value_hashtable: {
				nb_write = snprintf(buffer, length, "{");

				struct so_value_iterator * iter = so_value_hashtable_get_iterator(val);
				while (so_value_iterator_has_next(iter)) {
					struct so_value * key = so_value_iterator_get_key(iter, false, true);
					struct so_value * elt = so_value_iterator_get_value(iter, true);

					if (key->type == so_value_string) {
						nb_write += so_json_encode_to_string_inner(key, buffer + nb_write, length - nb_write);
						nb_write += snprintf(buffer + nb_write, length - nb_write, ":");
						nb_write += so_json_encode_to_string_inner(elt, buffer + nb_write, length - nb_write);

						if (so_value_iterator_has_next(iter))
							nb_write += snprintf(buffer + nb_write, length - nb_write, ",");
					}

					so_value_free(key);
					so_value_free(elt);
				}
				nb_write += snprintf(buffer + nb_write, length - nb_write, "}");
				so_value_iterator_free(iter);
			}
			break;

		case so_value_integer:
			nb_write = snprintf(buffer, length, "%Ld", so_value_integer_get(val));
			break;

		case so_value_null:
			nb_write = snprintf(buffer, length, "null");
			break;

		case so_value_string: {
				nb_write = snprintf(buffer, length, "\"");
				const char * str = so_value_string_get(val);

				while (*str != '\0') {
					if (strchr("\"\\/\b\f\n\r\t", *str) != NULL) {
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
					} else {
						int utf8_length = so_string_valid_utf8_char(str);
						unsigned int unicode_char = so_string_convert_utf8_to_unicode(str);

						const struct so_string_character * char_info = so_string_get_character_info(unicode_char);
						if (char_info->category == so_string_character_category_other)
							nb_write += snprintf(buffer + nb_write, length - nb_write, "\\u%04x", unicode_char);
						else
							nb_write += snprintf(buffer + nb_write, length - nb_write, "%.*s", utf8_length, str);

						str += utf8_length;
					}
				}

				nb_write += snprintf(buffer + nb_write, length - nb_write, "\"");
			}
			break;

		default:
			break;
	}

	return nb_write;
}

char * so_json_encode_to_string(struct so_value * value) {
	locale_t c_locale = newlocale(LC_ALL, "C", NULL);
	locale_t current_locale = uselocale(c_locale);

	size_t length = so_json_compute_length(value);
	if (length == 0) {
		freelocale(c_locale);
		uselocale(current_locale);

		return NULL;
	}

	char * buffer = malloc(length + 1);
	if (buffer == NULL) {
		freelocale(c_locale);
		uselocale(current_locale);

		return NULL;
	}

	buffer[length] = '\0';

	so_json_encode_to_string_inner(value, buffer, length + 1);

	freelocale(c_locale);
	uselocale(current_locale);

	return buffer;
}


static struct so_value * so_json_parse(struct so_json_parser * parser) {
	locale_t c_locale = newlocale(LC_ALL, "C", NULL);
	locale_t current_locale = uselocale(c_locale);

	struct so_value * returned = so_json_parse_inner(parser);

	freelocale(c_locale);
	uselocale(current_locale);

	if (parser->read != NULL && parser->position > 0) {
		static char tmp_buffer[4096];
		parser->read(parser, tmp_buffer, parser->position - parser->last_remain);
	}

	return returned;
}

struct so_value * so_json_parse_fd(int fd, int timeout) {
	struct so_value * ret_val = NULL;

	struct stat fd_info;
	int failed = fstat(fd, &fd_info);
	if (failed != 0)
		return NULL;

	if (S_ISREG(fd_info.st_mode))
		ret_val = so_json_parse_reg_file(fd, &fd_info);
	else if (S_ISFIFO(fd_info.st_mode))
		ret_val = so_json_parse_pipe(fd, timeout);
	else if (S_ISSOCK(fd_info.st_mode))
		ret_val = so_json_parse_socket(fd, timeout);

	return ret_val;
}

static bool so_json_parse_fd_has_more_data(struct so_json_parser * parser, bool wait) {
	parser->pfd.revents = 0;
	if (poll(&parser->pfd, 1, wait ? parser->timeout : 0) == 0)
		return false;

	if (parser->pfd.revents == POLLHUP)
		return false;

	return true;
}

static ssize_t so_json_parse_fd_read(struct so_json_parser * parser, char * buffer, ssize_t length) {
	return read(parser->pfd.fd, buffer, length);
}

struct so_value * so_json_parse_file(const char * file) {
	if (file == NULL)
		return NULL;

	if (access(file, R_OK))
		return NULL;

	int fd = open(file, O_RDONLY);
	if (fd < 0)
		return NULL;

	struct so_value * ret_value = so_json_parse_fd(fd, -1);
	close(fd);

	return ret_value;
}

static struct so_value * so_json_parse_inner(struct so_json_parser * parser) {
	if (!so_json_parse_skip(parser))
		return NULL;

	struct so_value * ret_val = NULL;
	switch (parser->buffer[parser->position]) {
		case '{':
			ret_val = so_value_new_hashtable2();
			parser->position++;

			if (!so_json_parse_skip(parser)) {
				so_value_free(ret_val);
				return NULL;
			}

			while (parser->buffer[parser->position] != '}') {
				if (!so_json_parse_skip(parser)) {
					so_value_free(ret_val);
					return NULL;
				}

				if (parser->buffer[parser->position] != '"') {
					so_value_free(ret_val);
					return NULL;
				}

				struct so_value * key = so_json_parse_inner(parser);
				if (key == NULL || key->type != so_value_string) {
					if (key != NULL)
						so_value_free(key);
					so_value_free(ret_val);
					return NULL;
				}

				if (!so_json_parse_skip(parser)) {
					so_value_free(ret_val);
					return NULL;
				}

				if (parser->buffer[parser->position] != ':') {
					so_value_free(ret_val);
					return NULL;
				}

				parser->position++;

				if (!so_json_parse_skip(parser)) {
					so_value_free(key);
					so_value_free(ret_val);
					return NULL;
				}

				struct so_value * value = so_json_parse_inner(parser);
				if (value == NULL) {
					so_value_free(ret_val);
					so_value_free(key);
					return NULL;
				}

				so_value_hashtable_put(ret_val, key, true, value, true);

				if (!so_json_parse_skip(parser)) {
					so_value_free(ret_val);
					return NULL;
				}

				if (parser->buffer[parser->position] == ',')
					parser->position++;
				else if (parser->buffer[parser->position] != '}') {
					so_value_free(ret_val);
					return NULL;
				}
			}

			parser->position++;
			break;

		case '[':
			ret_val = so_value_new_linked_list();
			parser->position++;

			if (!so_json_parse_skip(parser)) {
				so_value_free(ret_val);
				return NULL;
			}

			while (parser->buffer[parser->position] != ']') {
				struct so_value * value = so_json_parse_inner(parser);
				if (value == NULL) {
					so_value_free(ret_val);
					return NULL;
				}

				so_value_list_push(ret_val, value, true);

				if (!so_json_parse_skip(parser)) {
					so_value_free(ret_val);
					return NULL;
				}

				if (parser->buffer[parser->position] == ',')
					parser->position++;
				else if (parser->buffer[parser->position] != ']') {
					so_value_free(ret_val);
					return NULL;
				}
			}

			parser->position++;
			break;

		case '"': {
				parser->position++;
				const char * str = parser->buffer + parser->position;

				unsigned int str_length, position;
				for (str_length = 0, position = 0; str[position] != '"'; str_length++, position++) {
					if (parser->position + position >= parser->used) {
						if (!so_json_parse_read_more(parser))
							return NULL;

						str = parser->buffer;

						if (str[position] == '"')
							break;
					}

					if (str[position] == '\0')
						return NULL;

					if (str[position] == '\\') {
						position++;

						if (parser->position + position >= parser->used) {
							if (!so_json_parse_read_more(parser))
								return NULL;

							str = parser->buffer;

							if (str[position] == '"')
								return NULL;
						}

						if (strspn(str + position, "\"\\/bfnrtu") == 0)
							return NULL;

						if (str[position] == 'u') {
							position++;

							while (parser->position + position + 4 >= parser->used) {
								if (!so_json_parse_read_more(parser))
									return NULL;

								str = parser->buffer;
							}

							unsigned int unicode;
							if (sscanf(str + position, "%4x", &unicode) < 1)
								return NULL;

							size_t char_length = so_string_unicode_length(unicode);

							if (char_length == 0)
								return NULL;

							position += 3;
							str_length += char_length - 1;
						}
					}
				}
				parser->position += position + 1;

				char * tmp_string = malloc(str_length + 1);
				size_t from, to;
				for (from = 0, to = 0; to < str_length; from++, to++) {
					if (str[from] == '\\') {
						from++;
						unsigned int unicode;

						switch (str[from]) {
							case '"':
							case '\\':
							case '/':
								tmp_string[to] = str[from];
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
								sscanf(str + from, "%4x", &unicode);
								from += 3;

								so_string_convert_unicode_to_utf8(unicode, tmp_string + to, position - to, false);
								to += so_string_unicode_length(unicode) - 1;
								break;
						}
					} else
						tmp_string[to] = str[from];
				}
				tmp_string[to] = '\0';

				ret_val = so_value_new_string(tmp_string);
				free(tmp_string);
				return ret_val;

				break;
			}

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
				ssize_t length_integer = strspn(parser->buffer + parser->position, "0123456789-");
				ssize_t length_float = strspn(parser->buffer + parser->position, "0123456789.+-eE");

				if (parser->position + length_float == parser->used) {
					so_json_parse_read_more(parser);

					length_integer = strspn(parser->buffer, "0123456789-");
					length_float = strspn(parser->buffer, "0123456789.+-eE");
				}

				if (length_integer < length_float) {
					double val;
					sscanf(parser->buffer + parser->position, "%le", &val);

					ret_val = so_value_new_float(val);
				} else {
					long long int val;
					sscanf(parser->buffer + parser->position, "%Ld", &val);

					ret_val = so_value_new_integer(val);
				}

				parser->position += length_float;
				break;
			}

		case 't':
			if (strncmp(parser->buffer + parser->position, "true", 4) == 0) {
				parser->position += 4;
				ret_val = so_value_new_boolean(true);
			} else {
				so_json_parse_read_more(parser);

				if (strncmp(parser->buffer + parser->position, "true", 4) == 0) {
					parser->position += 4;
					ret_val = so_value_new_boolean(true);
				}
			}
			break;

		case 'f':
			if (strncmp(parser->buffer + parser->position, "false", 5) == 0) {
				parser->position += 5;
				ret_val = so_value_new_boolean(false);
			} else {
				so_json_parse_read_more(parser);

				if (strncmp(parser->buffer + parser->position, "false", 5) == 0) {
					parser->position += 5;
					ret_val = so_value_new_boolean(false);
				}
			}
			break;

		case 'n':
			if (strncmp(parser->buffer + parser->position, "null", 4) == 0) {
				parser->position += 4;
				ret_val = so_value_new_null();
			} else {
				so_json_parse_read_more(parser);

				if (strncmp(parser->buffer + parser->position, "null", 4) == 0) {
					parser->position += 4;
					ret_val = so_value_new_null();
				}
			}
			break;
	}

	return ret_val;
}

static struct so_value * so_json_parse_pipe(int fd, int timeout) {
	struct so_json_parser parser = {
		.dynamic_buffer = NULL,
		.position = 0,
		.used = 0,
		.buffer_size = 4096,
		.last_read = 0,
		.last_remain = 0,

		.pfd = {
			.fd = fd,
			.events = POLLIN | POLLHUP,
			.revents = 0,
		},
		.timeout = timeout,

		.has_more_data = so_json_parse_fd_has_more_data,
		.read = so_json_parse_fd_read,
		.read_more = so_json_parse_pipe_read,

		.pipe.fd = { -1, -1 },
		.stream.reader = NULL,
	};
	parser.buffer = parser.static_buffer;

	if (poll(&parser.pfd, 1, timeout) == 0)
		return NULL;

	if (parser.pfd.revents == POLLHUP)
		return NULL;

	ssize_t available_in_pipe = 0;
	int failed = ioctl(fd, FIONREAD, &available_in_pipe);
	if (failed != 0)
		return NULL;

	if (available_in_pipe > 4096)
		available_in_pipe = 4096;

	failed = pipe(parser.pipe.fd);
	if (failed != 0)
		return NULL;

	ssize_t nb_copied = tee(fd, parser.pipe.fd[1], available_in_pipe, 0);

	struct so_value * ret_val = NULL;
	if (nb_copied > 0) {
		parser.last_read = read(parser.pipe.fd[0], parser.buffer, nb_copied);
		if (parser.last_read > 0) {
			parser.used = parser.last_read;
			parser.buffer[parser.last_read] = '\0';

			ret_val = so_json_parse(&parser);
		}
	}

	free(parser.dynamic_buffer);
	close(parser.pipe.fd[0]);
	close(parser.pipe.fd[1]);

	return ret_val;
}

static bool so_json_parse_pipe_read(struct so_json_parser * parser) {
	ssize_t available_in_pipe = 0;
	int failed = ioctl(parser->pfd.fd, FIONREAD, &available_in_pipe);
	if (failed != 0)
		return false;

	ssize_t remain = parser->buffer_size - parser->used;
	if (available_in_pipe > remain)
		available_in_pipe = remain;

	ssize_t nb_copied = tee(parser->pfd.fd, parser->pipe.fd[1], available_in_pipe, 0);
	if (nb_copied < 0)
		return false;

	ssize_t nb_read = read(parser->pipe.fd[0], parser->buffer + parser->used, nb_copied);
	if (nb_read < 0)
		return false;

	parser->last_read = nb_read;
	parser->used += nb_read;
	parser->buffer[parser->used] = '\0';

	return true;
}

static bool so_json_parse_read_more(struct so_json_parser * parser) {
	if (parser->read == NULL)
		return false;

	if (parser->last_read > 0) {
		static char tmp_buffer[4096];
		ssize_t nb_read = parser->read(parser, tmp_buffer, parser->last_read);
		if (nb_read < 0)
			return false;
	}

	if (!parser->has_more_data(parser, true))
		return false;

	if (parser->position < parser->used) {
		parser->last_remain = parser->used - parser->position;

		if (parser->last_remain > 2048 && parser->dynamic_buffer == NULL) {
			parser->buffer_size = 8192;
			parser->dynamic_buffer = malloc(parser->buffer_size);

			memcpy(parser->dynamic_buffer, parser->static_buffer + parser->position, parser->last_remain);

			parser->buffer = parser->dynamic_buffer;
		} else {
			memmove(parser->buffer, parser->buffer + parser->position, parser->last_remain);

			if (parser->buffer_size - parser->last_remain < 2048) {
				parser->buffer_size += 4096;

				void * addr = realloc(parser->dynamic_buffer, parser->buffer_size);
				if (addr == NULL)
					return false;

				parser->buffer = parser->dynamic_buffer = addr;
			}
		}

		parser->position = 0;
		parser->used = parser->last_remain;
	} else
		parser->position = parser->used = parser->last_remain = 0;

	return parser->read_more(parser);
}

static struct so_value * so_json_parse_reg_file(int fd, struct stat * info) {
	char * buffer = mmap(NULL, info->st_size, PROT_READ, MAP_PRIVATE, fd, 0);

	struct so_json_parser parser = {
		.buffer = buffer,
		.dynamic_buffer = buffer,
		.position = 0,
		.used = info->st_size,
		.last_read = 0,
		.last_remain = 0,

		.pfd = {
			.fd = -1,
			.events = 0,
			.revents = 0,
		},
		.timeout = 0,

		.has_more_data = NULL,
		.read = NULL,
		.read_more = NULL,

		.pipe.fd = { -1, -1 },
		.stream.reader = NULL,
	};
	struct so_value * ret_val = so_json_parse(&parser);

	munmap(buffer, info->st_size);

	return ret_val;
}

static bool so_json_parse_skip(struct so_json_parser * parser) {
	do {
		if (parser->position == parser->used && !so_json_parse_read_more(parser))
			return false;

		ssize_t length = strspn(parser->buffer + parser->position, " \t\n");
		if (length > 0)
			parser->position += length;
	} while (parser->position == parser->used);

	return true;
}

static struct so_value * so_json_parse_socket(int fd, int timeout) {
	struct so_json_parser parser = {
		.dynamic_buffer = NULL,
		.position = 0,
		.used = 0,
		.buffer_size = 4096,
		.last_read = 0,
		.last_remain = 0,

		.pfd = {
			.fd = fd,
			.events = POLLIN | POLLHUP,
			.revents = 0,
		},
		.timeout = timeout,

		.has_more_data = so_json_parse_fd_has_more_data,
		.read = so_json_parse_fd_read,
		.read_more = so_json_parse_socket_read,

		.pipe.fd = { -1, -1 },
		.stream.reader = NULL,
	};
	parser.buffer = parser.static_buffer;

	if (poll(&parser.pfd, 1, timeout) == 0)
		return NULL;

	if (parser.pfd.revents == POLLHUP)
		return NULL;

	ssize_t available_in_socket = 0;
	int failed = ioctl(fd, SIOCINQ, &available_in_socket);
	if (failed != 0)
		return NULL;

	if (available_in_socket > 4096)
		available_in_socket = 4096;

	ssize_t nb_peek = recv(fd, parser.buffer, available_in_socket, MSG_PEEK);

	struct so_value * ret_val = NULL;

	if (nb_peek > 0)
		ret_val = so_json_parse(&parser);

	free(parser.dynamic_buffer);

	return ret_val;
}

static bool so_json_parse_socket_read(struct so_json_parser * parser) {
	ssize_t available_in_socket = 0;
	int failed = ioctl(parser->pfd.fd, SIOCINQ, &available_in_socket);
	if (failed != 0)
		return false;

	ssize_t remain = parser->buffer_size - parser->used;
	if (available_in_socket > remain)
		available_in_socket = remain;

	ssize_t nb_peek = recv(parser->pfd.fd, parser->buffer + parser->used, available_in_socket, MSG_PEEK);
	if (nb_peek < 0)
		return false;

	parser->last_read = nb_peek;
	parser->used += nb_peek;
	parser->buffer[parser->used] = '\0';

	return true;
}

struct so_value * so_json_parse_stream(struct so_stream_reader * reader) {
	struct so_json_parser parser = {
		.dynamic_buffer = NULL,
		.position = 0,
		.used = 0,
		.last_read = 0,
		.last_remain = 0,

		.pfd = {
			.fd = -1,
			.events = POLLIN | POLLHUP,
			.revents = 0,
		},
		.timeout = -1,

		.has_more_data = so_json_parse_stream_has_more_data,
		.read = so_json_parse_stream_read,
		.read_more = so_json_parse_stream_read_more,

		.pipe.fd = { -1, -1 },
		.stream.reader = reader,
	};
	parser.buffer = parser.static_buffer;

	struct so_value * ret_val = so_json_parse(&parser);

	free(parser.dynamic_buffer);

	return ret_val;
}

static bool so_json_parse_stream_has_more_data(struct so_json_parser * parser, bool wait __attribute__((unused))) {
	ssize_t available_in_stream = parser->stream.reader->ops->get_available_size(parser->stream.reader);
	return available_in_stream > 0;
}

static ssize_t so_json_parse_stream_read(struct so_json_parser * parser, char * buffer, ssize_t length) {
	return parser->stream.reader->ops->read(parser->stream.reader, buffer, length);
}

static bool so_json_parse_stream_read_more(struct so_json_parser * parser) {
	ssize_t available_in_stream = parser->stream.reader->ops->get_available_size(parser->stream.reader);

	ssize_t remain = parser->buffer_size - parser->used;
	if (available_in_stream > remain)
		available_in_stream = remain;

	ssize_t nb_read = parser->stream.reader->ops->peek(parser->stream.reader, parser->buffer + parser->used, remain);
	if (nb_read < 0)
		return false;

	parser->last_read = nb_read;
	parser->used += nb_read;
	parser->buffer[parser->used] = '\0';

	return true;
}

struct so_value * so_json_parse_string(const char * json) {
	struct so_json_parser parser = {
		.buffer = (char *) json,
		.dynamic_buffer = NULL,
		.position = 0,
		.used = strlen(json),
		.last_read = 0,
		.last_remain = 0,

		.pfd = {
			.fd = -1,
			.events = 0,
			.revents = 0,
		},
		.timeout = 0,

		.has_more_data = NULL,
		.read = NULL,
		.read_more = NULL,

		.pipe.fd = { -1, -1 },
		.stream.reader = NULL,
	};
	return so_json_parse(&parser);
}

