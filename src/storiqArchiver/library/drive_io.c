/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 28 Nov 2011 11:40:32 +0100                         *
\*************************************************************************/

// free, malloc
#include <stdlib.h>
// memcpy, memmove
#include <string.h>
// read, write
#include <unistd.h>

#include <storiqArchiver/io.h>
#include <storiqArchiver/library/drive.h>

#include "common.h"

struct sa_drive_io_reader {
	int fd;
	char * buffer;
	ssize_t buffer_used;
	ssize_t block_size;
	ssize_t position;
};

struct sa_drive_io_writer {
};

static int sa_drive_io_reader_close(struct sa_stream_reader * io);
static void sa_drive_io_reader_free(struct sa_stream_reader * io);
static ssize_t sa_drive_io_reader_position(struct sa_stream_reader * io);
static ssize_t sa_drive_io_reader_read(struct sa_stream_reader * io, void * buffer, ssize_t length);

static int sa_drive_io_writer_close(struct sa_stream_writer * io);
static void sa_drive_io_writer_free(struct sa_stream_writer * io);
static ssize_t sa_drive_io_writer_position(struct sa_stream_writer * io);
static ssize_t sa_drive_io_writer_write(struct sa_stream_writer * io);

static struct sa_stream_reader_ops sa_drive_io_reader_ops = {
	.close    = sa_drive_io_reader_close,
	.free     = sa_drive_io_reader_free,
	.position = sa_drive_io_reader_position,
	.read     = sa_drive_io_reader_read,
};

static struct sa_stream_writer_ops sa_drive_io_writer_ops = {
	.close    = sa_drive_io_writer_close,
	.free     = sa_drive_io_writer_free,
	.position = sa_drive_io_writer_position,
	.write    = sa_drive_io_writer_write,
};


int sa_drive_io_reader_close(struct sa_stream_reader * io) {
	if (!io || !io->data)
		return -1;

	struct sa_drive_io_reader * self = io->data;
	self->fd = -1;
	self->buffer_used = 0;
	return 0;
}

void sa_drive_io_reader_free(struct sa_stream_reader * io) {
	if (!io)
		return;

	struct sa_drive_io_reader * self = io->data;
	if (self) {
		self->fd = -1;
		free(self->buffer);
		free(self);
	}
	io->data = 0;
	io->ops = 0;
}

struct sa_stream_reader * sa_drive_reader_new(struct sa_drive * drive, int fd) {
	ssize_t block_size = drive->ops->get_block_size(drive);

	struct sa_drive_io_reader * self = malloc(sizeof(struct sa_drive_io_reader));
	self->fd = fd;
	self->buffer = malloc(block_size);
	self->buffer_used = 0;
	self->block_size = block_size;
	self->position = 0;

	struct sa_stream_reader * reader = malloc(sizeof(struct sa_stream_reader));
	reader->ops = &sa_drive_io_reader_ops;
	reader->data = self;

	return reader;
}

ssize_t sa_drive_io_reader_position(struct sa_stream_reader * io) {
	if (!io || !io->data)
		return -1;

	struct sa_drive_io_reader * self = io->data;
	return self->position;
}

ssize_t sa_drive_io_reader_read(struct sa_stream_reader * io, void * buffer, ssize_t length) {
	if (!io || !buffer || length < 0)
		return -1;

	struct sa_drive_io_reader * self = io->data;
	if (self->fd < 0)
		return -1;

	ssize_t nb_total_read = 0;
	if (self->buffer_used > 0) {
		ssize_t nb_copy = self->buffer_used >= length ? length : self->buffer_used;
		memcpy(buffer, self->buffer, nb_copy);

		if (nb_copy < self->buffer_used)
			memmove(self->buffer, self->buffer + nb_copy, self->buffer_used - nb_copy);
		self->buffer_used -= nb_copy;
		self->position += nb_copy;

		if (nb_copy == length)
			return length;

		nb_total_read += nb_copy;
	}

	char * c_buffer = buffer;
	while (length - nb_total_read >= self->block_size) {
		ssize_t nb_read = read(self->fd, c_buffer + nb_total_read, self->block_size);

		if (nb_read > 0) {
			self->position += nb_read;
			nb_total_read += nb_read;
		} else if (nb_read == 0)
			return nb_total_read;
		else
			return -1;
	}

	if (length == nb_total_read)
		return length;

	ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
	if (nb_read < 0)
		return -1;

	if (nb_read > 0) {
		self->buffer_used = nb_read;

		ssize_t nb_copy = length - nb_total_read;
		memcpy(c_buffer + nb_total_read, self->buffer, nb_copy);

		if (nb_copy < self->buffer_used)
			memmove(self->buffer, self->buffer + nb_copy, self->buffer_used - nb_copy);
		self->buffer_used -= nb_copy;
		self->position += nb_copy;

		if (nb_copy == length)
			return length;

		nb_total_read += nb_copy;
	}

	return nb_total_read;
}


int sa_drive_io_writer_close(struct sa_stream_writer * io) {}

void sa_drive_io_writer_free(struct sa_stream_writer * io) {}

struct sa_stream_writer * sa_drive_writer_new(struct sa_drive * drive, int fd) {}

ssize_t sa_drive_io_writer_position(struct sa_stream_writer * io) {}

ssize_t sa_drive_io_writer_write(struct sa_stream_writer * io) {}

