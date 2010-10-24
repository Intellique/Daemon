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
*  Last modified: Sun, 24 Oct 2010 21:28:06 +0200                       *
\***********************************************************************/

// free, malloc
#include <malloc.h>
// snprintf
#include <stdio.h>
// deflate, deflateEnd, deflateInit
#include <zlib.h>

#include "common.h"


struct io_gzip_write_private {
	z_stream gz_stream;
	struct stream_write_io * stream;
	uLong crc32;
};

static int io_gzip_write_close(struct stream_write_io * io);
static int io_gzip_write_flush(struct stream_write_io * io);
static void io_gzip_write_free(struct stream_write_io * io);
static int io_gzip_write_write(struct stream_write_io * io, const void * buffer, int length);

static struct stream_write_io_ops io_gzip_write_ops = {
	.close = io_gzip_write_close,
	.flush = io_gzip_write_flush,
	.free = io_gzip_write_free,
	.write = io_gzip_write_write,
};


int io_gzip_write_close(struct stream_write_io * io) {
	if (!io || !io->data)
		return -1;

	struct io_gzip_write_private * self = io->data;

	char buffer[1024];
	self->gz_stream.next_in = 0;
	self->gz_stream.avail_in = 0;

	int err;
	do {
		self->gz_stream.next_out = (unsigned char *) buffer;
		self->gz_stream.avail_out = 1024;
		self->gz_stream.total_out = 0;

		err = deflate(&self->gz_stream, Z_FINISH);
		if (err >= 0)
			self->stream->ops->write(self->stream, buffer, self->gz_stream.total_out);
	} while (err == Z_OK);

	self->stream->ops->write(self->stream, &self->crc32, 4);
	unsigned int length = self->gz_stream.total_in;
	self->stream->ops->write(self->stream, &length, 4);

	deflateEnd(&self->gz_stream);

	return self->stream->ops->close(self->stream);
}

int io_gzip_write_flush(struct stream_write_io * io) {
	if (!io || !io->data)
		return -1;

	struct io_gzip_write_private * self = io->data;

	char buffer[1024];
	self->gz_stream.next_out = (unsigned char *) buffer;
	self->gz_stream.avail_out = 1024;
	self->gz_stream.total_out = 0;

	deflate(&self->gz_stream, Z_FULL_FLUSH);

	self->stream->ops->write(self->stream, buffer, self->gz_stream.total_out);

	return self->stream->ops->flush(self->stream);
}

void io_gzip_write_free(struct stream_write_io * io) {
	if (!io || !io->data)
		return;

	if (io->data)
		free(io->data);
	io->data = 0;
	io->ops = 0;
}

struct stream_write_io * io_gzip_new_streamWrite(struct stream_write_io * io, struct stream_write_io * to) {
	if (!to)
		return 0;

	struct io_gzip_write_private * self = malloc(sizeof(struct io_gzip_write_private));
	self->stream = to;

	self->gz_stream.zalloc = 0;
	self->gz_stream.zfree = 0;
	self->gz_stream.opaque = 0;

	self->gz_stream.next_in = 0;
	self->gz_stream.total_in = 0;
	self->gz_stream.avail_in = 0;
	self->gz_stream.next_out = 0;
	self->gz_stream.total_out = 0;
	self->gz_stream.avail_out = 0;

	self->crc32 = crc32(0, Z_NULL, 0);

	int err = deflateInit2(&self->gz_stream, 9, Z_DEFLATED, -MAX_WBITS, 9, Z_DEFAULT_STRATEGY);
	if (err) {
		free(self);
		return 0;
	}

	// write header
	char header[11];
	snprintf(header, 11, "%c%c%c%c%c%c%c%c%c%c", 0x1f, 0x8b, 0x8, 0, 0, 0, 0, 0, self->gz_stream.data_type, 3);
	to->ops->write(to, header, 10);

	if (!io)
		io = malloc(sizeof(struct stream_write_io));

	io->data = self;
	io->ops = &io_gzip_write_ops;

	return io;
}

int io_gzip_write_write(struct stream_write_io * io, const void * buffer, int length) {
	if (!io || !io->data || !buffer || length < 0)
		return -1;

	struct io_gzip_write_private * self = io->data;
	char buff[1024];

	self->gz_stream.next_in = (unsigned char *) buffer;
	self->gz_stream.avail_in = length;

	self->crc32 = crc32(self->crc32, buffer, length);

	while (self->gz_stream.avail_in > 0) {
		self->gz_stream.next_out = (unsigned char *) buff;
		self->gz_stream.avail_out = 1024;
		self->gz_stream.total_out = 0;

		int err = deflate(&self->gz_stream, Z_NO_FLUSH);
		if (err >= 0 && self->gz_stream.total_out > 0)
			self->stream->ops->write(self->stream, buff, self->gz_stream.total_out);
	}

	return length;
}

