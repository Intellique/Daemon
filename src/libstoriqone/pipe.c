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

#define _GNU_SOURCE
// fcntl, splice, tee
#include <fcntl.h>
// close, fcntl, pipe, read
#include <unistd.h>

#include <libstoriqone/pipe.h>

void so_pipe_free(struct so_pipe * pipe) {
	if (pipe == NULL)
		return;

	close(pipe->fd_read);
	close(pipe->fd_write);

	pipe->fd_read = pipe->fd_write = -1;
	pipe->buffer_size = pipe->available_bytes = 0;
}

bool so_pipe_new(struct so_pipe * new_pipe, ssize_t length) {
	if (new_pipe == NULL)
		return false;

	int fds[2];
	int failed = pipe(fds);
	if (failed != 0)
		return false;

	new_pipe->fd_read = fds[0];
	new_pipe->fd_write = fds[1];
	new_pipe->available_bytes = 0;

	new_pipe->buffer_size = fcntl(fds[0], F_GETPIPE_SZ, NULL);

	if (new_pipe->buffer_size < length) {
		failed = fcntl(fds[0], F_SETPIPE_SZ, (int) length);
		if (failed < 0)
			return false;

		new_pipe->buffer_size = length;
	}

	return true;
}


ssize_t so_pipe_read(struct so_pipe * pipe, char * buffer, ssize_t length) {
	if (length > pipe->available_bytes)
		length = pipe->available_bytes;

	ssize_t nb_read = read(pipe->fd_read, buffer, length);
	if (nb_read < 0)
		return nb_read;

	pipe->available_bytes -= nb_read;

	return nb_read;
}


ssize_t so_pipe_forward(struct so_pipe * pipe, ssize_t length) {
	if (length > pipe->available_bytes)
		length = pipe->available_bytes;

	ssize_t nb_total_read = 0;
	while (nb_total_read < length) {
		ssize_t will_read = 8192 < length - nb_total_read ? 8192 : length - nb_total_read;

		char buffer[8192];
		ssize_t nb_read = read(pipe->fd_read, buffer, will_read);

		if (nb_read < 0)
			return nb_read;

		nb_total_read += nb_read;
		pipe->available_bytes -= nb_read;
	}

	return nb_total_read;
}


ssize_t so_pipe_splice(struct so_pipe * pipe_in, struct so_pipe * pipe_out, size_t length, unsigned int flags) {
	if (pipe_in == NULL || pipe_out == NULL)
		return -1;

	ssize_t nb_spliced = splice(pipe_in->fd_read, NULL, pipe_out->fd_write, NULL, length, flags);
	if (nb_spliced < 0)
		return nb_spliced;

	pipe_in->available_bytes -= nb_spliced;
	pipe_out->available_bytes += nb_spliced;

	return nb_spliced;
}

ssize_t so_pipe_splice_from(struct so_pipe * pipe_in, int fd_out, loff_t * off_out, size_t length, unsigned int flags) {
	if (pipe_in == 0 || fd_out < 0)
		return -1;

	ssize_t nb_spliced = splice(pipe_in->fd_read, NULL, fd_out, off_out, length, flags);
	if (nb_spliced < 0)
		return nb_spliced;

	pipe_in->available_bytes -= nb_spliced;

	return nb_spliced;
}

ssize_t so_pipe_splice_to(int fd_in, loff_t * off_in, struct so_pipe * pipe_out, size_t length, unsigned int flags) {
	if (fd_in < 0 || pipe_out == NULL)
		return -1;

	ssize_t nb_spliced = splice(fd_in, off_in, pipe_out->fd_write, NULL, length, flags);
	if (nb_spliced < 0)
		return nb_spliced;

	pipe_out->available_bytes += nb_spliced;

	return nb_spliced;
}


ssize_t so_pipe_tee(struct so_pipe * pipe_in, struct so_pipe * pipe_out, size_t length, unsigned int flags) {
	if (pipe_in == NULL || pipe_out == NULL)
		return -1;

	ssize_t nb_tee = tee(pipe_in->fd_read, pipe_out->fd_write, length, flags);

	if (nb_tee > 0)
		pipe_out->available_bytes += nb_tee;

	return nb_tee;
}

