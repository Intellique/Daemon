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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

// errno
#include <errno.h>
// free
#include <stdlib.h>
// close
#include <unistd.h>

#include <libstoriqone-drive/time.h>

#include "device.h"
#include "io.h"

int sodr_vtl_drive_io_close(struct sodr_vtl_drive_io * io) {
	struct so_drive * dr = sodr_vtl_drive_get_device();

	if (io->fd < 0)
		return 0;

	sodr_time_start();
	int failed = close(io->fd);
	sodr_time_stop(dr);

	if (failed != 0)
		io->last_errno = errno;
	else
		io->fd = -1;

	return failed;
}

void sodr_vtl_drive_io_free(struct sodr_vtl_drive_io * io) {
	if (io == NULL)
		return;

	if (io->fd >= 0)
		sodr_vtl_drive_io_close(io);

	free(io->buffer);
	free(io);
}

