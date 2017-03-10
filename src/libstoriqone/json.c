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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// bool
#include <stdbool.h>
// open
#include <fcntl.h>
// freelocale, newlocale, uselocale
#include <locale.h>
// poll
#include <poll.h>
// dprintf, snprintf, sscanf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// memmove, strlen, strncmp, strspn, strstr
#include <string.h>
// fstat, open
#include <sys/stat.h>
// fstat, open
#include <sys/types.h>
// access, close, fstat, read, write
#include <unistd.h>

#include <libstoriqone/io.h>
#include <libstoriqone/json.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

static size_t so_json_compute_length(struct so_value * val);
static ssize_t so_json_encode_to_string_inner(struct so_value * val, char * buffer, size_t length);
static struct so_value * so_json_parse_string_inner(const char ** json);
static void so_json_parse_string_strip(const char ** json);


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

						if (unicode_char >= 0x10000) {
							nb_write += 12;
						} else {
							const struct so_string_character * char_info = so_string_get_character_info(unicode_char);
							if (char_info->category == so_string_character_category_other)
								nb_write += 6;
							else
								nb_write += utf8_length;
						}

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

							if (unicode_char >= 0x10000) {
								// WTF-8
								unsigned int p1 = ((unicode_char - 0x10000) >> 10) + 0xD800;
								unsigned int p2 = ((unicode_char - 0x10000) & 0x3FF) + 0xDC00;

								nb_write += dprintf(fd, "\\u%04x\\u%04x", p1, p2);
							} else {
								const struct so_string_character * char_info = so_string_get_character_info(unicode_char);
								if (char_info->category == so_string_character_category_other)
									nb_write += dprintf(fd, "\\u%04x", unicode_char);
								else
									nb_write += dprintf(fd, "%.*s", utf8_length, str);
							}

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

						if (unicode_char >= 0x10000) {
							// WTF-8
							unsigned int p1 = ((unicode_char - 0x10000) >> 10) + 0xD800;
							unsigned int p2 = ((unicode_char - 0x10000) & 0x3FF) + 0xDC00;

							nb_write += snprintf(buffer + nb_write, length - nb_write, "\\u%04x\\u%04x", p1, p2);
						} else {
							const struct so_string_character * char_info = so_string_get_character_info(unicode_char);
							if (char_info->category == so_string_character_category_other)
								nb_write += snprintf(buffer + nb_write, length - nb_write, "\\u%04x", unicode_char);
							else
								nb_write += snprintf(buffer + nb_write, length - nb_write, "%.*s", utf8_length, str);
						}

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

struct so_value * so_json_parse_fd(int fd, int timeout) {
	struct pollfd pfd = {
		.fd = fd,
		.events = POLLIN | POLLHUP,
		.revents = 0,
	};
	if (poll(&pfd, 1, timeout) == 0)
		return NULL;

	if (pfd.revents == POLLHUP)
		return NULL;

	ssize_t buffer_size = 4096, nb_total_read = 0, size;
	char * buffer = malloc(buffer_size + 1);
	buffer[0] = '\0';

	struct so_value * ret_val = NULL;

	while (size = read(fd, buffer + nb_total_read, buffer_size - nb_total_read), size > 0) {
		nb_total_read += size;
		buffer[nb_total_read] = '\0';

		ret_val = so_json_parse_string(buffer);
		if (ret_val != NULL)
			break;

		pfd.revents = 0;
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

struct so_value * so_json_parse_file(const char * file) {
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

	struct so_value * ret_value = so_json_parse_string(buffer);
	free(buffer);

	return ret_value;
}

struct so_value * so_json_parse_stream(struct so_stream_reader * reader) {
	ssize_t buffer_size = 4096, nb_total_read = 0, size;
	char * buffer = malloc(buffer_size + 1);
	buffer[0] = '\0';

	struct so_value * ret_val = NULL;

	while (size = reader->ops->read(reader, buffer + nb_total_read, buffer_size - nb_total_read), size > 0) {
		nb_total_read += size;
		buffer[nb_total_read] = '\0';

		ret_val = so_json_parse_string(buffer);
		if (ret_val != NULL)
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

struct so_value * so_json_parse_string(const char * json) {
	locale_t c_locale = newlocale(LC_ALL, "C", NULL);
	locale_t current_locale = uselocale(c_locale);

	struct so_value * returned = so_json_parse_string_inner(&json);

	freelocale(c_locale);
	uselocale(current_locale);

	return returned;
}

static struct so_value * so_json_parse_string_inner(const char ** json) {
	so_json_parse_string_strip(json);

	struct so_value * ret_val = NULL;
	switch (**json) {
		case '{':
			ret_val = so_value_new_hashtable2();
			(*json)++;

			so_json_parse_string_strip(json);

			if (**json == '}') {
				(*json)++;
				return ret_val;
			}

			for (;;) {
				if (**json != '"') {
					so_value_free(ret_val);
					return NULL;
				}

				struct so_value * key = so_json_parse_string_inner(json);
				if (key == NULL || key->type != so_value_string) {
					if (key != NULL)
						so_value_free(key);
					so_value_free(ret_val);
					return NULL;
				}

				so_json_parse_string_strip(json);

				if (**json != ':') {
					so_value_free(key);
					so_value_free(ret_val);
					return NULL;
				}

				(*json)++;
				so_json_parse_string_strip(json);

				struct so_value * value = so_json_parse_string_inner(json);
				if (value == NULL) {
					so_value_free(ret_val);
					so_value_free(key);
					return NULL;
				}

				so_value_hashtable_put(ret_val, key, true, value, true);

				so_json_parse_string_strip(json);

				if (**json == '}') {
					(*json)++;
					return ret_val;
				}

				if (**json != ',') {
					so_value_free(ret_val);
					return NULL;
				}

				(*json)++;
				so_json_parse_string_strip(json);
			}
			break;

		case '[':
			ret_val = so_value_new_linked_list();

			(*json)++;
			so_json_parse_string_strip(json);

			if (**json == ']') {
				(*json)++;
				return ret_val;
			}

			for (;;) {
				struct so_value * value = so_json_parse_string_inner(json);
				if (value == NULL) {
					so_value_free(ret_val);
					return NULL;
				}

				so_value_list_push(ret_val, value, true);

				so_json_parse_string_strip(json);

				if (**json == ']') {
					(*json)++;
					return ret_val;
				} else if (**json != ',') {
					so_value_free(ret_val);
					return NULL;
				}

				(*json)++;
				so_json_parse_string_strip(json);
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

							if ((unicode & 0xD800) == 0xD800) {
								(*json) += 4;

								if (**json != '\\')
									return NULL;

								(*json)++;

								if (**json != 'u')
									return NULL;

								(*json)++;

								unsigned int p2 = 0;
								if (sscanf(*json, "%4x", &p2) < 1 || (p2 & 0xDC00) != 0xDC00)
									return NULL;

								// 0x10000 + ((U - 0xD800) << 10) + (next - 0xDC00)
								unicode = 0x10000 + ((unicode - 0xD800) << 10) + (p2 - 0xDC00);
							}

							size_t char_length = so_string_unicode_length(unicode);

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

								if ((unicode & 0xD800) == 0xD800) {
									from += 3;

									unsigned int p2 = 0;
									sscanf(string + from, "%4x", &p2);
									from += 3;

									// 0x10000 + ((U - 0xD800) << 10) + (next - 0xDC00)
									unicode = 0x10000 + ((unicode - 0xD800) << 10) + (p2 - 0xDC00);
								}

								so_string_convert_unicode_to_utf8(unicode, tmp_string + to, length - to, false);
								to += so_string_unicode_length(unicode) - 1;
								break;
						}
					} else
						tmp_string[to] = string[from];
				}
				tmp_string[to] = '\0';

				ret_val = so_value_new_string(tmp_string);
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
				ssize_t length_integer = strspn(*json, "0123456789-");
				ssize_t length_float = strspn(*json, "0123456789.+-eE");

				if (length_integer < length_float) {
					double val;
					sscanf(*json, "%le", &val);

					ret_val = so_value_new_float(val);
					(*json) += length_float;
				} else {
					long long int val;
					sscanf(*json, "%Ld", &val);

					ret_val = so_value_new_integer(val);
					(*json) += length_integer;
				}

				return ret_val;
			}

		case 't':
			if (strncmp(*json, "true", 4) == 0) {
				(*json) += 4;
				return so_value_new_boolean(true);
			}
			break;

		case 'f':
			if (strncmp(*json, "false", 5) == 0) {
				(*json) += 5;
				return so_value_new_boolean(false);
			}
			break;

		case 'n':
			if (strncmp(*json, "null", 4) == 0) {
				(*json) += 4;
				return so_value_new_null();
			}
			break;
	}

	return NULL;
}

static void so_json_parse_string_strip(const char ** json) {
	size_t length = strspn(*json, " \t\n");
	if (length > 0)
		(*json) += length;
}
