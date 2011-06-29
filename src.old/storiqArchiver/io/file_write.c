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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Thu, 24 Feb 2011 09:14:14 +0100                       *
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
// close, fsync, write
#include <unistd.h>

#include <storiqArchiver/io.h>

struct io_file_write_private {
	int fd;
	char * buffer;
	int bufferUsed;
	int blockSize;
	long long int position;
};

static int io_file_write_close(struct stream_write_io * io);
static int io_file_write_flush(struct stream_write_io * io);
static int io_file_write_flush_buffer(struct stream_write_io * io);
static void io_file_write_free(struct stream_write_io * io);
static long long int io_file_write_position(struct stream_write_io * io);
static int io_file_write_write(struct stream_write_io * io, const void * buffer, int length);
static int io_file_write_write_buffer(struct stream_write_io * io, const void * buffer, int length);

static struct stream_write_io_ops io_file_write_ops = {
	.close		= io_file_write_close,
	.flush		= io_file_write_flush,
	.free		= io_file_write_free,
	.position	= io_file_write_position,
	.write		= io_file_write_write,
};

static struct stream_write_io_ops io_file_write_buffer_ops = {
	.close		= io_file_write_close,
	.flush		= io_file_write_flush_buffer,
	.free		= io_file_write_free,
	.position	= io_file_write_position,
	.write		= io_file_write_write_buffer,
};


int io_file_write_close(struct stream_write_io * io) {
	if (!io || !io->data)
		return -1;

	struct io_file_write_private * self = io->data;
	if (self->bufferUsed > 0) {
		bzero(self->buffer + self->bufferUsed, self->blockSize - self->bufferUsed);
		write(self->fd, self->buffer, self->blockSize);
	}
	close(self->fd);
	return 0;
}

struct stream_write_io * io_file_write_fd(struct stream_write_io * io, int fd) {
	return io_file_write_fd2(io, fd, 0);
}

struct stream_write_io * io_file_write_fd2(struct stream_write_io * io, int fd, int blockSize) {
	if (fd < 0 || blockSize < 0)
		return 0;

	if (!io)
		io = malloc(sizeof(struct stream_write_io));

	struct io_file_write_private * self = malloc(sizeof(struct io_file_write_private));
	self->fd = fd;
	self->buffer = 0;
	if (blockSize > 0)
		self->buffer = malloc(blockSize);
	self->bufferUsed = 0;
	self->blockSize = blockSize;
	self->position = 0;

	if (blockSize > 0)
		io->ops = &io_file_write_buffer_ops;
	else
		io->ops = &io_file_write_ops;
	io->data = self;

	return io;
}

struct stream_write_io * io_file_write_file(struct stream_write_io * io, const char * filename, int option) {
	return io_file_write_file2(io, filename, option, 0);
}

struct stream_write_io * io_file_write_file2(struct stream_write_io * io, const char * filename, int option, int blockSize) {
	if (!filename)
		return 0;

	int fd = open(filename, O_WRONLY | option, 0644);
	if (fd < 0)
		return 0;

	return io_file_write_fd2(io, fd, blockSize);
}

int io_file_write_flush(struct stream_write_io * io) {
	if (!io || !io->data)
		return -1;

	struct io_file_write_private * self = io->data;
	return fsync(self->fd);
}

int io_file_write_flush_buffer(struct stream_write_io * io) {
	if (!io || !io->data)
		return -1;

	struct io_file_write_private * self = io->data;
	if (self->bufferUsed > 0) {
		int nbWrite = write(self->fd, self->buffer, self->bufferUsed);
		if (nbWrite < 0)
			return nbWrite;

		// suppose that nbWrite = self->bufferUsed
		self->bufferUsed = 0;
	}
	return fsync(self->fd);
}

void io_file_write_free(struct stream_write_io * io) {
	if (!io)
		return;

	struct io_file_write_private * self = io->data;
	if (self->buffer)
		free(self->buffer);
	self->buffer = 0;

	if (io->data)
		free(io->data);
	io->data = 0;
	io->ops = 0;
}

long long int io_file_write_position(struct stream_write_io * io) {
	if (!io || !io->data)
		return -1;

	struct io_file_write_private * self = io->data;
	return self->position;
}

int io_file_write_write(struct stream_write_io * io, const void * buffer, int length) {
	if (!io || !io->data || !buffer || length < 0)
		return -1;

	struct io_file_write_private * self = io->data;
	int nbWrite =  write(self->fd, buffer, length);
	if (nbWrite > 0)
		self->position += nbWrite;
	return nbWrite;
}

int io_file_write_write_buffer(struct stream_write_io * io, const void * buffer, int length) {
	if (!io || !io->data || !buffer || length < 0)
		return -1;

	struct io_file_write_private * self = io->data;

	if (self->blockSize >= length - self->bufferUsed) {
		memcpy(self->buffer + self->bufferUsed, buffer, length);
		self->bufferUsed += length;
		self->position += length;
		return length;
	}

	const char * ptr = buffer;
	if (self->bufferUsed > 0) {
		int writeSize = self->bufferUsed + length;
		writeSize -= writeSize % self->blockSize;

		char * tmp = malloc(writeSize);
		memcpy(tmp, self->buffer, self->bufferUsed);
		memcpy(tmp + self->bufferUsed, buffer, writeSize - self->bufferUsed);

		int nbWrite = write(self->fd, tmp, writeSize);

		if (nbWrite < 0) {
			free(tmp);
			return nbWrite;
		}

		// suppose that nbWrite = writeSize
		self->bufferUsed += length - writeSize;
		self->position += length;
		if (self->bufferUsed > 0)
			memcpy(self->buffer, ptr + writeSize, self->bufferUsed);

		free(tmp);
		return length;
	} else {
		if (length % self->blockSize == 0) {
			int nbWrite = write(self->fd, buffer, length);
			if (nbWrite > 0)
				self->position += nbWrite;
			return nbWrite;
		}

		int writeSize = length - (length % self->blockSize);
		int nbWrite = write(self->fd, buffer, writeSize);

		if (nbWrite < 0)
			return nbWrite;

		// suppose that nbWrite = writeSize
		memcpy(self->buffer, ptr + nbWrite, length - nbWrite);
		if (nbWrite > 0)
			self->position += nbWrite;
		return length;
	}
}

