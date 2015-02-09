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

// errno
#include <errno.h>
// getgrgid_r
#include <grp.h>
// getpwuid_r
#include <pwd.h>
// snprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// memset, strcpy, strlen, strncpy
#include <string.h>
// bzero
#include <strings.h>
// S_*
#include <sys/stat.h>
// time
#include <time.h>

#include <libstoriqone/format.h>
#include <libstoriqone/io.h>
#include <libstoriqone/value.h>

#include "common.h"

struct sodr_format_tar_writer_private {
	struct so_stream_writer * io;

	off_t position;
	ssize_t size;

	int last_errno;

	// multi volume
	void * remain_header;
	ssize_t remain_size;

	bool has_cheksum;
};

static enum so_format_writer_status sodr_format_tar_writer_add_file(struct so_format_writer * fw, struct so_format_file * file);
static enum so_format_writer_status sodr_format_tar_writer_add_label(struct so_format_writer * fw, const char * label);
static int sodr_format_tar_writer_close(struct so_format_writer * fw);
static void sodr_format_tar_writer_compute_checksum(const void * header, char * checksum);
static void sodr_format_tar_writer_compute_link(struct so_format_tar * header, char * link, const char * filename, ssize_t filename_length, char flag, struct stat * sfile, struct so_format_file * file);
static void sodr_format_tar_writer_compute_size(char * csize, long long size);
static ssize_t sodr_format_tar_writer_end_of_file(struct so_format_writer * fw);
static void sodr_format_tar_writer_free(struct so_format_writer * fw);
static ssize_t sodr_format_tar_writer_get_available_size(struct so_format_writer * fw);
static ssize_t sodr_format_tar_writer_get_block_size(struct so_format_writer * fw);
static struct so_value * sodr_format_tar_writer_get_digests(struct so_format_writer * fw);
static void sodr_format_tar_writer_gid2name(char * name, ssize_t length, gid_t uid);
static int sodr_format_tar_writer_last_errno(struct so_format_writer * fw);
static ssize_t sodr_format_tar_writer_position(struct so_format_writer * fw);
static void sodr_format_tar_writer_uid2name(char * name, ssize_t length, uid_t uid);
static ssize_t sodr_format_tar_writer_write(struct so_format_writer * fw, const void * buffer, ssize_t length);
static enum so_format_writer_status sodr_format_tar_writer_write_header(struct sodr_format_tar_writer_private * f, void * data, ssize_t length);

static struct so_format_writer_ops sodr_format_tar_writer_ops = {
	.add_file           = sodr_format_tar_writer_add_file,
	.add_label          = sodr_format_tar_writer_add_label,
	.close              = sodr_format_tar_writer_close,
	.end_of_file        = sodr_format_tar_writer_end_of_file,
	.free               = sodr_format_tar_writer_free,
	.get_available_size = sodr_format_tar_writer_get_available_size,
	.get_block_size     = sodr_format_tar_writer_get_block_size,
	.get_digests        = sodr_format_tar_writer_get_digests,
	.last_errno         = sodr_format_tar_writer_last_errno,
	.position           = sodr_format_tar_writer_position,
	.write              = sodr_format_tar_writer_write,
};


struct so_format_writer * sodr_format_tar_new_writer(struct so_stream_writer * writer, struct so_value * checksums) {
	struct sodr_format_tar_writer_private * self = malloc(sizeof(struct sodr_format_tar_writer_private));
	bzero(self, sizeof(struct sodr_format_tar_writer_private));

	self->has_cheksum = checksums == NULL && so_value_list_get_length(checksums) > 0;
	if (self->has_cheksum)
		self->io = so_io_checksum_writer_new(writer, checksums, true);
	else
		self->io = writer;

	self->position = 0;
	self->size = 0;
	self->last_errno = 0;
	self->remain_header = NULL;
	self->remain_size = 0;

	struct so_format_writer * new_writer = malloc(sizeof(struct so_format_writer));
	new_writer->ops = &sodr_format_tar_writer_ops;
	new_writer->data = self;

	return new_writer;
}


static enum so_format_writer_status sodr_format_tar_writer_add_file(struct so_format_writer * fw, struct so_format_file * file) {
	ssize_t block_size = 512;
	struct so_format_tar * header = malloc(block_size);
	struct so_format_tar * current_header = header;

	int filename_length = strlen(file->filename);
	if (S_ISLNK(file->mode)) {
		int link_length = strlen(file->link);
		if (filename_length > 100 && link_length > 100) {
			block_size += 2048 + filename_length - filename_length % 512 + link_length - link_length % 512;
			current_header = header = realloc(header, block_size);

			bzero(current_header, block_size - 512);
			sodr_format_tar_writer_compute_link(current_header, (char *) (current_header + 1), file->link, link_length, 'K', NULL, file);
			sodr_format_tar_writer_compute_link(current_header + 2, (char *) (current_header + 3), file->filename, filename_length, 'L', NULL, file);

			current_header += 4;
		} else if (filename_length > 100) {
			block_size += 1024 + filename_length - filename_length % 512;
			current_header = header = realloc(header, block_size);

			bzero(current_header, block_size - 512);
			sodr_format_tar_writer_compute_link(current_header, (char *) (current_header + 1), file->filename, filename_length, 'L', NULL, file);

			current_header += 2;
		} else if (link_length > 100) {
			block_size += 1024 + link_length - link_length % 512;
			current_header = header = realloc(header, block_size);
			bzero(current_header, block_size - 512);
			sodr_format_tar_writer_compute_link(current_header, (char *) (current_header + 1), file->link, link_length, 'K', NULL, file);

			current_header += 2;
		}
	}

	struct sodr_format_tar_writer_private * format = fw->data;
	bzero(current_header, 512);
	strncpy(current_header->filename, file->filename, 100);
	snprintf(current_header->filemode, 8, "%07o", file->mode & 07777);
	snprintf(current_header->uid, 8, "%07o", file->uid);
	snprintf(current_header->gid, 8, "%07o", file->gid);
	snprintf(current_header->mtime, 12, "%0*o", 11, (unsigned int) file->mtime);

	if (S_ISREG(file->mode)) {
		sodr_format_tar_writer_compute_size(current_header->size, file->size);
		current_header->flag = '0';
		format->size = file->size;
	} else if (S_ISLNK(file->mode)) {
		current_header->flag = '2';
		format->size = 0;
	} else if (S_ISCHR(file->mode)) {
		current_header->flag = '3';
		snprintf(current_header->devmajor, 8, "%07o", (unsigned int) (file->rdev >> 8));
		snprintf(current_header->devminor, 8, "%07o", (unsigned int) (file->rdev & 0xFF));
		format->size = 0;
	} else if (S_ISBLK(file->mode)) {
		current_header->flag = '4';
		snprintf(current_header->devmajor, 8, "%07o", (unsigned int) (file->rdev >> 8));
		snprintf(current_header->devminor, 8, "%07o", (unsigned int) (file->rdev & 0xFF));
		format->size = 0;
	} else if (S_ISDIR(file->mode)) {
		current_header->flag = '5';
		format->size = 0;
	} else if (S_ISFIFO(file->mode)) {
		current_header->flag = '6';
		format->size = 0;
	}

	strcpy(current_header->magic, "ustar  ");
	sodr_format_tar_writer_uid2name(current_header->uname, 32, file->uid);
	sodr_format_tar_writer_gid2name(current_header->gname, 32, file->gid);

	sodr_format_tar_writer_compute_checksum(current_header, current_header->checksum);

	format->position = 0;

	return sodr_format_tar_writer_write_header(format, header, block_size);
}

static enum so_format_writer_status sodr_format_tar_writer_add_label(struct so_format_writer * fw, const char * label) {
	if (label == NULL)
		return so_format_writer_error;

	struct so_format_tar * header = malloc(512);
	bzero(header, 512);
	strncpy(header->filename, label, 100);
	snprintf(header->mtime, 12, "%0*o", 11, (unsigned int) time(NULL));
	header->flag = 'V';

	sodr_format_tar_writer_compute_checksum(header, header->checksum);

	struct sodr_format_tar_writer_private * format = fw->data;
	format->position = 0;
	format->size = 0;

	return sodr_format_tar_writer_write_header(format, header, 512);
}

static int sodr_format_tar_writer_close(struct so_format_writer * fw) {
	struct sodr_format_tar_writer_private * format = fw->data;
	return format->io->ops->close(format->io);
}

static void sodr_format_tar_writer_compute_checksum(const void * header, char * checksum) {
	const unsigned char * ptr = header;

	memset(checksum, ' ', 8);

	unsigned int i, sum = 0;
	for (i = 0; i < 512; i++)
		sum += ptr[i];

	snprintf(checksum, 7, "%06o", sum);
}

static void sodr_format_tar_writer_compute_link(struct so_format_tar * header, char * link, const char * filename, ssize_t filename_length, char flag, struct stat * sfile, struct so_format_file * file) {
	strcpy(header->filename, "././@LongLink");
	memset(header->filemode, '0', 7);
	memset(header->uid, '0', 7);
	memset(header->gid, '0', 7);
	sodr_format_tar_writer_compute_size(header->size, filename_length);
	memset(header->mtime, '0', 11);
	header->flag = flag;
	strcpy(header->magic, "ustar  ");
	if (sfile != NULL) {
		sodr_format_tar_writer_uid2name(header->uname, 32, sfile->st_uid);
		sodr_format_tar_writer_gid2name(header->gname, 32, sfile->st_gid);
	} else {
		sodr_format_tar_writer_uid2name(header->uname, 32, file->uid);
		sodr_format_tar_writer_gid2name(header->gname, 32, file->gid);
	}

	sodr_format_tar_writer_compute_checksum(header, header->checksum);

	strcpy(link, filename);
}

static void sodr_format_tar_writer_compute_size(char * csize, long long size) {
	if (size > 077777777777) {
		*csize = (char) 0x80;
		unsigned int i;
		for (i = 11; i > 0; i--) {
			csize[i] = size & 0xFF;
			size >>= 8;
		}
	} else {
		snprintf(csize, 12, "%0*llo", 11, size);
	}
}

static ssize_t sodr_format_tar_writer_end_of_file(struct so_format_writer * fw) {
	struct sodr_format_tar_writer_private * self = fw->data;

	unsigned short mod = self->position % 512;
	if (mod > 0) {
		size_t length = 512 - mod;
		char * buffer = malloc(length);
		bzero(buffer, length);

		ssize_t nb_write = self->io->ops->write(self->io, buffer, length);
		free(buffer);

		return nb_write;
	}

	return 0;
}

static void sodr_format_tar_writer_free(struct so_format_writer * fw) {
	struct sodr_format_tar_writer_private * self = fw->data;

	if (self->io != NULL) {
		self->io->ops->close(self->io);
		self->io->ops->free(self->io);
	}

	free(self);
	free(fw);
}

static ssize_t sodr_format_tar_writer_get_available_size(struct so_format_writer * fw) {
	struct sodr_format_tar_writer_private * format = fw->data;
	return format->io->ops->get_available_size(format->io);
}

static ssize_t sodr_format_tar_writer_get_block_size(struct so_format_writer * fw) {
	struct sodr_format_tar_writer_private * format = fw->data;
	return format->io->ops->get_block_size(format->io);
}

static struct so_value * sodr_format_tar_writer_get_digests(struct so_format_writer * fw) {
}

static void sodr_format_tar_writer_gid2name(char * name, ssize_t length, gid_t uid) {
	char * buffer = malloc(512);

	struct group gr;
	struct group * tmp_gr;

	if (!getgrgid_r(uid, &gr, buffer, 512, &tmp_gr)) {
		strncpy(name, gr.gr_name, length);
		name[length - 1] = '\0';
	} else {
		snprintf(name, length, "%d", uid);
	}

	free(buffer);
}

static int sodr_format_tar_writer_last_errno(struct so_format_writer * fw) {
	struct sodr_format_tar_writer_private * format = fw->data;
	if (format->last_errno != 0)
		return format->last_errno;
	return format->io->ops->last_errno(format->io);
}

static ssize_t sodr_format_tar_writer_position(struct so_format_writer * fw) {
	struct sodr_format_tar_writer_private * format = fw->data;
	return format->io->ops->position(format->io);
}

static void sodr_format_tar_writer_uid2name(char * name, ssize_t length, uid_t uid) {
	char * buffer = malloc(512);

	struct passwd pw;
	struct passwd * tmp_pw;

	if (!getpwuid_r(uid, &pw, buffer, 512, &tmp_pw)) {
		strncpy(name, pw.pw_name, length);
		name[length - 1] = '\0';
	} else {
		snprintf(name, length, "%d", uid);
	}

	free(buffer);
}

static ssize_t sodr_format_tar_writer_write(struct so_format_writer * fw, const void * buffer, ssize_t length) {
	struct sodr_format_tar_writer_private * format = fw->data;

	if (format->position >= format->size)
		return 0;

	if (format->position + length > format->size)
		length = format->size - format->position;

	ssize_t nb_write = format->io->ops->write(format->io, buffer, length);

	if (nb_write > 0)
		format->position += nb_write;

	return nb_write;
}

static enum so_format_writer_status sodr_format_tar_writer_write_header(struct sodr_format_tar_writer_private * f, void * data, ssize_t length) {
	ssize_t available = f->io->ops->get_available_size(f->io);

	ssize_t nb_write = 0;

	if (available > 0)
		nb_write = f->io->ops->write(f->io, data, length);

	int last_errno = 0;
	if (nb_write < 0) {
		last_errno = f->io->ops->last_errno(f->io);

		if (last_errno != ENOSPC)
			return so_format_writer_error;

		nb_write = 0;
	}

	if (nb_write == 0) {
		f->remain_size = length;
		f->remain_header = data;

		f->io->ops->close(f->io);

		return so_format_writer_end_of_volume;
	} else if (length > nb_write) {
		f->remain_size = length - nb_write;
		f->remain_header = malloc(f->remain_size);
		memcpy(f->remain_header, data, f->remain_size);
		free(data);

		f->io->ops->close(f->io);

		return so_format_writer_end_of_volume;
	}

	free(data);

	return so_format_writer_ok;
}

