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

// open
#include <fcntl.h>
// free, malloc
#include <malloc.h>
// fstat, open
#include <sys/stat.h>
// fstat, open
#include <sys/types.h>
// access, close, fstat, read, readlink, unlink, write
#include <unistd.h>

#include <libstone/util/string.h>
#include <libstone/util/value.h>

#include "conf.h"

static void st_conf_exit(void) __attribute__((destructor));
static void st_conf_init(void) __attribute__((constructor));

static struct st_value * st_conf_callback = NULL;


static void st_conf_exit() {
	st_value_free_v1(st_conf_callback);
}

static void st_conf_init() {
	st_conf_callback = st_value_new_hashtable_v1(st_util_string_compute_hash_v1);
}

int st_conf_read_config(const char * conf_file) {
	if (conf_file == NULL)
		return -1;

	if (access(conf_file, R_OK))
		return -1;

	int fd = open(conf_file, O_RDONLY);

	struct stat st;
	fstat(fd, &st);

	char * buffer = malloc(st.st_size + 1);
	ssize_t nb_read = read(fd, buffer, st.st_size);
	close(fd);
	buffer[nb_read] = '\0';

	if (nb_read < 0) {
		free(buffer);
		return 1;
	}

	char * ptr = buffer;
	char section[24] = { '\0' };
	struct st_hashtable * params = st_hashtable_new2(st_util_string_compute_hash, st_util_basic_free);

	while (ptr != NULL && *ptr) {
		switch (*ptr) {
			case ';':
				ptr = strchr(ptr, '\n');
				continue;

			case '\n':
				ptr++;
				if (*ptr == '\n') {
					if (*section) {
						st_conf_callback_f f = st_hashtable_get(st_conf_callback, section).value.custom;
						if (f != NULL)
							f(params);
					}

					*section = 0;
					st_hashtable_clear(params);
				}
				continue;

			case '[':
				sscanf(ptr, "[%23[^]]]", section);

				ptr = strchr(ptr, '\n');
				continue;

			default:
				if (!*section)
					continue;

				if (strchr(ptr, '=') < strchr(ptr, '\n')) {
					char key[32];
					char value[64];
					int val = sscanf(ptr, "%s = %s", key, value);
					if (val == 2)
						st_hashtable_put(params, strdup(key), st_hashtable_val_string(strdup(value)));
				}
				ptr = strchr(ptr, '\n');
		}
	}

	if (params->nb_elements > 0 && *section) {
		st_conf_callback_f f = st_hashtable_get(st_conf_callback, section).value.custom;
		if (f != NULL)
			f(params);
	}

	st_hashtable_free(params);
	free(buffer);

	st_log_start_logger();

	return 0;
}

