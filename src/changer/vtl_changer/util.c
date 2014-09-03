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
// strdup, strlen
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// close
#include <unistd.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>

#include <libstone/file.h>

#include "util.h"

char * vtl_util_get_serial(const char * filename) {
	char * serial = st_file_read_all_from(filename);
	if (serial == NULL) {
		uuid_t id;
		uuid_generate(id);

		char uuid[37];
		uuid_unparse_lower(id, uuid);

		int fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0644);
		if (fd < 0)
			return NULL;

		serial = strdup(uuid);

		write(fd, serial, strlen(serial));
		close(fd);
	}

	return serial;
}

