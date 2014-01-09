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
*  Last modified: Fri, 22 Oct 2010 13:43:31 +0200                       *
\***********************************************************************/

// free, malloc
#include <malloc.h>
// strchr
#include <string.h>
// inflate, inflateEnd, inflateInit2
#include <zlib.h>

#include "common.h"

struct io_gzip_read_private {
	z_stream gz_stream;
	struct stream_read_io * stream;

	char bufferIn[1024];
};

static int io_gzip_read_close(struct stream_read_io * io);
static void io_gzip_read_free(struct stream_read_io * io);
static int io_gzip_read_read(struct stream_read_io * io, void * buffer, int length);

static struct stream_read_io_ops io_gzip_read_ops = {
	.close = io_gzip_read_close,
	.free = io_gzip_read_free,
	.read = io_gzip_read_read,
};


int io_gzip_read_close(struct stream_read_io * io) {
	if (!io || io->data)
		return -1;

	struct io_gzip_read_private * self = io->data;
	inflateEnd(&self->gz_stream);
	return self->stream->ops->close(self->stream);;
}

void io_gzip_read_free(struct stream_read_io * io) {
	if (!io || io->data)
		return;

	if (io->data)
		free(io->data);
	io->data = 0;
	io->ops = 0;
}

struct stream_read_io * io_gzip_new_streamRead(struct stream_read_io * io, struct stream_read_io * to) {
	if (!to)
		return 0;

	struct io_gzip_read_private * self = malloc(sizeof(struct io_gzip_read_private));
	self->stream = to;

	int nbRead = to->ops->read(to, self->bufferIn, 1024);

	if (self->bufferIn[0] != 0x1f || ((unsigned char) self->bufferIn[1]) != 0x8b) {
		free(self);
		return 0;
	}

	// skip gzip header
	unsigned char flag = self->bufferIn[3];
	char * ptr = self->bufferIn + 10;
	if (flag & 0x4) {
		unsigned short length = ptr[0] << 8 | ptr[1];
		ptr += length + 2;
	}
	if (flag & 0x8) {
		char * end = strchr(ptr, 0);
		if (end)
			ptr = end + 1;
	}
	if (flag & 0x10) {
		char * end = strchr(ptr, 0);
		if (end)
			ptr = end + 1;
	}
	if (flag & 0x2)
		ptr += 2;

	// init data
	self->gz_stream.zalloc = 0;
	self->gz_stream.zfree = 0;
	self->gz_stream.opaque = 0;

	self->gz_stream.next_in = (unsigned char *) ptr;
	self->gz_stream.avail_in = nbRead - (ptr - self->bufferIn);

	int err = inflateInit2(&self->gz_stream, -MAX_WBITS);
	if (err < 0) {
		free(self);
		return 0;
	}

	if (!io)
		io = malloc(sizeof(struct stream_read_io));
	io->ops = &io_gzip_read_ops;
	io->data = self;

	return io;
}

int io_gzip_read_read(struct stream_read_io * io, void * buffer, int length) {
	if (!io || !io->data || !buffer || length < 0)
		return -1;

	struct io_gzip_read_private * self = io->data;

	self->gz_stream.next_out = buffer;
	self->gz_stream.avail_out = length;
	self->gz_stream.total_out = 0;

	while (self->gz_stream.avail_out > 0) {
		if (self->gz_stream.avail_in > 0) {
			int err = inflate(&self->gz_stream, Z_SYNC_FLUSH);
			if (err == Z_STREAM_END || self->gz_stream.avail_out == 0)
				return self->gz_stream.total_out;
		}

		int nbRead = self->stream->ops->read(self->stream, self->bufferIn + self->gz_stream.avail_in, 1024 - self->gz_stream.avail_in);
		if (nbRead > 0) {
			self->gz_stream.next_in = (unsigned char *) self->bufferIn;
			self->gz_stream.avail_in = nbRead;
			self->gz_stream.total_in = 0;
		} else if (nbRead == 0) {
			return self->gz_stream.total_out;
		} else {
			return nbRead;
		}
	}

	return self->gz_stream.total_out;
}

