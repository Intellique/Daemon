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
// strchr
#include <string.h>
// fstat, open
#include <sys/stat.h>
// fstat, open
#include <sys/types.h>
// access, close, fstat, read, readlink, unlink, write
#include <unistd.h>

#include <libstone/string.h>
#include <libstone/value.h>

#include "conf.h"


struct st_value * st_conf_read_config(const char * conf_file) {
	if (conf_file == NULL)
		return NULL;

	if (access(conf_file, R_OK))
		return NULL;

	int fd = open(conf_file, O_RDONLY);

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

	char * ptr = buffer;
	struct st_value * params = st_value_new_hashtable(st_string_compute_hash);
	struct st_value * section = NULL;
	struct st_value * param = NULL;

	char sec[24];
	struct st_value * key;
	while (ptr != NULL && *ptr) {
		switch (*ptr) {
			case ';':
				ptr = strchr(ptr, '\n');
				continue;

			case '\n':
				ptr++;
				if (*ptr == '\n')
					section = NULL;
				continue;

			case '[':
				sscanf(ptr, "[%23[^]]]", sec);

				key = st_value_new_string(sec);
				if (st_value_hashtable_has_key(params, key))
					section = st_value_hashtable_get(params, key, false);
				else {
					section = st_value_new_linked_list();
					st_value_hashtable_put(params, key, false, section, true);
				}
				st_value_free(key);
				key = NULL;

				param = st_value_new_hashtable(st_string_compute_hash);
				st_value_list_push(section, param, true);

				ptr = strchr(ptr, '\n');
				continue;

			default:
				if (section == NULL)
					continue;

				if (strchr(ptr, '=') < strchr(ptr, '\n')) {
					char key[32];
					char value[64];
					int val = sscanf(ptr, "%s = %s", key, value);
					if (val == 2)
						st_value_hashtable_put(param, st_value_new_string(key), true, st_value_new_string(value), true);
				}
				ptr = strchr(ptr, '\n');
		}
	}

	free(buffer);

	return params;
}

