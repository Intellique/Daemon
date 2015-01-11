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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

// free, malloc
#include <stdlib.h>

#include <libstoriqone-job/io.h>

#include "checksum.h"

struct soj_stream_checksum_reader_private {
	struct so_stream_reader * in;
	bool closed;

	struct soj_stream_checksum_backend * worker;
};

static int soj_checksum_reader_close(struct so_stream_reader * sr);
static bool soj_checksum_reader_end_of_file(struct so_stream_reader * sr);
static off_t soj_checksum_reader_forward(struct so_stream_reader * sr, off_t offset);
static void soj_checksum_reader_free(struct so_stream_reader * sr);
static ssize_t soj_checksum_reader_get_block_size(struct so_stream_reader * sr);
static int soj_checksum_reader_last_errno(struct so_stream_reader * sr);
static ssize_t soj_checksum_reader_position(struct so_stream_reader * sr);
static ssize_t soj_checksum_reader_read(struct so_stream_reader * sr, void * buffer, ssize_t length);

static struct so_stream_reader_ops soj_checksum_reader_ops = {
	.close          = soj_checksum_reader_close,
	.end_of_file    = soj_checksum_reader_end_of_file,
	.forward        = soj_checksum_reader_forward,
	.free           = soj_checksum_reader_free,
	.get_block_size = soj_checksum_reader_get_block_size,
	.last_errno     = soj_checksum_reader_last_errno,
	.position       = soj_checksum_reader_position,
	.read           = soj_checksum_reader_read,
};


static int soj_checksum_reader_close(struct so_stream_reader * sr) {
	struct soj_stream_checksum_reader_private * self = sr->data;

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

static bool soj_checksum_reader_end_of_file(struct so_stream_reader * sr) {
	struct soj_stream_checksum_reader_private * self = sr->data;
	return self->in->ops->end_of_file(self->in);
}

static off_t soj_checksum_reader_forward(struct so_stream_reader * sr, off_t offset) {
	struct soj_stream_checksum_reader_private * self = sr->data;

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

static void soj_checksum_reader_free(struct so_stream_reader * sr) {
	struct soj_stream_checksum_reader_private * self = sr->data;

	if (!self->closed)
		self->in->ops->close(self->in);

	if (self->in != NULL)
		self->in->ops->free(self->in);

	self->worker->ops->free(self->worker);
	free(self);
	free(sr);
}

static ssize_t soj_checksum_reader_get_block_size(struct so_stream_reader * sr) {
	struct soj_stream_checksum_reader_private * self = sr->data;
	return self->in->ops->get_block_size(self->in);
}

struct so_value * soj_checksum_reader_get_checksums(struct so_stream_reader * sr) {
	struct soj_stream_checksum_reader_private * self = sr->data;
	return self->worker->ops->digest(self->worker);
}

static int soj_checksum_reader_last_errno(struct so_stream_reader * sr) {
	struct soj_stream_checksum_reader_private * self = sr->data;
	return self->in->ops->last_errno(self->in);
}

struct so_stream_reader * soj_checksum_reader_new(struct so_stream_reader * stream, struct so_value * checksums, bool thread_helper) {
	struct soj_stream_checksum_reader_private * self = malloc(sizeof(struct soj_stream_checksum_reader_private));
	self->in = stream;
	self->closed = false;

	if (thread_helper)
		self->worker = soj_stream_checksum_threaded_backend_new(checksums);
	else
		self->worker = soj_stream_checksum_backend_new(checksums);

	struct so_stream_reader * reader = malloc(sizeof(struct so_stream_reader));
	reader->ops = &soj_checksum_reader_ops;
	reader->data = self;

	return reader;
}

static ssize_t soj_checksum_reader_position(struct so_stream_reader * sr) {
	struct soj_stream_checksum_reader_private * self = sr->data;
	return self->in->ops->position(self->in);
}

static ssize_t soj_checksum_reader_read(struct so_stream_reader * sr, void * buffer, ssize_t length) {
	struct soj_stream_checksum_reader_private * self = sr->data;

	if (self->closed)
		return 0;

	ssize_t nb_read = self->in->ops->read(self->in, buffer, length);

	if (nb_read > 0)
		self->worker->ops->update(self->worker, buffer, nb_read);

	return nb_read;
}

