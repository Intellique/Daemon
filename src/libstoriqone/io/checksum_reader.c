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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// free, malloc
#include <stdlib.h>

#include <libstoriqone/io.h>

#include "checksum.h"

struct so_io_stream_checksum_reader_private {
	struct so_stream_reader * in;
	bool closed;

	struct so_io_stream_checksum_backend * worker;
};

static int so_io_checksum_reader_close(struct so_stream_reader * sr);
static bool so_io_checksum_reader_end_of_file(struct so_stream_reader * sr);
static off_t so_io_checksum_reader_forward(struct so_stream_reader * sr, off_t offset);
static void so_io_checksum_reader_free(struct so_stream_reader * sr);
static ssize_t so_io_checksum_reader_get_available_size(struct so_stream_reader * sr);
static ssize_t so_io_checksum_reader_get_block_size(struct so_stream_reader * sr);
static int so_io_checksum_reader_last_errno(struct so_stream_reader * sr);
static ssize_t so_io_checksum_reader_peek(struct so_stream_reader * sr, void * buffer, ssize_t length);
static ssize_t so_io_checksum_reader_position(struct so_stream_reader * sr);
static ssize_t so_io_checksum_reader_read(struct so_stream_reader * sr, void * buffer, ssize_t length);
static int so_io_checksum_reader_rewind(struct so_stream_reader * sr);

static struct so_stream_reader_ops so_io_checksum_reader_ops = {
	.close              = so_io_checksum_reader_close,
	.end_of_file        = so_io_checksum_reader_end_of_file,
	.forward            = so_io_checksum_reader_forward,
	.free               = so_io_checksum_reader_free,
	.get_available_size = so_io_checksum_reader_get_available_size,
	.get_block_size     = so_io_checksum_reader_get_block_size,
	.last_errno         = so_io_checksum_reader_last_errno,
	.peek               = so_io_checksum_reader_peek,
	.position           = so_io_checksum_reader_position,
	.read               = so_io_checksum_reader_read,
	.rewind             = so_io_checksum_reader_rewind,
};


static int so_io_checksum_reader_close(struct so_stream_reader * sr) {
	struct so_io_stream_checksum_reader_private * self = sr->data;

	if (self->closed)
		return 0;

	int failed = 0;
	if (self->in != NULL) {
		failed = self->in->ops->close(self->in);
	}

	if (!failed) {
		self->closed = true;
		self->worker->ops->finish(self->worker);
	}

	return failed;
}

static bool so_io_checksum_reader_end_of_file(struct so_stream_reader * sr) {
	struct so_io_stream_checksum_reader_private * self = sr->data;
	return self->in->ops->end_of_file(self->in);
}

static off_t so_io_checksum_reader_forward(struct so_stream_reader * sr, off_t offset) {
	struct so_io_stream_checksum_reader_private * self = sr->data;

	off_t nb_total_read = 0;
	while (nb_total_read < offset) {
		ssize_t will_read = nb_total_read + 4096 > offset ? offset - nb_total_read : 4096;
		char buffer[4096];

		ssize_t nb_read = self->in->ops->read(self->in, buffer, will_read);
		if (nb_read < 0)
			return -1;

		if (nb_read == 0)
			break;

		self->worker->ops->update(self->worker, buffer, nb_read);
		nb_total_read += nb_read;
	}

	return self->in->ops->position(self->in);
}

static void so_io_checksum_reader_free(struct so_stream_reader * sr) {
	struct so_io_stream_checksum_reader_private * self = sr->data;

	if (!self->closed)
		self->in->ops->close(self->in);

	if (self->in != NULL)
		self->in->ops->free(self->in);

	self->worker->ops->free(self->worker);
	free(self);
	free(sr);
}

static ssize_t so_io_checksum_reader_get_block_size(struct so_stream_reader * sr) {
	struct so_io_stream_checksum_reader_private * self = sr->data;
	return self->in->ops->get_block_size(self->in);
}

static ssize_t so_io_checksum_reader_get_available_size(struct so_stream_reader * sr) {
	struct so_io_stream_checksum_reader_private * self = sr->data;
	return self->in->ops->get_available_size(self->in);
}

struct so_value * so_io_checksum_reader_get_checksums(struct so_stream_reader * sr) {
	struct so_io_stream_checksum_reader_private * self = sr->data;
	return self->worker->ops->digest(self->worker);
}

static int so_io_checksum_reader_last_errno(struct so_stream_reader * sr) {
	struct so_io_stream_checksum_reader_private * self = sr->data;
	return self->in->ops->last_errno(self->in);
}

struct so_stream_reader * so_io_checksum_reader_new(struct so_stream_reader * stream, struct so_value * checksums, bool thread_helper) {
	struct so_io_stream_checksum_reader_private * self = malloc(sizeof(struct so_io_stream_checksum_reader_private));
	self->in = stream;
	self->closed = false;

	if (thread_helper)
		self->worker = so_io_stream_checksum_threaded_backend_new(checksums);
	else
		self->worker = so_io_stream_checksum_backend_new(checksums);

	struct so_stream_reader * reader = malloc(sizeof(struct so_stream_reader));
	reader->ops = &so_io_checksum_reader_ops;
	reader->data = self;

	return reader;
}

static ssize_t so_io_checksum_reader_peek(struct so_stream_reader * sr, void * buffer, ssize_t length) {
	struct so_io_stream_checksum_reader_private * self = sr->data;
	return self->in->ops->peek(self->in, buffer, length);
}

static ssize_t so_io_checksum_reader_position(struct so_stream_reader * sr) {
	struct so_io_stream_checksum_reader_private * self = sr->data;
	return self->in->ops->position(self->in);
}

static ssize_t so_io_checksum_reader_read(struct so_stream_reader * sr, void * buffer, ssize_t length) {
	struct so_io_stream_checksum_reader_private * self = sr->data;

	if (self->closed)
		return 0;

	ssize_t nb_read = self->in->ops->read(self->in, buffer, length);

	if (nb_read > 0)
		self->worker->ops->update(self->worker, buffer, nb_read);

	return nb_read;
}

static int so_io_checksum_reader_rewind(struct so_stream_reader * sr) {
	struct so_io_stream_checksum_reader_private * self = sr->data;

	if (self->closed)
		return 0;

	int failed = self->in->ops->rewind(self->in);
	if (failed == 0)
		self->worker->ops->reset(self->worker);

	return failed;
}

