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

// open
#include <fcntl.h>
// prinf
#include <stdio.h>
// free
#include <stdlib.h>
// strcmp
#include <string.h>
// close, read
#include <unistd.h>

#include <libstoriqone/checksum.h>

int main(int argc, char ** argv) {
	if (argc < 3)
		return 1;

	struct so_checksum_driver * driver = so_checksum_get_driver(argv[1]);
	if (driver == NULL)
		return 2;

	struct so_checksum * chck = driver->new_checksum();
	if (chck == NULL)
		return 3;

	int fd = 0;
	if (strcmp(argv[2], "-") != 0) {
		fd = open(argv[2], O_RDONLY);
		if (fd < 0)
			return 4;
	}

	static char buffer[65536];
	ssize_t nb_read;

	while (nb_read = read(fd, buffer, 65536), nb_read > 0)
		chck->ops->update(chck, buffer, nb_read);

	close(fd);

	char * digest = chck->ops->digest(chck);

	if (strcmp(argv[2], "-") != 0)
		printf("Compute '%s' of '%s' => '%s'\n", argv[1], argv[2], digest);
	else
		printf("Compute '%s' of stdin => '%s'\n", argv[1], digest);

	chck->ops->free(chck);
	free(digest);

	return 0;
}

