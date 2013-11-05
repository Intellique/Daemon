/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Fri, 18 Oct 2013 11:42:38 +0200                            *
\****************************************************************************/

// sscanf, snprintf
#include <stdio.h>
// free, malloc, realloc
#include <stdlib.h>
// memcpy, memmove, memset, strncpy, strncmp
#include <string.h>
// bzero
#include <string.h>
// S_*
#include <sys/stat.h>

#include <libstone/io.h>

#include "common.h"

struct st_format_tar_reader_private {
	struct st_stream_reader * io;

	char * buffer;
	ssize_t buffer_size;
	ssize_t buffer_used;
	off_t position;

	ssize_t file_size;
	ssize_t skip_size;
};


static int st_format_tar_reader_check_header(struct st_format_tar * header);
static int st_format_tar_reader_close(struct st_format_reader * sfr);
static dev_t st_format_tar_reader_convert_dev(struct st_format_tar * header);
static gid_t st_format_tar_reader_convert_gid(struct st_format_tar * header);
static ssize_t st_format_tar_reader_convert_size(const char * size);
static time_t st_format_tar_reader_convert_time(struct st_format_tar * header);
static uid_t st_format_tar_reader_convert_uid(struct st_format_tar * header);
static bool st_format_tar_reader_end_of_file(struct st_format_reader * sfr);
static enum st_format_reader_header_status st_format_tar_reader_forward(struct st_format_reader * fr, ssize_t block_position);
static void st_format_tar_reader_free(struct st_format_reader * sfr);
static ssize_t st_format_tar_reader_get_block_size(struct st_format_reader * sfr);
static enum st_format_reader_header_status st_format_tar_reader_get_header(struct st_format_reader * sfr, struct st_format_file * header);
static int st_format_tar_reader_last_errno(struct st_format_reader * sfr);
static ssize_t st_format_tar_reader_position(struct st_format_reader * sfr);
static ssize_t st_format_tar_reader_read(struct st_format_reader * sfr, void * data, ssize_t length);
static ssize_t st_format_tar_reader_read_to_end_of_data(struct st_format_reader * sfr);
static enum st_format_reader_header_status st_format_tar_reader_skip_file(struct st_format_reader * sfr);

static struct st_format_reader_ops st_format_tar_reader_ops = {
	.close               = st_format_tar_reader_close,
	.forward             = st_format_tar_reader_forward,
	.free                = st_format_tar_reader_free,
	.end_of_file         = st_format_tar_reader_end_of_file,
	.get_block_size      = st_format_tar_reader_get_block_size,
	.get_header          = st_format_tar_reader_get_header,
	.last_errno          = st_format_tar_reader_last_errno,
	.position            = st_format_tar_reader_position,
	.read                = st_format_tar_reader_read,
	.read_to_end_of_data = st_format_tar_reader_read_to_end_of_data,
	.skip_file           = st_format_tar_reader_skip_file,
};


struct st_format_reader * st_format_tar_get_reader(struct st_stream_reader * sfr) {
	if (sfr == NULL)
		return NULL;

	struct st_format_tar_reader_private * data = malloc(sizeof(struct st_format_tar_reader_private));
	data->io = sfr;
	data->position = 0;
	data->buffer = malloc(512);
	data->buffer_size = 512;
	data->buffer_used = 0;
	data->file_size = 0;
	data->skip_size = 0;

	struct st_format_reader * self = malloc(sizeof(struct st_format_reader));
	self->ops = &st_format_tar_reader_ops;
	self->data = data;

	return self;
}

static int st_format_tar_reader_check_header(struct st_format_tar * header) {
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

static int st_format_tar_reader_close(struct st_format_reader * sfr) {
	struct st_format_tar_reader_private * self = sfr->data;
	return self->io->ops->close(self->io);
}

static dev_t st_format_tar_reader_convert_dev(struct st_format_tar * header) {
	unsigned int major = 0, minor = 0;

	sscanf(header->devmajor, "%o", &major);
	sscanf(header->devminor, "%o", &minor);

	return (major << 8 ) | minor;
}

static gid_t st_format_tar_reader_convert_gid(struct st_format_tar * header) {
	unsigned int result;
	sscanf(header->gid, "%o", &result);
	return result;
}

static ssize_t st_format_tar_reader_convert_size(const char * size) {
	if (size[0] == (char) 0x80) {
		short i;
		ssize_t result = 0;
		for (i = 1; i < 12; i++) {
			result <<= 8;
			result |= (unsigned char) size[i];
		}
		return result;
	} else {
		unsigned long long result = 0;
		sscanf(size, "%llo", &result);
		return result;
	}
	return 0;
}

static time_t st_format_tar_reader_convert_time(struct st_format_tar * header) {
	unsigned int result;
	sscanf(header->mtime, "%o", &result);
	return result;
}

static uid_t st_format_tar_reader_convert_uid(struct st_format_tar * header) {
	unsigned int result;
	sscanf(header->uid, "%o", &result);
	return result;
}

static bool st_format_tar_reader_end_of_file(struct st_format_reader * sfr) {
	struct st_format_tar_reader_private * self = sfr->data;
	return self->io->ops->end_of_file(self->io);
}

static enum st_format_reader_header_status st_format_tar_reader_forward(struct st_format_reader * sfr, ssize_t block_position) {
	struct st_format_tar_reader_private * self = sfr->data;

	ssize_t current_position = self->io->ops->position(self->io);
	ssize_t block_size = self->io->ops->get_block_size(self->io);
	ssize_t current_block = current_position / block_size;
	if (current_block > block_position)
		return st_format_reader_header_not_found;

	if (block_position - current_block < 2)
		return st_format_reader_header_ok;

	off_t next_position = block_position * block_size;
	off_t new_position = self->io->ops->forward(self->io, next_position - current_position);

	if (next_position != new_position)
		return st_format_reader_header_not_found;

	return st_format_reader_header_ok;
}

static void st_format_tar_reader_free(struct st_format_reader * sfr) {
	struct st_format_tar_reader_private * self = sfr->data;

	if (self->io != NULL)
		self->io->ops->free(self->io);
	self->io = NULL;

	if (self->buffer != NULL)
		free(self->buffer);
	self->buffer = NULL;

	free(sfr->data);
	free(sfr);
}

static ssize_t st_format_tar_reader_get_block_size(struct st_format_reader * sfr) {
	struct st_format_tar_reader_private * self = sfr->data;
	return self->io->ops->get_block_size(self->io);
}

static enum st_format_reader_header_status st_format_tar_reader_get_header(struct st_format_reader * sfr, struct st_format_file * header) {
	struct st_format_tar_reader_private * self = sfr->data;

	if (self->io->ops->end_of_file(self->io))
		return st_format_reader_header_not_found;

	ssize_t nb_read = self->io->ops->read(self->io, self->buffer, 512);
	if (nb_read == 0)
		return st_format_reader_header_not_found;
	if (nb_read < 0)
		return st_format_reader_header_bad_header;

	struct st_format_tar * raw_header = (struct st_format_tar *) self->buffer;

	if (st_format_tar_reader_check_header(raw_header))
		return st_format_reader_header_bad_header;

	bzero(header, sizeof(struct st_format_file));

	for (;;) {
		ssize_t next_read;
		switch (raw_header->flag) {
			case 'L':
				next_read = 512 + st_format_tar_reader_convert_size(raw_header->size);
				if (next_read % 512)
					next_read -= next_read % 512;

				if (next_read > self->buffer_size)
					self->buffer = realloc(self->buffer, next_read);

				nb_read = self->io->ops->read(self->io, self->buffer, next_read);
				if (nb_read < next_read) {
					st_format_file_free(header);
					return st_format_reader_header_bad_header;
				}
				header->filename = strdup(self->buffer);

				nb_read = self->io->ops->read(self->io, self->buffer, 512);
				if (nb_read < 512 || st_format_tar_reader_check_header(raw_header)) {
					st_format_file_free(header);
					return st_format_reader_header_bad_header;
				}
				continue;

			case 'K':
				next_read = 512 + st_format_tar_reader_convert_size(raw_header->size);
				if (next_read % 512)
					next_read -= next_read % 512;

				if (next_read > self->buffer_size)
					self->buffer = realloc(self->buffer, next_read);

				nb_read = self->io->ops->read(self->io, self->buffer, next_read);
				if (nb_read < next_read) {
					st_format_file_free(header);
					return st_format_reader_header_bad_header;
				}
				header->link = strdup(self->buffer);

				nb_read = self->io->ops->read(self->io, self->buffer, 512);
				if (nb_read < 512 || st_format_tar_reader_check_header(raw_header)) {
					st_format_file_free(header);
					return st_format_reader_header_bad_header;
				}
				continue;

			case 'M':
				header->position = st_format_tar_reader_convert_size(raw_header->position);
				break;

			case '1':
			case '2':
				if (header->link == NULL) {
					if (strlen(raw_header->linkname) > 100) {
						header->link = malloc(101);
						strncpy(header->link, raw_header->linkname, 100);
						header->link[100] = '\0';
					} else {
						header->link = strdup(raw_header->linkname);
					}
				}
				break;

			case '3':
			case '4':
				header->dev = st_format_tar_reader_convert_dev(raw_header);
				break;

			case 'V':
				header->is_label = 1;
				break;
		}

		if (header->filename == NULL) {
			if (strlen(raw_header->filename) > 100) {
				header->filename = malloc(101);
				strncpy(header->filename, raw_header->filename, 100);
				header->filename[100] = '\0';
			} else {
				header->filename = strdup(raw_header->filename);
			}
		}
		header->size = st_format_tar_reader_convert_size(raw_header->size);
		sscanf(raw_header->filemode, "%o", &header->mode);
		header->mtime = st_format_tar_reader_convert_time(raw_header);
		header->uid = st_format_tar_reader_convert_uid(raw_header);
		header->user = strdup(raw_header->uname);
		header->gid = st_format_tar_reader_convert_gid(raw_header);
		header->group = strdup(raw_header->gname);

		switch (raw_header->flag) {
			case '0':
			case '1':
			case 'M':
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

		break;
	}

	self->skip_size = self->file_size = header->size;

	if (header->size > 0 && header->size % 512)
		self->skip_size = 512 + header->size - header->size % 512;

	return st_format_reader_header_ok;
}

static int st_format_tar_reader_last_errno(struct st_format_reader * sfr) {
	struct st_format_tar_reader_private * self = sfr->data;
	return self->io->ops->last_errno(self->io);
}

static ssize_t st_format_tar_reader_position(struct st_format_reader * sfr) {
	struct st_format_tar_reader_private * self = sfr->data;
	return self->io->ops->position(self->io);
}

static ssize_t st_format_tar_reader_read(struct st_format_reader * sfr, void * data, ssize_t length) {
	if (sfr == NULL || data == NULL)
		return -1;

	struct st_format_tar_reader_private * self = sfr->data;
	if (length > self->file_size)
		length = self->file_size;

	if (length < 1)
		return 0;

	ssize_t nb_read = self->io->ops->read(self->io, data, length);
	if (nb_read > 0) {
		self->file_size -= nb_read;
		self->skip_size -= nb_read;
	}

	if (self->file_size == 0 && self->skip_size > 0)
		st_format_tar_reader_skip_file(sfr);

	return nb_read;
}

static ssize_t st_format_tar_reader_read_to_end_of_data(struct st_format_reader * sfr) {
	if (sfr == NULL)
		return -1;

	struct st_format_tar_reader_private * self = sfr->data;
	char buffer[4096];
	ssize_t nb_total_read = 0, nb_read;
	while (nb_read = self->io->ops->read(self->io, buffer, 4096), nb_read > 0)
		nb_total_read += nb_read;

	if (nb_read < 0)
		return -1;

	return nb_total_read;
}

static enum st_format_reader_header_status st_format_tar_reader_skip_file(struct st_format_reader * sfr) {
	struct st_format_tar_reader_private * self = sfr->data;

	if (self->skip_size == 0)
		return st_format_reader_header_ok;

	if (self->skip_size > 0) {
		off_t next_pos = self->io->ops->position(self->io) + self->skip_size;
		off_t new_pos = self->io->ops->forward(self->io, self->skip_size);
		if (new_pos == (off_t) -1)
			return st_format_reader_header_not_found;

		self->position += self->skip_size;
		self->file_size = self->skip_size = 0;
		return new_pos == next_pos ? st_format_reader_header_ok : st_format_reader_header_not_found;
	}

	return st_format_reader_header_ok;
}

