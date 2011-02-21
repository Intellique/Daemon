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
*  Last modified: Mon, 21 Feb 2011 23:28:38 +0100                       *
\***********************************************************************/

// open
#include <fcntl.h>
// free, malloc
#include <malloc.h>
// memcpy, memmove
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// close, read
#include <unistd.h>

#include <storiqArchiver/io.h>

struct io_read_private {
	int fd;
	char * buffer;
	int bufferUsed;
	int blockSize;
};

static int io_read_close(struct stream_read_io * io);
static void io_read_free(struct stream_read_io * io);
static int io_read_read(struct stream_read_io * io, void * buffer, int length);
static int io_read_read_buffer(struct stream_read_io * io, void * buffer, int length);

static struct stream_read_io_ops io_read_ops = {
	.close	= io_read_close,
	.free	= io_read_free,
	.read	= io_read_read,
};

static struct stream_read_io_ops io_read_buffer_ops = {
	.close	= io_read_close,
	.free	= io_read_free,
	.read	= io_read_read_buffer,
};


int io_read_close(struct stream_read_io * io) {
	if (!io || !io->data)
		return -1;

	struct io_read_private * self = io->data;
	close(self->fd);
	self->fd = -1;
	return 0;
}

struct stream_read_io * io_read_fd(struct stream_read_io * io, int fd) {
	return io_read_fd2(io, fd, 0);
}

struct stream_read_io * io_read_fd2(struct stream_read_io * io, int fd, int blockSize) {
	if (fd < 0 || blockSize < 0)
		return 0;

	if (!io)
		io = malloc(sizeof(struct stream_read_io));

	struct io_read_private * self = malloc(sizeof(struct io_read_private));
	self->fd = fd;
	self->buffer = 0;
	self->bufferUsed = 0;
	if (blockSize > 0)
		self->buffer = malloc(blockSize);
	self->blockSize = blockSize;

	if (blockSize > 0)
		io->ops = &io_read_buffer_ops;
	else
		io->ops = &io_read_ops;
	io->data = self;

	return io;
}

struct stream_read_io * io_read_file(struct stream_read_io * io, const char * filename) {
	return io_read_file2(io, filename, 0);
}

struct stream_read_io * io_read_file2(struct stream_read_io * io, const char * filename, int blockSize) {
	if (!filename)
		return 0;

	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		return 0;

	return io_read_fd2(io, fd, blockSize);
}

void io_read_free(struct stream_read_io * io) {
	if (!io)
		return;

	if (io->data) {
		struct io_read_private * self = io->data;

		if (self->fd >= 0)
			io_read_close(io);

		if (self->buffer)
			free(self->buffer);
		self->buffer = 0;

		free(io->data);
	}
	io->data = 0;
	io->ops = 0;
}

int io_read_read(struct stream_read_io * io, void * buffer, int length) {
	if (!io || !io->data || !buffer || length < 0)
		return -1;

	struct io_read_private * self = io->data;
	return read(self->fd, buffer, length);
}

int io_read_read_buffer(struct stream_read_io * io, void * buffer, int length) {
	if (!io || !io->data || !buffer || length < 0)
		return -1;

	struct io_read_private * self = io->data;

	if (length <= self->bufferUsed) {
		memcpy(buffer, self->buffer, length);
		if (length < self->bufferUsed)
			memmove(self->buffer, self->buffer + length, self->bufferUsed - length);
		self->bufferUsed -= length;
		return length;
	}

	int readSize = length - self->bufferUsed;
	readSize = self->blockSize + readSize - (readSize % self->blockSize);

	char * buf = malloc(readSize);
	int nbRead = read(self->fd, buf, readSize);

	if (nbRead < 0) {
		free(buf);
		return nbRead;
	}

	int nbTotalRead = 0;
	if (self->bufferUsed > 0) {
		memcpy(buffer, self->buffer, self->bufferUsed);
		nbTotalRead += self->bufferUsed;
		length -= self->bufferUsed;
		self->bufferUsed = 0;
	}

	if (nbRead == 0) {
		free(buf);
		return nbTotalRead;
	}

	char * cBuffer = buffer;
	if (length >= nbRead) {
		memcpy(cBuffer + nbTotalRead, buf, nbRead);
		nbTotalRead += nbRead;
		free(buf);
		return nbRead;
	}

	memcpy(cBuffer + nbTotalRead, buf, length);
	nbTotalRead += length;

	self->bufferUsed = nbRead - length;
	memcpy(self->buffer, buf + length, self->bufferUsed);
	free(buf);

	return nbTotalRead;
}

