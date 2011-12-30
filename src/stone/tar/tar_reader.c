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
*  Last modified: Fri, 30 Dec 2011 14:45:46 +0100                         *
\*************************************************************************/

// sscanf, snprintf
#include <stdio.h>
// free, malloc, realloc
#include <stdlib.h>
// memcpy, memmove, memset, strncpy, strncmp
#include <string.h>
// S_*
#include <sys/stat.h>

#include <stone/io.h>

#include "common.h"

struct st_tar_private_in {
	struct st_stream_reader * io;
	off_t position;
	char * buffer;
	unsigned int bufferSize;
	unsigned int bufferUsed;
	ssize_t filesize;
	ssize_t skipsize;
};


static int st_tar_in_check_header(struct st_tar * header);
static int st_tar_in_close(struct st_tar_in * f);
static dev_t st_tar_in_convert_dev(struct st_tar * header);
static gid_t st_tar_in_convert_gid(struct st_tar * header);
static ssize_t st_tar_in_convert_size(struct st_tar * header);
static time_t st_tar_in_convert_time(struct st_tar * header);
static uid_t st_tar_in_convert_uid(struct st_tar * header);
static void st_tar_in_free(struct st_tar_in * f);
static ssize_t st_tar_in_get_block_size(struct st_tar_in * in);
static enum st_tar_header_status st_tar_in_get_header(struct st_tar_in * f, struct st_tar_header * header);
static int st_tar_in_last_errno(struct st_tar_in * f);
static ssize_t st_tar_in_prefetch(struct st_tar_private_in * self, ssize_t length);
static ssize_t st_tar_in_read(struct st_tar_in * f, void * data, ssize_t length);
static ssize_t st_tar_in_read_buffer(struct st_tar_private_in * f, void * data, ssize_t length);
static int st_tar_in_skip_file(struct st_tar_in * f);

static struct st_tar_in_ops st_tar_in_ops = {
	.close          = st_tar_in_close,
	.free           = st_tar_in_free,
	.get_block_size = st_tar_in_get_block_size,
	.get_header     = st_tar_in_get_header,
	.last_errno     = st_tar_in_last_errno,
	.read           = st_tar_in_read,
	.skip_file      = st_tar_in_skip_file,
};


int st_tar_in_check_header(struct st_tar * header) {
	char checksum[8];
	strncpy(checksum, header->checksum, 8);
	memset(header->checksum, ' ', 8);

	unsigned char * ptr = (unsigned char *) header;
	unsigned int i, sum = 0;
	for (i = 0; i < 512; i++)
		sum += ptr[i];
	snprintf(header->checksum, 7, "%06o", sum);

	return strncmp(checksum, header->checksum, 8);
}

int st_tar_in_close(struct st_tar_in * f) {
	struct st_tar_private_in * self = f->data;
	return self->io->ops->close(self->io);
}

dev_t st_tar_in_convert_dev(struct st_tar * header) {
	unsigned int major = 0, minor = 0;

	sscanf(header->devmajor, "%o", &major);
	sscanf(header->devminor, "%o", &minor);

	return (major << 8 ) | minor;
}

gid_t st_tar_in_convert_gid(struct st_tar * header) {
	unsigned int result;
	sscanf(header->gid, "%o", &result);
	return result;
}

ssize_t st_tar_in_convert_size(struct st_tar * header) {
	if (header->size[0] == (char) 0x80) {
		short i;
		ssize_t result = 0;
		for (i = 1; i < 12; i++) {
			result <<= 8;
			result |= header->size[i];
		}
		return result;
	} else {
		unsigned long long result = 0;
		sscanf(header->size, "%llo", &result);
		return result;
	}
	return 0;
}

time_t st_tar_in_convert_time(struct st_tar * header) {
	unsigned int result;
	sscanf(header->mtime, "%o", &result);
	return result;
}

uid_t st_tar_in_convert_uid(struct st_tar * header) {
	unsigned int result;
	sscanf(header->uid, "%o", &result);
	return result;
}

void st_tar_in_free(struct st_tar_in * f) {
	struct st_tar_private_in * self = f->data;

	if (self->io)
		self->io->ops->free(self->io);

	if (self->buffer)
		free(self->buffer);
	self->buffer = 0;

	free(f->data);
	free(f);
}

ssize_t st_tar_in_get_block_size(struct st_tar_in * in) {
	struct st_tar_private_in * self = in->data;
	return self->io->ops->get_block_size(self->io);
}

enum st_tar_header_status st_tar_in_get_header(struct st_tar_in * f, struct st_tar_header * header) {
	struct st_tar_private_in * self = f->data;
	ssize_t nbRead = st_tar_in_prefetch(self, 2560);
	if (nbRead == 0)
		return ST_TAR_HEADER_NOT_FOUND;
	if (nbRead < 0)
		return ST_TAR_HEADER_BAD_HEADER;

	st_tar_init_header(header);

	do {
		char buffer[512];
		struct st_tar * h = (struct st_tar *) buffer;
		nbRead = st_tar_in_read_buffer(self, buffer, 512);

		if (h->filename[0] == '\0')
			return ST_TAR_HEADER_NOT_FOUND;

		// no header found
		if (nbRead < 512)
			return ST_TAR_HEADER_BAD_HEADER;

		if (st_tar_in_check_header(h)) {
			// header checksum failed !!!
			return ST_TAR_HEADER_BAD_CHECKSUM;
		}

		ssize_t next_read;
		switch (h->flag) {
			case 'L':
				next_read = st_tar_in_convert_size(h);
				nbRead = st_tar_in_read_buffer(self, buffer, 512 + next_read - next_read % 512);
				strncpy(header->path, buffer, next_read);
				continue;

			case 'K':
				next_read = st_tar_in_convert_size(h);
				nbRead = st_tar_in_read_buffer(self, buffer, 512 + next_read - next_read % 512);
				strncpy(header->link, buffer, next_read);
				continue;

			case '1':
			case '2':
				if (header->link[0] == 0)
					strncpy(header->link, h->linkname, 256);
				break;

			case '3':
			case '4':
				header->dev = st_tar_in_convert_dev(h);
				break;

			case 'V':
				header->is_label = 1;
				break;
		}

		if (header->path[0] == 0)
			strncpy(header->path, h->filename, 256);
		header->filename = header->path;
		header->size = st_tar_in_convert_size(h);
		sscanf(h->filemode, "%o", &header->mode);
		header->mtime = st_tar_in_convert_time(h);
		header->uid = st_tar_in_convert_uid(h);
		strcpy(header->uname, h->uname);
		header->gid = st_tar_in_convert_gid(h);
		strcpy(header->gname, h->gname);

		switch (h->flag) {
			case '0':
				header->mode |= S_IFREG;
				break;

			case '2':
				header->mode |= S_IFLNK;
				break;

			case '3':
				header->mode |= S_IFCHR;
				break;

			case '4':
				header->mode |= S_IFBLK;
				break;

			case '5':
				header->mode |= S_IFDIR;
				break;

			case '6':
				header->mode |= S_IFIFO;
				break;
		}
	} while (!header->filename);

	self->skipsize = self->filesize = header->size;

	if (header->size > 0 && header->size % 512)
		self->skipsize = 512 + header->size - header->size % 512;

	return ST_TAR_HEADER_OK;
}

int st_tar_in_last_errno(struct st_tar_in * f) {
	struct st_tar_private_in * self = f->data;
	return self->io->ops->last_errno(self->io);
}

ssize_t st_tar_in_prefetch(struct st_tar_private_in * self, ssize_t length) {
	if (self->bufferSize < length) {
		self->buffer = realloc(self->buffer, length);
		self->bufferSize = length;
	}
	if (self->bufferUsed < length) {
		ssize_t nbRead = self->io->ops->read(self->io, self->buffer + self->bufferUsed, length - self->bufferUsed);
		if (nbRead < 0)
			return nbRead;
		if (nbRead > 0)
			self->bufferUsed += nbRead;
	}
	return self->bufferUsed;
}

ssize_t st_tar_in_read(struct st_tar_in * f, void * data, ssize_t length) {
	struct st_tar_private_in * self = f->data;
	if (length > self->filesize)
		length = self->filesize;
	ssize_t nbRead = st_tar_in_read_buffer(self, data, length);
	if (nbRead > 0) {
		self->filesize -= nbRead;
		self->skipsize -= nbRead;
	}
	if (self->filesize == 0 && self->skipsize > 0)
		st_tar_in_skip_file(f);
	return nbRead;
}

ssize_t st_tar_in_read_buffer(struct st_tar_private_in * self, void * data, ssize_t length) {
	ssize_t nbRead = 0;
	if (self->bufferUsed > 0) {
		nbRead = length > self->bufferUsed ? self->bufferUsed : length;
		memcpy(data, self->buffer, nbRead);

		self->bufferUsed -= nbRead;
		if (self->bufferUsed > 0)
			memmove(self->buffer, self->buffer + nbRead, self->bufferUsed);

		if (length == nbRead) {
			self->position += nbRead;
			return nbRead;
		}
	}

	char * pdata = data;
	ssize_t r = self->io->ops->read(self->io, pdata + nbRead, length - nbRead);
	if (r < 0) {
		memcpy(self->buffer, data, nbRead);
		self->bufferUsed = nbRead;
		return r;
	}

	if (self->bufferUsed == 0) {
		free(self->buffer);
		self->buffer = 0;
		self->bufferSize = 0;
	}

	nbRead += r;
	self->position += nbRead;
	return nbRead;
}

int st_tar_in_skip_file(struct st_tar_in * f) {
	struct st_tar_private_in * self = f->data;
	if (self->skipsize == 0)
		return 0;
	if (self->bufferUsed > 0) {
		if (self->skipsize < self->bufferUsed) {
			memmove(self->buffer, self->buffer + self->skipsize, self->bufferUsed - self->skipsize);
			self->bufferUsed -= self->skipsize;
			self->filesize = 0;
			self->skipsize = 0;
			return 0;
		} else {
			self->filesize -= self->bufferUsed;
			self->skipsize -= self->bufferUsed;
			self->bufferUsed = 0;
		}
	}
	if (self->skipsize > 0) {
		off_t next_pos = self->io->ops->position(self->io) + self->skipsize;
		off_t new_pos = self->io->ops->forward(self->io, self->skipsize);
		if (new_pos == (off_t) -1)
			return 1;
		self->filesize = self->skipsize = 0;
		return new_pos != next_pos;
	}
	return 0;
}

struct st_tar_in * st_tar_new_in(struct st_stream_reader * io) {
	if (!io)
		return 0;

	struct st_tar_private_in * data = malloc(sizeof(struct st_tar_private_in));
	data->io = io;
	data->position = 0;
	data->buffer = 0;
	data->bufferSize = 0;
	data->bufferUsed = 0;
	data->filesize = 0;
	data->skipsize = 0;

	struct st_tar_in * self = malloc(sizeof(struct st_tar_in));
	self->ops = &st_tar_in_ops;
	self->data = data;

	return self;
}

