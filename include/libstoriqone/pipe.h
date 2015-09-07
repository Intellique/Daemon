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

#ifndef __LIBSTORIQONE_PIPE_H__
#define __LIBSTORIQONE_PIPE_H__

// bool
#include <stdbool.h>
// size_t, ssize_t
#include <sys/types.h>

struct so_pipe {
	int fd_read;
	int fd_write;

	size_t available_bytes;
	size_t buffer_size;
};

void so_pipe_free(struct so_pipe * pipe);
bool so_pipe_new(struct so_pipe * pipe, size_t buffer_size);

ssize_t so_pipe_splice_from(struct so_pipe * pipe_in, int fd_out, loff_t * off_out, size_t length, unsigned int flags);
ssize_t so_pipe_splice_to(int fd_in, loff_t * off_in, struct so_pipe * pipe_out, size_t length, unsigned int flags);

ssize_t so_pipe_tee(struct so_pipe * pipe_in, struct so_pipe * pipe_out, size_t length, unsigned int flags);

#endif

