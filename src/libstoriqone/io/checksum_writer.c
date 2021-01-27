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
#include <libstoriqone/value.h>

#include "checksum.h"

struct so_io_stream_checksum_writer_private {
	struct so_value * checksums;
	bool thread_helper;

	struct so_stream_writer * out;
	bool closed;

	struct so_io_stream_checksum_backend * worker;
};

static ssize_t so_io_checksum_writer_before_close(struct so_stream_writer * sw, void * buffer, ssize_t length);
static int so_io_checksum_writer_close(struct so_stream_writer * sw);
static int so_io_checksum_writer_create_new_file(struct so_stream_writer * sw);
static int so_io_checksum_writer_file_position(struct so_stream_writer * sw);
static void so_io_checksum_writer_free(struct so_stream_writer * sw);
static ssize_t so_io_checksum_writer_get_available_size(struct so_stream_writer * sw);
static ssize_t so_io_checksum_writer_get_block_size(struct so_stream_writer * sw);
static int so_io_checksum_writer_last_errno(struct so_stream_writer * sw);
static ssize_t so_io_checksum_writer_position(struct so_stream_writer * sw);
static struct so_stream_reader * so_io_checksum_writer_reopen(struct so_stream_writer * sw);
static ssize_t so_io_checksum_writer_write(struct so_stream_writer * sw, const void * buffer, ssize_t length);

static struct so_stream_writer_ops so_io_writer_ops = {
	.before_close       = so_io_checksum_writer_before_close,
	.close              = so_io_checksum_writer_close,
	.create_new_file    = so_io_checksum_writer_create_new_file,
	.file_position      = so_io_checksum_writer_file_position,
	.free               = so_io_checksum_writer_free,
	.get_available_size = so_io_checksum_writer_get_available_size,
	.get_block_size     = so_io_checksum_writer_get_block_size,
	.last_errno         = so_io_checksum_writer_last_errno,
	.position           = so_io_checksum_writer_position,
	.reopen             = so_io_checksum_writer_reopen,
	.write              = so_io_checksum_writer_write,
};


static ssize_t so_io_checksum_writer_before_close(struct so_stream_writer * sw, void * buffer, ssize_t length) {
	struct so_io_stream_checksum_writer_private * self = sw->data;

	ssize_t nb_read = self->out->ops->before_close(self->out, buffer, length);

	if (nb_read > 0)
		self->worker->ops->update(self->worker, buffer, nb_read);

	return nb_read;
}

static int so_io_checksum_writer_close(struct so_stream_writer * sw) {
	struct so_io_stream_checksum_writer_private * self = sw->data;

	if (self->closed)
		return 0;

	int failed = 0;
	if (self->out != NULL) {
		char buffer[4096];
		ssize_t nb_read;
		while (nb_read = self->out->ops->before_close(self->out, buffer, 4096), nb_read > 0)
			self->worker->ops->update(self->worker, buffer, nb_read);

		if (nb_read < 0)
			return -1;

		failed = self->out->ops->close(self->out);
	}

	if (!failed) {
		self->closed = true;
		self->worker->ops->finish(self->worker);
	}

	return failed;
}

static int so_io_checksum_writer_create_new_file(struct so_stream_writer * sw) {
	struct so_io_stream_checksum_writer_private * self = sw->data;
	self->worker->ops->reset(self->worker);

	int failed = 0;
	if (self->out != NULL)
		failed = self->out->ops->create_new_file(self->out);

	if (failed == 0) {
		self->closed = false;
	}

	return failed;
}

static int so_io_checksum_writer_file_position(struct so_stream_writer * sw) {
	struct so_io_stream_checksum_writer_private * self = sw->data;
	return self->out->ops->file_position(self->out);
}

static void so_io_checksum_writer_free(struct so_stream_writer * sw) {
	struct so_io_stream_checksum_writer_private * self = sw->data;

	so_value_free(self->checksums);

	if (!self->closed)
		self->out->ops->close(self->out);

	if (self->out != NULL)
		self->out->ops->free(self->out);

	self->worker->ops->free(self->worker);
	free(self);
	free(sw);
}

static ssize_t so_io_checksum_writer_get_available_size(struct so_stream_writer * sw) {
	struct so_io_stream_checksum_writer_private * self = sw->data;
	if (self->out != NULL)
		return self->out->ops->get_available_size(self->out);
	return 0;
}

static ssize_t so_io_checksum_writer_get_block_size(struct so_stream_writer * sw) {
	struct so_io_stream_checksum_writer_private * self = sw->data;
	if (self->out != NULL)
		return self->out->ops->get_block_size(self->out);
	return 0;
}

struct so_value * so_io_checksum_writer_get_checksums(struct so_stream_writer * sw) {
	struct so_io_stream_checksum_writer_private * self = sw->data;
	return self->worker->ops->digest(self->worker);
}

static int so_io_checksum_writer_last_errno(struct so_stream_writer * sw) {
	struct so_io_stream_checksum_writer_private * self = sw->data;
	if (self->out != NULL)
		return self->out->ops->last_errno(self->out);
	return 0;
}

struct so_stream_writer * so_io_checksum_writer_new(struct so_stream_writer * stream, struct so_value * checksums, bool thread_helper) {
	struct so_io_stream_checksum_writer_private * self = malloc(sizeof(struct so_io_stream_checksum_writer_private));
	self->checksums = so_value_share(checksums);
	self->thread_helper = thread_helper;
	self->out = stream;
	self->closed = false;

	if (thread_helper)
		self->worker = so_io_stream_checksum_threaded_backend_new(checksums);
	else
		self->worker = so_io_stream_checksum_backend_new(checksums);

	struct so_stream_writer * writer = malloc(sizeof(struct so_stream_writer));
	writer->ops = &so_io_writer_ops;
	writer->data = self;

	return writer;
}

static ssize_t so_io_checksum_writer_position(struct so_stream_writer * sw) {
	struct so_io_stream_checksum_writer_private * self = sw->data;
	if (self->out != NULL)
		return self->out->ops->position(self->out);
	return 0;
}

static struct so_stream_reader * so_io_checksum_writer_reopen(struct so_stream_writer * sw) {
	struct so_io_stream_checksum_writer_private * self = sw->data;

	if (self->closed)
		return NULL;

	if (self->out != NULL) {
		char buffer[4096];
		ssize_t nb_read;
		while (nb_read = self->out->ops->before_close(self->out, buffer, 4096), nb_read > 0)
			self->worker->ops->update(self->worker, buffer, nb_read);

		if (nb_read < 0)
			return NULL;

		struct so_stream_reader * new_in = self->out->ops->reopen(self->out);
		if (new_in != NULL) {
			self->closed = true;
			self->worker->ops->finish(self->worker);

			return so_io_checksum_reader_new(new_in, self->checksums, self->thread_helper);
		}
	}

	return NULL;
}

static ssize_t so_io_checksum_writer_write(struct so_stream_writer * sw, const void * buffer, ssize_t length) {
	struct so_io_stream_checksum_writer_private * self = sw->data;

	if (self->closed)
		return -1;

	ssize_t nb_write = length;
	if (self->out != NULL)
		nb_write = self->out->ops->write(self->out, buffer, length);

	if (nb_write > 0)
		self->worker->ops->update(self->worker, buffer, nb_write);

	return nb_write;
}

