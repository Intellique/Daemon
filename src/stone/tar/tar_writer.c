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
*  Last modified: Mon, 23 Jan 2012 13:07:53 +0100                         *
\*************************************************************************/

#include <errno.h>
// getgrgid_r
#include <grp.h>
// getpwuid_r
#include <pwd.h>
// free, malloc, realloc
#include <stdlib.h>
// snprintf
#include <stdio.h>
// memset, strcpy, strlen, strncpy
#include <string.h>
// bzero
#include <strings.h>
// lstat, stat
#include <sys/stat.h>
// getgrgid_r, getpwuid_r, lstat, stat
#include <sys/types.h>
// time
#include <time.h>
// access, readlink, stat
#include <unistd.h>

#include <stone/io.h>

#include "common.h"

struct st_tar_private_out {
	struct st_stream_writer * io;
	off_t position;
	ssize_t size;
	int last_errno;
};

static int st_tar_out_add_file(struct st_tar_out * f, const char * filename);
static int st_tar_out_add_label(struct st_tar_out * f, const char * label);
static int st_tar_out_add_link(struct st_tar_out * f, const char * src, const char * target, struct st_tar_header * header);
static int st_tar_out_close(struct st_tar_out * f);
static void st_tar_out_compute_checksum(const void * header, char * checksum);
static void st_tar_out_compute_link(struct st_tar * header, char * link, const char * filename, ssize_t filename_length, char flag, struct stat * sfile);
static void st_tar_out_compute_size(char * csize, ssize_t size);
static void st_tar_out_copy(struct st_tar_header * h_to, struct st_tar * h_from, struct stat * sfile);
static int st_tar_out_end_of_file(struct st_tar_out * f);
static void st_tar_out_free(struct st_tar_out * f);
static ssize_t st_tar_out_get_available_size(struct st_tar_out * f);
static ssize_t st_tar_out_get_block_size(struct st_tar_out * f);
static ssize_t st_tar_out_get_file_position(struct st_tar_out * f);
static void st_tar_out_gid2name(char * name, ssize_t length, gid_t uid);
static int st_tar_out_last_errno(struct st_tar_out * f);
static ssize_t st_tar_out_position(struct st_tar_out * f);
static int st_tar_out_restart_file(struct st_tar_out * f, const char * filename, ssize_t position);
static const char * st_tar_out_skip_leading_slash(const char * str);
static void st_tar_out_uid2name(char * name, ssize_t length, uid_t uid);
static ssize_t st_tar_out_write(struct st_tar_out * f, const void * data, ssize_t length);

static struct st_tar_out_ops st_tar_out_ops = {
	.add_file           = st_tar_out_add_file,
	.add_label          = st_tar_out_add_label,
	.add_link           = st_tar_out_add_link,
	.close              = st_tar_out_close,
	.end_of_file        = st_tar_out_end_of_file,
	.get_available_size = st_tar_out_get_available_size,
	.get_block_size     = st_tar_out_get_block_size,
	.get_file_position  = st_tar_out_get_file_position,
	.free               = st_tar_out_free,
	.last_errno         = st_tar_out_last_errno,
	.position           = st_tar_out_position,
	.restart_file       = st_tar_out_restart_file,
	.write              = st_tar_out_write,
};


struct st_tar_out * st_tar_new_out(struct st_stream_writer * io) {
	if (!io)
		return 0;

	struct st_tar_private_out * data = malloc(sizeof(struct st_tar_private_out));
	data->io = io;
	data->position = 0;
	data->size = 0;
	data->last_errno = 0;

	struct st_tar_out * self = malloc(sizeof(struct st_tar_out));
	self->ops  = &st_tar_out_ops;
	self->data = data;

	return self;
}

int st_tar_out_add_file(struct st_tar_out * f, const char * filename) {
	if (access(filename, F_OK)) {
		//mtar_verbose_printf("Can access to file: %s\n", filename);
		return 1;
	}
	if (access(filename, R_OK)) {
		//mtar_verbose_printf("Can read file: %s\n", filename);
		return 1;
	}

	struct stat sfile;
	if (lstat(filename, &sfile)) {
		//mtar_verbose_printf("An unexpected error occured while getting information about: %s\n", filename);
		return 1;
	}

	ssize_t block_size = 512;
	struct st_tar * header = malloc(block_size);
	struct st_tar * current_header = header;

	int filename_length = strlen(filename);
	if (S_ISLNK(sfile.st_mode)) {
		char link[257];
		ssize_t link_length = readlink(filename, link, 256);
		link[link_length] = '\0';

		if (filename_length > 100 && link_length > 100) {
			block_size += 2048 + filename_length - filename_length % 512 + link_length - link_length % 512;
			current_header = header = realloc(header, block_size);

			bzero(current_header, block_size - 512);
			st_tar_out_compute_link(current_header, (char *) (current_header + 1), filename, filename_length, 'K', &sfile);
			st_tar_out_compute_link(current_header + 2, (char *) (current_header + 3), link, link_length, 'L', &sfile);

			current_header += 4;
		} else if (filename_length > 100) {
			block_size += 1024 + filename_length - filename_length % 512;
			current_header = header = realloc(header, block_size);

			bzero(current_header, block_size - 512);
			st_tar_out_compute_link(current_header, (char *) (current_header + 1), filename, filename_length, 'L', &sfile);

			current_header += 2;
		} else if (link_length > 100) {
			block_size += 1024 + link_length - link_length % 512;
			current_header = header = realloc(header, block_size);

			bzero(current_header, block_size - 512);
			st_tar_out_compute_link(current_header, (char *) (current_header + 1), link, link_length, 'L', &sfile);

			current_header += 2;
		}
	} else if (filename_length > 100) {
		block_size += 1024 + filename_length - filename_length % 512;
		current_header = header = realloc(header, block_size);

		bzero(current_header, 1024);
		st_tar_out_compute_link(current_header, (char *) (current_header + 1), filename, filename_length, 'L', &sfile);

		current_header += 2;
	}

	struct st_tar_private_out * format = f->data;
	bzero(current_header, 512);
	strncpy(current_header->filename, st_tar_out_skip_leading_slash(filename), 100);
	snprintf(header->filemode, 8, "%07o", sfile.st_mode & 0777);
	snprintf(header->uid, 8, "%07o", sfile.st_uid);
	snprintf(header->gid, 8, "%07o", sfile.st_gid);
	snprintf(current_header->mtime, 12, "%0*o", 11, (unsigned int) sfile.st_mtime);

	if (S_ISREG(sfile.st_mode)) {
		st_tar_out_compute_size(current_header->size, sfile.st_size);
		current_header->flag = '0';
	} else if (S_ISLNK(sfile.st_mode)) {
		current_header->flag = '2';
		readlink(filename, current_header->linkname, 100);
	} else if (S_ISCHR(sfile.st_mode)) {
		current_header->flag = '3';
		snprintf(current_header->devmajor, 8, "%07o", (unsigned int) (sfile.st_rdev >> 8));
		snprintf(current_header->devminor, 8, "%07o", (unsigned int) (sfile.st_rdev & 0xFF));
	} else if (S_ISBLK(sfile.st_mode)) {
		current_header->flag = '4';
		snprintf(current_header->devmajor, 8, "%07o", (unsigned int) (sfile.st_rdev >> 8));
		snprintf(current_header->devminor, 8, "%07o", (unsigned int) (sfile.st_rdev & 0xFF));
	} else if (S_ISDIR(sfile.st_mode)) {
		current_header->flag = '5';
	} else if (S_ISFIFO(sfile.st_mode)) {
		current_header->flag = '6';
	}

	strcpy(current_header->magic, "ustar  ");
	st_tar_out_uid2name(header->uname, 32, sfile.st_uid);
	st_tar_out_gid2name(header->gname, 32, sfile.st_gid);

	st_tar_out_compute_checksum(current_header, current_header->checksum);

	format->position = 0;
	format->size = sfile.st_size;

	if (block_size > format->io->ops->get_available_size(format->io)) {
		format->last_errno = ENOSPC;
		return -1;
	}

	ssize_t nbWrite = format->io->ops->write(format->io, header, block_size);

	free(header);

	return block_size != nbWrite ? -1 : 0;
}

int st_tar_out_add_label(struct st_tar_out * f, const char * label) {
	if (!label) {
		//mtar_verbose_printf("Label should be defined\n");
		return 1;
	}

	struct st_tar * header = malloc(512);
	bzero(header, 512);
	strncpy(header->filename, label, 100);
	snprintf(header->mtime, 12, "%0*o", 11, (unsigned int) time(0));
	header->flag = 'V';

	st_tar_out_compute_checksum(header, header->checksum);

	struct st_tar_private_out * format = f->data;
	format->position = 0;
	format->size = 0;

	ssize_t nbWrite = format->io->ops->write(format->io, header, 512);

	free(header);

	return 512 != nbWrite ? -1 : 0;
}

int st_tar_out_add_link(struct st_tar_out * f, const char * src, const char * target, struct st_tar_header * h_out) {
	if (access(src, F_OK)) {
		//mtar_verbose_printf("Can access to file: %s\n", src);
		return 1;
	}

	struct stat sfile;
	if (stat(src, &sfile)) {
		//mtar_verbose_printf("An unexpected error occured while getting information about: %s\n", src);
		return 1;
	}

	ssize_t block_size = 512;
	struct st_tar * header = malloc(block_size);
	struct st_tar * current_header = header;

	struct st_tar_private_out * format = f->data;
	bzero(current_header, 512);
	strncpy(current_header->filename, src, 100);
	snprintf(current_header->filemode, 8, "%07o", sfile.st_mode & 0777);
	snprintf(header->uid, 8, "%07o", sfile.st_uid);
	snprintf(header->gid, 8, "%07o", sfile.st_gid);
	snprintf(current_header->mtime, 12, "%0*o", 11, (unsigned int) sfile.st_mtime);
	current_header->flag = '1';
	strncpy(current_header->linkname, target, 100);
	strcpy(current_header->magic, "ustar  ");
	st_tar_out_uid2name(header->uname, 32, sfile.st_uid);
	st_tar_out_gid2name(header->gname, 32, sfile.st_gid);

	st_tar_out_compute_checksum(current_header, current_header->checksum);

	st_tar_out_copy(h_out, header, &sfile);
	h_out->size = 0;

	format->position = 0;
	format->size = 0;

	ssize_t nbWrite = format->io->ops->write(format->io, header, block_size);

	free(header);

	return block_size != nbWrite ? -1 : 0;
}

int st_tar_out_close(struct st_tar_out * f) {
	struct st_tar_private_out * format = f->data;
	return format->io->ops->close(format->io);
}

void st_tar_out_copy(struct st_tar_header * h_to, struct st_tar * h_from, struct stat * sfile) {
	st_tar_init_header(h_to);

	h_to->dev = sfile->st_rdev;
	strcpy(h_to->path, h_from->filename);
	h_to->filename = h_to->path;
	strcpy(h_to->link, h_from->linkname);
	h_to->size = S_ISREG(sfile->st_mode) ? sfile->st_size : 0;
	h_to->mode = sfile->st_mode;
	h_to->mtime = sfile->st_mtime;
	h_to->uid = sfile->st_uid;
	strcpy(h_to->uname, h_from->uname);
	h_to->gid = sfile->st_gid;
	strcpy(h_to->gname, h_from->gname);
	h_to->is_label = (h_from->flag == 'V');
}

void st_tar_out_compute_checksum(const void * header, char * checksum) {
	const unsigned char * ptr = header;

	memset(checksum, ' ', 8);

	unsigned int i, sum = 0;
	for (i = 0; i < 512; i++)
		sum += ptr[i];

	snprintf(checksum, 7, "%06o", sum);
}

void st_tar_out_compute_link(struct st_tar * header, char * link, const char * filename, ssize_t filename_length, char flag, struct stat * sfile) {
	strcpy(header->filename, "././@LongLink");
	memset(header->filemode, '0', 7);
	memset(header->uid, '0', 7);
	memset(header->gid, '0', 7);
	st_tar_out_compute_size(header->size, filename_length);
	memset(header->mtime, '0', 11);
	header->flag = flag;
	strcpy(header->magic, "ustar  ");
	st_tar_out_uid2name(header->uname, 32, sfile->st_uid);
	st_tar_out_gid2name(header->gname, 32, sfile->st_gid);

	st_tar_out_compute_checksum(header, header->checksum);

	strcpy(link, filename);
}

void st_tar_out_compute_size(char * csize, ssize_t size) {
	if (size > 077777777777) {
		*csize = (char) 0x80;
		unsigned int i;
		for (i = 11; i > 0; i--) {
			csize[i] = size & 0xFF;
			size >>= 8;
		}
	} else {
		snprintf(csize, 12, "%0*zo", 11, size);
	}
}

int st_tar_out_end_of_file(struct st_tar_out * f) {
	struct st_tar_private_out * format = f->data;

	unsigned short mod = format->position % 512;
	if (mod > 0) {
		char * buffer = malloc(512);
		bzero(buffer, 512 - mod);

		ssize_t nbWrite = format->io->ops->write(format->io, buffer, 512 - mod);
		free(buffer);
		if (nbWrite < 0)
			return -1;
	}

	return 0;
}

ssize_t st_tar_out_get_available_size(struct st_tar_out * f) {
	struct st_tar_private_out * format = f->data;
	return format->io->ops->get_available_size(format->io);
}

ssize_t st_tar_out_get_block_size(struct st_tar_out * f) {
	struct st_tar_private_out * format = f->data;
	return format->io->ops->get_block_size(format->io);
}

ssize_t st_tar_out_get_file_position(struct st_tar_out * f) {
	struct st_tar_private_out * format = f->data;
	return format->position;
}

void st_tar_out_free(struct st_tar_out * f) {
	struct st_tar_private_out * self = f->data;

	if (self->io) {
		self->io->ops->close(self->io);
		self->io->ops->free(self->io);
	}

	free(f->data);
	free(f);
}

void st_tar_out_gid2name(char * name, ssize_t length, gid_t uid) {
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

int st_tar_out_last_errno(struct st_tar_out * f) {
	struct st_tar_private_out * format = f->data;
	if (format->last_errno)
		return format->last_errno;
	return format->io->ops->last_errno(format->io);
}

ssize_t st_tar_out_position(struct st_tar_out * f) {
	struct st_tar_private_out * format = f->data;
	return format->io->ops->position(format->io);
}

int st_tar_out_restart_file(struct st_tar_out * f, const char * filename, ssize_t position) {
	if (access(filename, F_OK)) {
		//mtar_verbose_printf("Can access to file: %s\n", filename);
		return 1;
	}
	if (access(filename, R_OK)) {
		//mtar_verbose_printf("Can read file: %s\n", filename);
		return 1;
	}

	struct stat sfile;
	if (lstat(filename, &sfile)) {
		//mtar_verbose_printf("An unexpected error occured while getting information about: %s\n", filename);
		return 1;
	}

	ssize_t block_size = 512;
	struct st_tar * header = malloc(block_size);
	struct st_tar * current_header = header;

	int filename_length = strlen(filename);
	if (S_ISLNK(sfile.st_mode)) {
		char link[257];
		ssize_t link_length = readlink(filename, link, 256);
		link[link_length] = '\0';

		if (filename_length > 100 && link_length > 100) {
			block_size += 2048 + filename_length - filename_length % 512 + link_length - link_length % 512;
			current_header = header = realloc(header, block_size);

			bzero(current_header, block_size - 512);
			st_tar_out_compute_link(current_header, (char *) (current_header + 1), filename, filename_length, 'K', &sfile);
			st_tar_out_compute_link(current_header + 2, (char *) (current_header + 3), link, link_length, 'L', &sfile);

			current_header += 4;
		} else if (filename_length > 100) {
			block_size += 1024 + filename_length - filename_length % 512;
			current_header = header = realloc(header, block_size);

			bzero(current_header, block_size - 512);
			st_tar_out_compute_link(current_header, (char *) (current_header + 1), filename, filename_length, 'L', &sfile);

			current_header += 2;
		} else if (link_length > 100) {
			block_size += 1024 + link_length - link_length % 512;
			current_header = header = realloc(header, block_size);

			bzero(current_header, block_size - 512);
			st_tar_out_compute_link(current_header, (char *) (current_header + 1), link, link_length, 'L', &sfile);

			current_header += 2;
		}
	} else if (filename_length > 100) {
		block_size += 1024 + filename_length - filename_length % 512;
		current_header = header = realloc(header, block_size);

		bzero(current_header, 1024);
		st_tar_out_compute_link(current_header, (char *) (current_header + 1), filename, filename_length, 'L', &sfile);

		current_header += 2;
	}

	struct st_tar_private_out * format = f->data;
	bzero(current_header, 512);
	strncpy(current_header->filename, st_tar_out_skip_leading_slash(filename), 100);
	st_tar_out_compute_size(current_header->size, sfile.st_size - position);
	current_header->flag = 'M';
	st_tar_out_uid2name(header->uname, 32, sfile.st_uid);
	st_tar_out_gid2name(header->gname, 32, sfile.st_gid);
	st_tar_out_compute_size(current_header->position, position);

	st_tar_out_compute_checksum(current_header, current_header->checksum);

	format->position = 0;
	format->size = position;

	if (block_size > format->io->ops->get_available_size(format->io)) {
		format->last_errno = ENOSPC;
		return -1;
	}

	ssize_t nbWrite = format->io->ops->write(format->io, header, block_size);

	free(header);

	return block_size != nbWrite ? -1 : 0;
}

const char * st_tar_out_skip_leading_slash(const char * str) {
	if (!str)
		return 0;

	const char * ptr;
	size_t i = 0, length = strlen(str);
	for (ptr = str; *ptr == '/' && i < length; i++, ptr++);

	return ptr;
}

void st_tar_out_uid2name(char * name, ssize_t length, uid_t uid) {
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

ssize_t st_tar_out_write(struct st_tar_out * f, const void * data, ssize_t length) {
	struct st_tar_private_out * format = f->data;

	if (format->position >= format->size)
		return 0;

	if (format->position + length > format->size)
		length = format->size - format->position;

	ssize_t nbWrite = format->io->ops->write(format->io, data, length);

	if (nbWrite > 0)
		format->position += nbWrite;

	return nbWrite;
}

