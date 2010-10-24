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
*  Last modified: Sun, 24 Oct 2010 21:26:34 +0200                       *
\***********************************************************************/

// open
#include <fcntl.h>
// free, malloc
#include <malloc.h>
// bzero, memcpy
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// close, write
#include <unistd.h>

#include <storiqArchiver/io.h>

struct io_write_private {
	int fd;
	char * buffer;
	int bufferUsed;
	int blockSize;
};

static int io_write_close(struct stream_write_io * io);
static int io_write_flush(struct stream_write_io * io);
static void io_write_free(struct stream_write_io * io);
static int io_write_write(struct stream_write_io * io, const void * buffer, int length);
static int io_write_write_buffer(struct stream_write_io * io, const void * buffer, int length);

static struct stream_write_io_ops io_write_ops = {
	.close = io_write_close,
	.flush = io_write_flush,
	.free = io_write_free,
	.write = io_write_write,
};

static struct stream_write_io_ops io_write_buffer_ops = {
	.close = io_write_close,
	.flush = io_write_flush,
	.free = io_write_free,
	.write = io_write_write_buffer,
};


int io_write_close(struct stream_write_io * io) {
	if (!io || !io->data)
		return -1;

	struct io_write_private * self = io->data;
	if (self->bufferUsed > 0) {
		bzero(self->buffer + self->bufferUsed, self->blockSize - self->bufferUsed);
		write(self->fd, self->buffer, self->blockSize);
	}
	close(self->fd);
	return 0;
}

struct stream_write_io * io_write_fd(struct stream_write_io * io, int fd) {
	return io_write_fd2(io, fd, 0);
}

struct stream_write_io * io_write_fd2(struct stream_write_io * io, int fd, int blockSize) {
	if (fd < 0 || blockSize < 0)
		return 0;

	if (!io)
		io = malloc(sizeof(struct stream_write_io));

	struct io_write_private * self = malloc(sizeof(struct io_write_private));
	self->fd = fd;
	self->buffer = 0;
	if (blockSize > 0)
		self->buffer = malloc(blockSize);
	self->bufferUsed = 0;
	self->blockSize = blockSize;

	if (blockSize > 0)
		io->ops = &io_write_buffer_ops;
	else
		io->ops = &io_write_ops;
	io->data = self;

	return io;
}

struct stream_write_io * io_write_file(struct stream_write_io * io, const char * filename, int option) {
	return io_write_file2(io, filename, option, 0);
}

struct stream_write_io * io_write_file2(struct stream_write_io * io, const char * filename, int option, int blockSize) {
	if (!filename)
		return 0;

	int fd = open(filename, O_WRONLY | option, 0644);
	if (fd < 0)
		return 0;

	return io_write_fd2(io, fd, blockSize);
}

int io_write_flush(struct stream_write_io * io) {
	if (!io || !io->data)
		return -1;

	struct io_write_private * self = io->data;
	return fsync(self->fd);
}

void io_write_free(struct stream_write_io * io) {
	if (!io)
		return;

	struct io_write_private * self = io->data;
	if (self->buffer)
		free(self->buffer);
	self->buffer = 0;

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

int io_write_write_buffer(struct stream_write_io * io, const void * buffer, int length) {
	if (!io || !io->data || !buffer || length < 0)
		return -1;

	struct io_write_private * self = io->data;

	const char * ptr = buffer;
	int nbTotalWrite = 0;
	if (self->bufferUsed > 0) {
		if (self->bufferUsed + length < self->blockSize) {
			memcpy(self->buffer + self->bufferUsed, buffer, length);
			self->bufferUsed += length;
			return length;
		} else {
			int remain = self->blockSize - self->bufferUsed;
			memcpy(self->buffer + self->bufferUsed, buffer, remain);
			int nbWrite = write(self->fd, self->buffer, self->blockSize);
			if (nbWrite == self->blockSize) {
				self->bufferUsed = 0;
				length -= nbTotalWrite;
				ptr += remain;
			} else if (nbWrite == 0) {
				self->bufferUsed = self->blockSize;
				return remain;
			} else if (nbWrite < 0) {
				return nbWrite;
			} else {
				memmove(self->buffer, self->buffer + nbWrite, self->blockSize - nbWrite);
				self->bufferUsed -= nbWrite;
				return remain;
			}
		}
	}

	while (length >= self->blockSize) {
		int nbWrite = write(self->fd, ptr, self->blockSize);
		if (nbWrite == self->blockSize) {
			nbTotalWrite += self->blockSize;
			length -= self->blockSize;
			ptr += self->blockSize;
		} else if (nbWrite == 0) {
			return nbTotalWrite;
		} else if (nbWrite < 0) {
			return nbWrite;
		} else {
			self->bufferUsed = self->blockSize - nbWrite;
			memmove(self->buffer, ptr + nbWrite, self->bufferUsed);
			nbTotalWrite += nbWrite;
			return nbTotalWrite;
		}
	}

	if (length > 0) {
		memcpy(self->buffer, ptr, length);
		self->bufferUsed = length;
		nbTotalWrite += length;
	}

	return nbTotalWrite;
}

