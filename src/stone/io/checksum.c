/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
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
*  Last modified: Mon, 09 Jan 2012 21:47:46 +0100                         *
\*************************************************************************/

// calloc, free, malloc
#include <stdlib.h>

#include <stone/checksum.h>
#include <stone/io/checksum.h>

struct st_io_checksum_reader_private {
    struct st_stream_reader * reader;

    struct st_checksum * checksums;
    char ** digests;
    unsigned int nb_checksums;
};

struct st_io_checksum_writer_private {
    struct st_stream_writer * writer;

    struct st_checksum * checksums;
    char ** digests;
    unsigned int nb_checksums;
	short computed;
};


static int st_io_checksum_reader_close(struct st_stream_reader * io);
static off_t st_io_checksum_reader_forward(struct st_stream_reader * io, off_t offset);
static void st_io_checksum_reader_free(struct st_stream_reader * io);
static ssize_t st_io_checksum_reader_get_block_size(struct st_stream_reader * io);
static int st_io_checksum_reader_last_errno(struct st_stream_reader * io);
static ssize_t st_io_checksum_reader_position(struct st_stream_reader * io);
static ssize_t st_io_checksum_reader_read(struct st_stream_reader * io, void * buffer, ssize_t length);

static int st_io_checksum_writer_close(struct st_stream_writer * io);
static void st_io_checksum_writer_free(struct st_stream_writer * io);
static ssize_t st_io_checksum_writer_get_block_size(struct st_stream_writer * io);
static int st_io_checksum_writer_last_errno(struct st_stream_writer * io);
static ssize_t st_io_checksum_writer_position(struct st_stream_writer * io);
static ssize_t st_io_checksum_writer_write(struct st_stream_writer * io, const void * buffer, ssize_t length);

static struct st_stream_reader_ops st_io_checksum_reader_ops = {
	.close          = st_io_checksum_reader_close,
	.forward        = st_io_checksum_reader_forward,
	.free           = st_io_checksum_reader_free,
	.get_block_size = st_io_checksum_reader_get_block_size,
	.last_errno     = st_io_checksum_reader_last_errno,
	.position       = st_io_checksum_reader_position,
	.read           = st_io_checksum_reader_read,
};

static struct st_stream_writer_ops st_io_checksum_writer_ops = {
	.close          = st_io_checksum_writer_close,
	.free           = st_io_checksum_writer_free,
	.get_block_size = st_io_checksum_writer_get_block_size,
	.last_errno     = st_io_checksum_writer_last_errno,
	.position       = st_io_checksum_writer_position,
	.write          = st_io_checksum_writer_write,
};


char ** st_checksum_get_digest_from_writer(struct st_stream_writer * writer) {
	struct st_io_checksum_writer_private * self = writer->data;
	return self->digests;
}

struct st_stream_reader * st_checksum_get_steam_reader(const char ** checksums, unsigned int nb_checksums, struct st_stream_reader * reader) {
	struct st_io_checksum_reader_private * self = malloc(sizeof(struct st_io_checksum_reader_private));
	self->reader = reader;
	self->checksums = calloc(nb_checksums << 1, sizeof(struct st_checksum));
	self->digests = calloc(nb_checksums, sizeof(char *));
	self->nb_checksums = nb_checksums;

	unsigned int i;
	for (i = 0; i < nb_checksums; i++) {
		struct st_checksum_driver * driver = st_checksum_get_driver(checksums[i]);

		driver->new_checksum(self->checksums + (i << 1));
		st_checksum_get_helper(self->checksums + (i << 1) + 1, self->checksums + (i << 1));
	}

	struct st_stream_reader * r = malloc(sizeof(struct st_stream_reader));
	r->ops = &st_io_checksum_reader_ops;
	r->data = self;

	return r;
}

struct st_stream_writer * st_checksum_get_steam_writer(const char ** checksums, unsigned int nb_checksums, struct st_stream_writer * writer) {
	struct st_io_checksum_writer_private * self = malloc(sizeof(struct st_io_checksum_writer_private));
	self->writer = writer;
	self->checksums = calloc(nb_checksums << 1, sizeof(struct st_checksum));
	self->digests = calloc(nb_checksums, sizeof(char *));
	self->nb_checksums = nb_checksums;
	self->computed = 0;

	unsigned int i;
	for (i = 0; i < nb_checksums; i++) {
		struct st_checksum_driver * driver = st_checksum_get_driver(checksums[i]);

		driver->new_checksum(self->checksums + (i << 1));
		st_checksum_get_helper(self->checksums + (i << 1) + 1, self->checksums + (i << 1));
	}

	struct st_stream_writer * w = malloc(sizeof(struct st_stream_writer));
	w->ops = &st_io_checksum_writer_ops;
	w->data = self;

	return w;
}


int st_io_checksum_reader_close(struct st_stream_reader * io) {
	struct st_io_checksum_reader_private * self = io->data;
	int failed = self->reader->ops->close(self->reader);
	if (!failed) {
		unsigned int i;
		for (i = 0; i < self->nb_checksums; i++)
			self->digests[i] = self->checksums[(i << 1) + 1].ops->digest(self->checksums + (i << 1) + 1);
	}
	return failed;
}

off_t st_io_checksum_reader_forward(struct st_stream_reader * io, off_t offset) {
	struct st_io_checksum_reader_private * self = io->data;
	ssize_t block_size = self->reader->ops->get_block_size(self->reader);

	char * buffer = malloc(block_size);
	ssize_t nb_read = 1;
	while (offset > 0 && nb_read > 0)
		nb_read = st_io_checksum_reader_read(io, buffer, offset > block_size ? block_size : offset);

	// if (nb_read < 0) ??

	free(buffer);

	return self->reader->ops->position(self->reader);
}

void st_io_checksum_reader_free(struct st_stream_reader * io) {
	struct st_io_checksum_reader_private * self = io->data;

	self->reader->ops->free(self->reader);
	free(self->reader);

	unsigned int i;
	for (i = 0; i < self->nb_checksums; i++) {
		self->checksums[(i << 1) + 1].ops->free(self->checksums + (i << 1) + 1);

		if (self->digests[i])
			free(self->digests[i]);
	}
	free(self->checksums);
	free(self->digests);
	free(self);

	io->ops = 0;
	io->data = 0;
}

ssize_t st_io_checksum_reader_get_block_size(struct st_stream_reader * io) {
	struct st_io_checksum_reader_private * self = io->data;
	return self->reader->ops->get_block_size(self->reader);
}

int st_io_checksum_reader_last_errno(struct st_stream_reader * io) {
	struct st_io_checksum_reader_private * self = io->data;
	return self->reader->ops->last_errno(self->reader);
}

ssize_t st_io_checksum_reader_position(struct st_stream_reader * io) {
	struct st_io_checksum_reader_private * self = io->data;
	return self->reader->ops->position(self->reader);
}

ssize_t st_io_checksum_reader_read(struct st_stream_reader * io, void * buffer, ssize_t length) {
	struct st_io_checksum_reader_private * self = io->data;
	ssize_t nb_read = self->reader->ops->read(self->reader, buffer, length);

	if (nb_read < 0)
		return nb_read;

	if (nb_read > 0) {
		unsigned int i;
		for (i = 0; i < self->nb_checksums; i++)
			self->checksums[(i << 1) + 1].ops->update(self->checksums + (i << 1) + 1, buffer, nb_read);
	}

	return nb_read;
}


int st_io_checksum_writer_close(struct st_stream_writer * io) {
	struct st_io_checksum_writer_private * self = io->data;

	int failed = 0;
	if (self->writer)
		failed = self->writer->ops->close(self->writer);

	if (!failed && !self->computed) {
		unsigned int i;
		for (i = 0; i < self->nb_checksums; i++)
			self->digests[i] = self->checksums[(i << 1) + 1].ops->digest(self->checksums + (i << 1) + 1);
		self->computed = 1;
	}
	return failed;
}

void st_io_checksum_writer_free(struct st_stream_writer * io) {
	struct st_io_checksum_writer_private * self = io->data;

	if (self->writer) {
		self->writer->ops->free(self->writer);
		free(self->writer);
	}

	unsigned int i;
	for (i = 0; i < self->nb_checksums; i++) {
		self->checksums[(i << 1) + 1].ops->free(self->checksums + (i << 1) + 1);

		if (self->digests[i])
			free(self->digests[i]);
	}
	free(self->checksums);
	free(self->digests);
	free(self);

	io->ops = 0;
	io->data = 0;
}

ssize_t st_io_checksum_writer_get_block_size(struct st_stream_writer * io) {
	struct st_io_checksum_writer_private * self = io->data;
	if (self->writer)
		return self->writer->ops->get_block_size(self->writer);
	return 0;
}

int st_io_checksum_writer_last_errno(struct st_stream_writer * io) {
	struct st_io_checksum_writer_private * self = io->data;
	if (self->writer)
		return self->writer->ops->last_errno(self->writer);
	return 0;
}

ssize_t st_io_checksum_writer_position(struct st_stream_writer * io) {
	struct st_io_checksum_writer_private * self = io->data;
	if (self->writer)
		return self->writer->ops->position(self->writer);
	return 0;
}

ssize_t st_io_checksum_writer_write(struct st_stream_writer * io, const void * buffer, ssize_t length) {
	struct st_io_checksum_writer_private * self = io->data;

	ssize_t nb_write = length;
	if (self->writer)
		nb_write = self->writer->ops->write(self->writer, buffer, length);

	if (nb_write < 0)
		return nb_write;

	if (nb_write > 0) {
		unsigned int i;
		for (i = 0; i < self->nb_checksums; i++)
			self->checksums[(i << 1) + 1].ops->update(self->checksums + (i << 1) + 1, buffer, nb_write);
	}

	return nb_write;
}

