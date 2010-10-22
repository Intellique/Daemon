/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2010, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Fri, 22 Oct 2010 17:11:34 +0200                       *
\***********************************************************************/

// open
#include <fcntl.h>
// free, malloc
#include <malloc.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// close, write
#include <unistd.h>

#include <storiqArchiver/io.h>

struct io_write_private {
	int fd;
};

static int io_write_close(struct stream_write_io * io);
static void io_write_free(struct stream_write_io * io);
static int io_write_write(struct stream_write_io * io, const void * buffer, int length);

static struct stream_write_io_ops io_write_ops = {
	.close = io_write_close,
	.free = io_write_free,
	.write = io_write_write,
};


int io_write_close(struct stream_write_io * io) {
	if (!io || !io->data)
		return -1;

	struct io_write_private * self = io->data;
	close(self->fd);
	return 0;
}

struct stream_write_io * io_write_fd(struct stream_write_io * io, int fd) {
	if (fd < 0)
		return 0;

	if (!io)
		io = malloc(sizeof(struct stream_write_io));

	struct io_write_private * self = malloc(sizeof(struct io_write_private));
	self->fd = fd;

	io->ops = &io_write_ops;
	io->data = self;

	return io;
}

struct stream_write_io * io_write_file(struct stream_write_io * io, const char * filename, int option) {
	if (!filename)
		return 0;

	int fd = open(filename, O_WRONLY | option, 0644);
	if (fd < 0)
		return 0;

	return io_write_fd(io, fd);
}

void io_write_free(struct stream_write_io * io) {
	if (!io)
		return;

	if (io->data)
		free(io->data);
	io->data = 0;
	io->ops = 0;
}

int io_write_write(struct stream_write_io * io, const void * buffer, int length) {
	if (!io || !io->data || !buffer || length < 0)
		return -1;

	struct io_write_private * self = io->data;
	return write(self->fd, buffer, length);
}

