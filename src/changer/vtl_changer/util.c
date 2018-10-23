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
*  Copyright (C) 2013-2018, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// open
#include <fcntl.h>
// free
#include <stdlib.h>
// strdup, strlen
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// access, close
#include <unistd.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>

#include <libstoriqone/file.h>

#include "util.h"

char * sochgr_vtl_util_get_serial(const char * filename) {
	char * serial = NULL;

	if (access(filename, R_OK) == 0)
		serial = so_file_read_all_from(filename);

	if (serial == NULL) {
		uuid_t id;
		uuid_generate(id);

		char uuid[37];
		uuid_unparse_lower(id, uuid);

		int fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0644);
		if (fd < 0)
			return NULL;

		serial = strdup(uuid);

		ssize_t nb_write = write(fd, serial, strlen(serial));
		close(fd);

		if (nb_write < 0) {
			free(serial);
			return NULL;
		}
	}

	return serial;
}
