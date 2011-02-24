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
*  Last modified: Thu, 24 Feb 2011 21:13:57 +0100                       *
\***********************************************************************/

// calloc, free, malloc
#include <malloc.h>
// strdup
#include <string.h>

#include <storiqArchiver/checksum.h>
#include <storiqArchiver/io.h>
#include <storiqArchiver/util/hashtable.h>

#include "../util/util.h"

struct io_checksum_private {
	struct stream_read_io * to;
	struct checksum * checksums;
	unsigned int nbChecksums;
};

static int io_checksum_read_close(struct stream_read_io * io);
static void io_checksum_read_free(struct stream_read_io * io);
static long long int io_checksum_read_position(struct stream_read_io * io);
static int io_checksum_read_read(struct stream_read_io * io, void * buffer, int length);
static int io_checksum_read_reopen(struct stream_read_io * io);

static struct stream_read_io_ops io_checksum_ops = {
	.close		= io_checksum_read_close,
	.free		= io_checksum_read_free,
	.position	= io_checksum_read_position,
	.read		= io_checksum_read_read,
	.reopen		= io_checksum_read_reopen,
};


struct hashtable * io_checksum_completeDigest(struct stream_read_io * io) {
	struct io_checksum_private * self = io->data;
	struct hashtable * digests = hashtable_new2(util_hashString, util_freeKeyValue);

	unsigned int i;
	for (i = 0; i < self->nbChecksums; i++) {
		char * hash = strdup(self->checksums[i].driver->name);

		struct checksum digest;
		self->checksums[i].ops->clone(&digest, self->checksums + i);
		char * result = digest.ops->digest(&digest);

		hashtable_put(digests, hash, result);
	}

	return digests;
}

int io_checksum_read_close(struct stream_read_io * io) {
	struct io_checksum_private * self = io->data;
	return self->to->ops->close(self->to);
}

void io_checksum_read_free(struct stream_read_io * io) {
	if (!io)
		return;

	if (io->data) {
		struct io_checksum_private * self = io->data;

		unsigned int i;
		for (i = 0; i < self->nbChecksums; i++)
			self->checksums[i].ops->free(self->checksums + i);
		free(self->checksums);
		self->checksums = 0;

		free(self);
		io->data = 0;
	}
}

struct stream_read_io * io_checksum_read_new(struct stream_read_io * io, struct stream_read_io * to, char ** checksums, unsigned int nbChecksums) {
	if (!to || !checksums || nbChecksums == 0)
		return 0;

	if (!io)
		io = malloc(sizeof(struct stream_read_io));

	struct io_checksum_private * self = malloc(sizeof(struct io_checksum_private));
	self->to = to;
	self->checksums = calloc(nbChecksums, sizeof(struct checksum));
	for (self->nbChecksums = 0; self->nbChecksums < nbChecksums; self->nbChecksums++) {
		struct checksum_driver * driver = checksum_getDriver(checksums[self->nbChecksums]);
		driver->new_checksum(self->checksums + self->nbChecksums);
	}

	io->ops = &io_checksum_ops;
	io->data = self;
	return io;
}

long long int io_checksum_read_position(struct stream_read_io * io) {
	struct io_checksum_private * self = io->data;
	return self->to->ops->position(self->to);
}

int io_checksum_read_read(struct stream_read_io * io, void * buffer, int length) {
	struct io_checksum_private * self = io->data;

	int nbRead = self->to->ops->read(self->to, buffer, length);

	if (nbRead > 0) {
		unsigned int i;
		for (i = 0; i < self->nbChecksums; i++)
			self->checksums[i].ops->update(self->checksums + i, buffer, length);
	}

	return nbRead;
}

int io_checksum_read_reopen(struct stream_read_io * io) {
	struct io_checksum_private * self = io->data;
	return self->to->ops->reopen(self->to);
}

