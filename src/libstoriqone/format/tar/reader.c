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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// sscanf, snprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// memset, strdup, strlen, strncpy
#include <string.h>
// bzero
#include <strings.h>
// S_*
#include <sys/stat.h>
#include <sys/types.h>

#include <libstoriqone/format.h>
#include <libstoriqone/io.h>
#include <libstoriqone/value.h>

#include "common.h"

struct so_format_tar_reader_private {
	struct so_stream_reader * io;

	char * buffer;
	ssize_t buffer_size;
	ssize_t buffer_used;

	ssize_t file_size;
	ssize_t skip_size;

	bool has_checksum;
};

static int so_format_tar_reader_check_header(struct so_format_tar * header);
static int so_format_tar_reader_close(struct so_format_reader * fr);
static dev_t so_format_tar_reader_convert_dev(struct so_format_tar * header);
static gid_t so_format_tar_reader_convert_gid(struct so_format_tar * header);
static ssize_t so_format_tar_reader_convert_size(const char * size);
static time_t so_format_tar_reader_convert_time(struct so_format_tar * header);
static uid_t so_format_tar_reader_convert_uid(struct so_format_tar * header);
static bool so_format_tar_reader_end_of_file(struct so_format_reader * fr);
static enum so_format_reader_header_status so_format_tar_reader_forward(struct so_format_reader * fr, off_t offset);
static void so_format_tar_reader_free(struct so_format_reader * fr);
static ssize_t so_format_tar_reader_get_block_size(struct so_format_reader * fr);
static struct so_value * so_format_tar_reader_get_digests(struct so_format_reader * fr);
static enum so_format_reader_header_status so_format_tar_reader_get_header(struct so_format_reader * fr, struct so_format_file * file);
static char * so_format_tar_reader_get_root(struct so_format_reader * fr);
static int so_format_tar_reader_last_errno(struct so_format_reader * fr);
static ssize_t so_format_tar_reader_position(struct so_format_reader * fr);
static ssize_t so_format_tar_reader_read(struct so_format_reader * fr, void * buffer, ssize_t length);
static ssize_t so_format_tar_reader_read_to_end_of_data(struct so_format_reader * fr);
static int so_format_tar_reader_rewind(struct so_format_reader * fr);
static enum so_format_reader_header_status so_format_tar_reader_skip_file(struct so_format_reader * fr);

static struct so_format_reader_ops so_format_tar_reader_ops = {
	.close               = so_format_tar_reader_close,
	.end_of_file         = so_format_tar_reader_end_of_file,
	.forward             = so_format_tar_reader_forward,
	.free                = so_format_tar_reader_free,
	.get_block_size      = so_format_tar_reader_get_block_size,
	.get_digests         = so_format_tar_reader_get_digests,
	.get_header          = so_format_tar_reader_get_header,
	.get_root            = so_format_tar_reader_get_root,
	.last_errno          = so_format_tar_reader_last_errno,
	.position            = so_format_tar_reader_position,
	.read                = so_format_tar_reader_read,
	.read_to_end_of_data = so_format_tar_reader_read_to_end_of_data,
	.rewind              = so_format_tar_reader_rewind,
	.skip_file           = so_format_tar_reader_skip_file,
};


struct so_format_reader * so_format_tar_new_reader(struct so_stream_reader * reader, struct so_value * checksums) {
	bool has_checksums = checksums != NULL && so_value_list_get_length(checksums) > 0;
	if (has_checksums)
		reader = so_io_checksum_reader_new(reader, checksums, true);

	if (reader == NULL)
		return NULL;

	return so_format_tar_new_reader2(reader, has_checksums);
}

struct so_format_reader * so_format_tar_new_reader2(struct so_stream_reader * reader, bool has_checksums) {
	struct so_format_tar_reader_private * self = malloc(sizeof(struct so_format_tar_reader_private));
	bzero(self, sizeof(struct so_format_tar_reader_private));

	self->io = reader;
	self->has_checksum = has_checksums;
	self->buffer = malloc(512);
	self->buffer_size = 512;
	self->buffer_used = 0;
	self->file_size = 0;
	self->skip_size = 0;

	struct so_format_reader * new_reader = malloc(sizeof(struct so_format_reader));
	new_reader->ops = &so_format_tar_reader_ops;
	new_reader->data = self;

	return new_reader;
}


static int so_format_tar_reader_check_header(struct so_format_tar * header) {
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

static int so_format_tar_reader_close(struct so_format_reader * fr) {
	struct so_format_tar_reader_private * self = fr->data;
	return self->io->ops->close(self->io);
}

static dev_t so_format_tar_reader_convert_dev(struct so_format_tar * header) {
	unsigned int major = 0, minor = 0;

	sscanf(header->devmajor, "%o", &major);
	sscanf(header->devminor, "%o", &minor);

	return (major << 8 ) | minor;
}

static gid_t so_format_tar_reader_convert_gid(struct so_format_tar * header) {
	unsigned int result;
	sscanf(header->gid, "%o", &result);
	return result;
}

static ssize_t so_format_tar_reader_convert_size(const char * size) {
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

static time_t so_format_tar_reader_convert_time(struct so_format_tar * header) {
	unsigned int result;
	sscanf(header->mtime, "%o", &result);
	return result;
}

static uid_t so_format_tar_reader_convert_uid(struct so_format_tar * header) {
	unsigned int result;
	sscanf(header->uid, "%o", &result);
	return result;
}

static bool so_format_tar_reader_end_of_file(struct so_format_reader * fr) {
	struct so_format_tar_reader_private * self = fr->data;
	return self->io->ops->end_of_file(self->io);
}

static enum so_format_reader_header_status so_format_tar_reader_forward(struct so_format_reader * fr, off_t block_position) {
	struct so_format_tar_reader_private * self = fr->data;

	ssize_t current_position = self->io->ops->position(self->io);
	ssize_t block_size = self->io->ops->get_block_size(self->io);
	ssize_t current_block = current_position / block_size;
	if (current_block > block_position)
		return so_format_reader_header_not_found;

	if (block_position - current_block < 2)
		return so_format_reader_header_ok;

	off_t next_position = block_position * block_size;
	off_t new_position = self->io->ops->forward(self->io, next_position - current_position);

	if (next_position != new_position)
		return so_format_reader_header_not_found;

	return so_format_reader_header_ok;
}

static void so_format_tar_reader_free(struct so_format_reader * fr) {
	struct so_format_tar_reader_private * self = fr->data;

	if (self->io != NULL)
		self->io->ops->free(self->io);
	self->io = NULL;

	if (self->buffer != NULL)
		free(self->buffer);
	self->buffer = NULL;

	free(self);
	free(fr);
}

static ssize_t so_format_tar_reader_get_block_size(struct so_format_reader * fr) {
	struct so_format_tar_reader_private * self = fr->data;
	return self->io->ops->get_block_size(self->io);
}

static struct so_value * so_format_tar_reader_get_digests(struct so_format_reader * fr) {
	struct so_format_tar_reader_private * self = fr->data;
	if (self->has_checksum)
		return so_io_checksum_reader_get_checksums(self->io);
	else
		return so_value_new_linked_list();
}

static char * so_format_tar_reader_get_root(struct so_format_reader * fr __attribute__((unused))) {
	return strdup("/");
}

static enum so_format_reader_header_status so_format_tar_reader_get_header(struct so_format_reader * fr, struct so_format_file * file) {
	struct so_format_tar_reader_private * self = fr->data;

	if (self->io->ops->end_of_file(self->io))
		return so_format_reader_header_not_found;

	ssize_t nb_read = self->io->ops->read(self->io, self->buffer, 512);
	if (nb_read == 0)
		return so_format_reader_header_not_found;
	if (nb_read < 0)
		return so_format_reader_header_bad_header;

	struct so_format_tar * raw_header = (struct so_format_tar *) self->buffer;

	if (so_format_tar_reader_check_header(raw_header))
		return so_format_reader_header_bad_header;

	so_format_file_init(file);

	for (;;) {
		ssize_t next_read;
		switch (raw_header->flag) {
			case 'L':
				next_read = 512 + so_format_tar_reader_convert_size(raw_header->size);
				if (next_read % 512)
					next_read -= next_read % 512;

				if (next_read > self->buffer_size)
					self->buffer = realloc(self->buffer, next_read);

				nb_read = self->io->ops->read(self->io, self->buffer, next_read);
				if (nb_read < next_read) {
					so_format_file_free(file);
					return so_format_reader_header_bad_header;
				}
				file->filename = strdup(self->buffer);

				nb_read = self->io->ops->read(self->io, self->buffer, 512);
				if (nb_read < 512 || so_format_tar_reader_check_header(raw_header)) {
					so_format_file_free(file);
					return so_format_reader_header_bad_header;
				}
				continue;

			case 'K':
				next_read = 512 + so_format_tar_reader_convert_size(raw_header->size);
				if (next_read % 512)
					next_read -= next_read % 512;

				if (next_read > self->buffer_size)
					self->buffer = realloc(self->buffer, next_read);

				nb_read = self->io->ops->read(self->io, self->buffer, next_read);
				if (nb_read < next_read) {
					so_format_file_free(file);
					return so_format_reader_header_bad_header;
				}
				file->link = strdup(self->buffer);

				nb_read = self->io->ops->read(self->io, self->buffer, 512);
				if (nb_read < 512 || so_format_tar_reader_check_header(raw_header)) {
					so_format_file_free(file);
					return so_format_reader_header_bad_header;
				}
				continue;

			case 'M':
				file->position = so_format_tar_reader_convert_size(raw_header->position);
				break;

			case '1':
			case '2':
				if (file->link == NULL) {
					if (strlen(raw_header->linkname) > 100) {
						file->link = malloc(101);
						strncpy(file->link, raw_header->linkname, 100);
						file->link[100] = '\0';
					} else {
						file->link = strdup(raw_header->linkname);
					}
				}
				break;

			case '3':
			case '4':
				file->dev = so_format_tar_reader_convert_dev(raw_header);
				break;

			case 'V':
				file->is_label = true;
				break;
		}

		if (file->filename == NULL) {
			if (strlen(raw_header->filename) > 100) {
				file->filename = malloc(101);
				strncpy(file->filename, raw_header->filename, 100);
				file->filename[100] = '\0';
			} else {
				file->filename = strdup(raw_header->filename);
			}
		}
		file->size = so_format_tar_reader_convert_size(raw_header->size);
		sscanf(raw_header->filemode, "%o", &file->mode);
		file->mtime = so_format_tar_reader_convert_time(raw_header);
		file->uid = so_format_tar_reader_convert_uid(raw_header);
		file->user = strdup(raw_header->uname);
		file->gid = so_format_tar_reader_convert_gid(raw_header);
		file->group = strdup(raw_header->gname);

		switch (raw_header->flag) {
			case '0':
			case '1':
			case 'M':
				file->mode |= S_IFREG;
				break;

			case '2':
				file->mode |= S_IFLNK;
				break;

			case '3':
				file->mode |= S_IFCHR;
				break;

			case '4':
				file->mode |= S_IFBLK;
				break;

			case '5':
				file->mode |= S_IFDIR;
				break;

			case '6':
				file->mode |= S_IFIFO;
				break;
		}

		break;
	}

	self->skip_size = self->file_size = file->size;

	if (file->size > 0 && file->size % 512)
		self->skip_size = 512 + file->size - file->size % 512;

	return so_format_reader_header_ok;
}

static int so_format_tar_reader_last_errno(struct so_format_reader * fr) {
	struct so_format_tar_reader_private * self = fr->data;
	return self->io->ops->last_errno(self->io);
}

static ssize_t so_format_tar_reader_position(struct so_format_reader * fr) {
	struct so_format_tar_reader_private * self = fr->data;
	return self->io->ops->position(self->io);
}

static ssize_t so_format_tar_reader_read(struct so_format_reader * fr, void * buffer, ssize_t length) {
	if (fr == NULL || buffer == NULL)
		return -1;

	struct so_format_tar_reader_private * self = fr->data;
	if (length > self->file_size)
		length = self->file_size;

	if (length < 1)
		return 0;

	ssize_t nb_read = self->io->ops->read(self->io, buffer, length);
	if (nb_read > 0) {
		self->file_size -= nb_read;
		self->skip_size -= nb_read;
	}

	if (self->file_size == 0 && self->skip_size > 0)
		so_format_tar_reader_skip_file(fr);

	return nb_read;
}

static ssize_t so_format_tar_reader_read_to_end_of_data(struct so_format_reader * fr) {
	if (fr == NULL)
		return -1;

	struct so_format_tar_reader_private * self = fr->data;
	char buffer[4096];
	ssize_t nb_total_read = 0, nb_read;
	while (nb_read = self->io->ops->read(self->io, buffer, 4096), nb_read > 0)
		nb_total_read += nb_read;

	if (nb_read < 0)
		return -1;

	return nb_total_read;
}

static int so_format_tar_reader_rewind(struct so_format_reader * fr) {
	if (fr == NULL)
		return -1;

	struct so_format_tar_reader_private * self = fr->data;
	int failed = self->io->ops->rewind(self->io);

	if (failed == 0) {
		self->buffer_used = 0;
		self->file_size = 0;
		self->skip_size = 0;
	}

	return failed;
}

static enum so_format_reader_header_status so_format_tar_reader_skip_file(struct so_format_reader * fr) {
	struct so_format_tar_reader_private * self = fr->data;

	if (self->skip_size == 0)
		return so_format_reader_header_ok;

	if (self->skip_size > 0) {
		off_t next_pos = self->io->ops->position(self->io) + self->skip_size;
		off_t new_pos = self->io->ops->forward(self->io, self->skip_size);
		if (new_pos == (off_t) -1)
			return so_format_reader_header_not_found;

		self->file_size = self->skip_size = 0;
		return new_pos == next_pos ? so_format_reader_header_ok : so_format_reader_header_not_found;
	}

	return so_format_reader_header_ok;
}

