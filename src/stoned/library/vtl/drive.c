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
*  Copyright (C) 2013-2015, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Fri, 07 Feb 2014 10:03:33 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// errno
#include <errno.h>
// open
#include <fcntl.h>
// glob, globfree
#include <glob.h>
// asprintf
#include <stdio.h>
// malloc
#include <stdlib.h>
// memcpy, strdup
#include <string.h>
// fstat, open
#include <sys/stat.h>
// fstat, lseek, open
#include <sys/types.h>
// time
#include <time.h>
// access, close, fstat, lseek, unlink
#include <unistd.h>

#include <libstone/format.h>
#include <libstone/io.h>
#include <libstone/library/changer.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>
#include <libstone/util/command.h>
#include <libstone/util/file.h>

#include "common.h"

struct st_vtl_drive_reader {
	int fd;

	char * buffer;
	ssize_t block_size;
	char * buffer_pos;

	ssize_t position;
	bool end_of_file;

	int last_errno;

	struct st_drive * drive;
	struct st_media * media;
};

struct st_vtl_drive_writer {
	int fd;
	char * filename;
	int file_position;

	char * buffer;
	ssize_t buffer_used;
	ssize_t block_size;
	ssize_t position;

	bool eof_reached;

	int last_errno;

	struct st_drive * drive;
	struct st_media * media;
};

static bool st_vtl_drive_check_media(struct st_vtl_drive * dr);
static int st_vtl_drive_eject(struct st_drive * drive);
static int st_vtl_drive_format(struct st_drive * drive, int file_position, bool quick_mode);
static void st_vtl_drive_free(struct st_drive * drive);
static int st_vtl_drive_get_position(struct st_drive * drive);
static struct st_stream_reader * st_vtl_drive_get_raw_reader(struct st_drive * drive, int file_position);
static struct st_stream_writer * st_vtl_drive_get_raw_writer(struct st_drive * drive, bool append);
static struct st_format_reader * st_vtl_drive_get_reader(struct st_drive * drive, int file_position, struct st_stream_reader * (*filter)(struct st_stream_reader * writer, void * param), void * param);
static struct st_format_writer * st_vtl_drive_get_writer(struct st_drive * drive, bool append, struct st_stream_writer * (*filter)(struct st_stream_writer * writer, void * param), void * param);
static void st_vtl_drive_operation_start(struct st_vtl_drive * dr);
static void st_vtl_drive_operation_stop(struct st_drive * dr);
static int st_vtl_drive_rewind(struct st_drive * drive);
static int st_vtl_drive_update_media_info(struct st_drive * drive);

static int st_vtl_drive_reader_close(struct st_stream_reader * sr);
static bool st_vtl_drive_reader_end_of_file(struct st_stream_reader * sr);
static off_t st_vtl_drive_reader_forward(struct st_stream_reader * sr, off_t offset);
static void st_vtl_drive_reader_free(struct st_stream_reader * sr);
static ssize_t st_vtl_drive_reader_get_block_size(struct st_stream_reader * sr);
static int st_vtl_drive_reader_last_errno(struct st_stream_reader * sr);
static struct st_stream_reader * st_vtl_drive_reader_new(struct st_drive * drive, char * filename, int file_positsrn);
static ssize_t st_vtl_drive_reader_position(struct st_stream_reader * sr);
static ssize_t st_vtl_drive_reader_read(struct st_stream_reader * sr, void * buffer, ssize_t length);

static ssize_t st_vtl_drive_writer_before_close(struct st_stream_writer * sw, void * buffer, ssize_t length);
static int st_vtl_drive_writer_close(struct st_stream_writer * sw);
static void st_vtl_drive_writer_free(struct st_stream_writer * sw);
static ssize_t st_vtl_drive_writer_get_available_size(struct st_stream_writer * sw);
static ssize_t st_vtl_drive_writer_get_block_size(struct st_stream_writer * sw);
static int st_vtl_drive_writer_last_errno(struct st_stream_writer * sw);
static struct st_stream_writer * st_vtl_drive_writer_new(struct st_drive * drive, char * filename, int file_position);
static ssize_t st_vtl_drive_writer_position(struct st_stream_writer * sw);
static struct st_stream_reader * st_vtl_drive_writer_reopen(struct st_stream_writer * sw);
static ssize_t st_vtl_drive_writer_write(struct st_stream_writer * sw, const void * buffer, ssize_t length);

static struct st_drive_ops st_vtl_drive_ops = {
	.eject             = st_vtl_drive_eject,
	.format            = st_vtl_drive_format,
	.free              = st_vtl_drive_free,
	.get_position      = st_vtl_drive_get_position,
	.get_raw_reader    = st_vtl_drive_get_raw_reader,
	.get_raw_writer    = st_vtl_drive_get_raw_writer,
	.get_reader        = st_vtl_drive_get_reader,
	.get_writer        = st_vtl_drive_get_writer,
	.rewind            = st_vtl_drive_rewind,
	.update_media_info = st_vtl_drive_update_media_info,
};

static struct st_stream_reader_ops st_vtl_drive_reader_ops = {
	.close          = st_vtl_drive_reader_close,
	.end_of_file    = st_vtl_drive_reader_end_of_file,
	.forward        = st_vtl_drive_reader_forward,
	.free           = st_vtl_drive_reader_free,
	.get_block_size = st_vtl_drive_reader_get_block_size,
	.last_errno     = st_vtl_drive_reader_last_errno,
	.position       = st_vtl_drive_reader_position,
	.read           = st_vtl_drive_reader_read,
};

static struct st_stream_writer_ops st_vtl_drive_writer_ops = {
	.before_close       = st_vtl_drive_writer_before_close,
	.close              = st_vtl_drive_writer_close,
	.free               = st_vtl_drive_writer_free,
	.get_available_size = st_vtl_drive_writer_get_available_size,
	.get_block_size     = st_vtl_drive_writer_get_block_size,
	.last_errno         = st_vtl_drive_writer_last_errno,
	.position           = st_vtl_drive_writer_position,
	.reopen             = st_vtl_drive_writer_reopen,
	.write              = st_vtl_drive_writer_write,
};


static bool st_vtl_drive_check_media(struct st_vtl_drive * dr) {
	return !access(dr->media_path, F_OK);
}

static int st_vtl_drive_eject(struct st_drive * drive __attribute__((unused))) {
	return 0;
}

static int st_vtl_drive_format(struct st_drive * drive, int file_position, bool quick_mode) {
	struct st_vtl_drive * vdr = drive->data;

	char * files;
	asprintf(&files, "%s/file_*", vdr->media_path);

	glob_t gl;
	int ret = glob(files, 0, NULL, &gl);
	if (ret != 0)
		return 1;

	unsigned int i;
	if (quick_mode) {
		for (i = file_position; i < gl.gl_pathc; i++) {
			int failed = unlink(gl.gl_pathv[i]);
			if (failed != 0)
				st_log_write_all(st_log_level_error, st_log_type_drive, "[%s | %s | #%td]: error while removing file (%s) because %m", drive->vendor, drive->model, drive - drive->changer->drives, gl.gl_pathv[i]);
		}
	} else {
		for (i = file_position; i < gl.gl_pathc; i++) {
			const char * params[] = { "-z", "-u", gl.gl_pathv[i] };
			struct st_util_command shred;
			st_util_command_new(&shred, "shred", params, 3);
			st_util_command_start(&shred, 1);
			st_util_command_wait(&shred, 1);
			if (shred.exited_code != 0)
				st_log_write_all(st_log_level_error, st_log_type_drive, "[%s | %s | #%td]: error while shredding file (%s), exiting code : %d", drive->vendor, drive->model, drive - drive->changer->drives, gl.gl_pathv[i], shred.exited_code);
			st_util_command_free(&shred, 1);
		}
	}

	globfree(&gl);

	return 0;
}

static void st_vtl_drive_free(struct st_drive * drive) {
	free(drive->model);
	free(drive->vendor);
	free(drive->revision);
	free(drive->serial_number);
	free(drive->db_data);
	drive->lock->ops->free(drive->lock);

	struct st_vtl_drive * vdr = drive->data;
	free(vdr->path);
	free(vdr->media_path);
	free(vdr);
}

static int st_vtl_drive_get_position(struct st_drive * drive) {
	struct st_vtl_drive * vdr = drive->data;

	if (access(vdr->media_path, F_OK))
		return -1;

	return vdr->file_position;
}

static struct st_stream_reader * st_vtl_drive_get_raw_reader(struct st_drive * drive, int file_position) {
	if (drive == NULL)
		return NULL;

	struct st_vtl_drive * vdr = drive->data;

	char * path;
	asprintf(&path, "%s/file_%d", vdr->media_path, file_position);
	if (access(path, F_OK | R_OK)) {
		free(path);
		return NULL;
	}

	st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is open for reading", drive->vendor, drive->model, drive - drive->changer->drives);

	return st_vtl_drive_reader_new(drive, path, file_position);
}

static struct st_stream_writer * st_vtl_drive_get_raw_writer(struct st_drive * drive, bool append) {
	if (drive == NULL)
		return NULL;

	struct st_vtl_drive * vdr = drive->data;

	char * path;
	asprintf(&path, "%s/file_*", vdr->media_path);

	glob_t gl;
	int failed = glob(path, 0, NULL, &gl);
	free(path);

	if (failed > 0 && failed != GLOB_NOMATCH)
		return NULL;

	char * filename;
	if (!append && failed != GLOB_NOMATCH) {
		unsigned int i;
		for (i = vdr->file_position; i < gl.gl_pathc; i++)
			unlink(gl.gl_pathv[i]);
	} else if (append) {
		vdr->file_position = gl.gl_pathc;
	}

	asprintf(&filename, "%s/file_%d", vdr->media_path, vdr->file_position);

	globfree(&gl);

	return st_vtl_drive_writer_new(drive, filename, vdr->file_position);
}

static struct st_format_reader * st_vtl_drive_get_reader(struct st_drive * drive, int file_position, struct st_stream_reader * (*filter)(struct st_stream_reader * writer, void * param), void * param) {
	struct st_stream_reader * reader = st_vtl_drive_get_raw_reader(drive, file_position);
	if (reader != NULL) {
		if (filter != NULL) {
			struct st_stream_reader * tmp_io = filter(reader, param);
			if (tmp_io != NULL)
				reader = tmp_io;
			else {
				reader->ops->free(reader);
				return NULL;
			}
		}

		return st_format_get_reader(reader, drive->slot->media->format);
	}
	return NULL;
}

static struct st_format_writer * st_vtl_drive_get_writer(struct st_drive * drive, bool append, struct st_stream_writer * (*filter)(struct st_stream_writer * writer, void * param), void * param) {
	struct st_stream_writer * writer = st_vtl_drive_get_raw_writer(drive, append);
	if (writer != NULL) {
		if (filter != NULL) {
			struct st_stream_writer * tmp_io = filter(writer, param);
			if (tmp_io != NULL)
				writer = tmp_io;
			else {
				writer->ops->free(writer);
				return NULL;
			}
		}

		return st_format_get_writer(writer, drive->slot->media->format);
	}
	return NULL;
}

void st_vtl_drive_init(struct st_drive * drive, struct st_slot * slot, char * base_dir, struct st_media_format * format) {
	char * serial_file;
	asprintf(&serial_file, "%s/serial_number", base_dir);

	char * serial = st_util_file_get_serial(serial_file);
	free(serial_file);

	struct st_vtl_drive * self = malloc(sizeof(struct st_vtl_drive));
	self->path = base_dir;
	asprintf(&self->media_path, "%s/media", base_dir);
	self->file_position = 0;
	self->format = format;

	bool has_media = st_vtl_drive_check_media(self);

	drive->device = NULL;
	drive->scsi_device = NULL;
	drive->status = has_media ? st_drive_loaded_idle : st_drive_empty_idle;
	drive->enabled = true;

	drive->density_code = format->density_code;
	drive->mode = st_media_format_mode_linear;
	drive->operation_duration = 0;
	drive->last_clean = 0;
	drive->is_empty = !has_media;

	drive->model = strdup("Stone vtl drive");
	drive->vendor = strdup("Intellique");
	drive->revision = strdup("A00");
	drive->serial_number = serial;

	drive->changer = slot->changer;
	drive->slot = slot;

	drive->ops = &st_vtl_drive_ops;
	drive->data = self;
	drive->db_data = NULL;

	drive->lock = st_ressource_new(false);
}

static void st_vtl_drive_operation_start(struct st_vtl_drive * dr) {
	gettimeofday(&dr->last_start, NULL);
}

static void st_vtl_drive_operation_stop(struct st_drive * dr) {
	struct st_vtl_drive * vdr = dr->data;

	struct timeval finish;
	gettimeofday(&finish, NULL);

	dr->operation_duration += (finish.tv_sec - vdr->last_start.tv_sec) + ((double) (finish.tv_usec - vdr->last_start.tv_usec)) / 1000000;
}

static int st_vtl_drive_rewind(struct st_drive * drive) {
	struct st_vtl_drive * vdr = drive->data;

	if (access(vdr->media_path, F_OK))
		return -1;

	vdr->file_position = 0;
	return 0;
}

static int st_vtl_drive_update_media_info(struct st_drive * drive) {
	if (drive->is_empty && drive->slot->media != NULL) {
		drive->status = st_drive_loaded_idle;
		drive->is_empty = false;

		struct st_media * media = drive->slot->media;
		if (media->status != st_media_status_in_use)
			return 0;

		st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: Checking media header", drive->vendor, drive->model, drive - drive->changer->drives);

		if (media->status == st_media_status_in_use && !st_media_check_header(drive)) {
			media->status = st_media_status_error;
			st_log_write_all(st_log_level_error, st_log_type_drive, "[%s | %s | #%td]: Error while checking media header", drive->vendor, drive->model, drive - drive->changer->drives);
			return 1;
		}
	}

	return 0;
}


static int st_vtl_drive_reader_close(struct st_stream_reader * sr) {
	struct st_vtl_drive_reader * reader = sr->data;

	if (reader->fd < 0)
		return 0;

	st_vtl_drive_operation_start(reader->drive->data);
	int failed = close(reader->fd);
	st_vtl_drive_operation_stop(reader->drive);

	if (!failed) {
		reader->fd = -1;
		free(reader->buffer);
		reader->buffer = reader->buffer_pos = NULL;

		reader->media->last_read = time(NULL);
	} else {
		reader->last_errno = errno;
		reader->media->nb_read_errors++;
	}

	return failed;
}

static bool st_vtl_drive_reader_end_of_file(struct st_stream_reader * sr) {
	struct st_vtl_drive_reader * reader = sr->data;
	return reader->end_of_file;
}

static off_t st_vtl_drive_reader_forward(struct st_stream_reader * sr, off_t offset) {
	struct st_vtl_drive_reader * reader = sr->data;
	if (reader->fd < 0)
		return -1;

	if (reader->end_of_file)
		return reader->position;

	struct stat st;
	int failed = fstat(reader->fd, &st);

	if (failed != 0) {
		reader->last_errno = errno;
		reader->media->nb_read_errors++;
		return -1;
	}

	if (st.st_size < reader->position + offset) {
		reader->buffer_pos = reader->buffer + reader->block_size;
		reader->end_of_file = true;
		return reader->position = st.st_size;
	}

	ssize_t available_in_buffer = reader->block_size - (reader->buffer_pos - reader->buffer);
	if (available_in_buffer >= offset) {
		reader->buffer_pos += offset;
		reader->position += offset;
		return reader->position;
	} else if (available_in_buffer > 0) {
		reader->buffer_pos += available_in_buffer;
		reader->position += available_in_buffer;
		offset -= available_in_buffer;
	}

	ssize_t extra_size = offset % reader->block_size;
	st_vtl_drive_operation_start(reader->drive->data);
	off_t new_pos = lseek(reader->fd, offset - extra_size, SEEK_CUR);
	st_vtl_drive_operation_stop(reader->drive);
	if (new_pos == -1) {
		reader->last_errno = errno;
		reader->media->nb_read_errors++;
		return -1;
	}

	reader->position += offset - extra_size;

	if (extra_size > 0) {
		st_vtl_drive_operation_start(reader->drive->data);
		ssize_t nb_read = read(reader->fd, reader->buffer, reader->block_size);
		st_vtl_drive_operation_stop(reader->drive);

		if (nb_read < 0) {
			reader->last_errno = errno;
			reader->media->nb_read_errors++;
			return -1;
		} else if (nb_read == 0) {
			reader->end_of_file = true;
			return reader->position;
		}

		reader->buffer_pos = reader->buffer + extra_size;
		reader->media->nb_total_read++;
	}

	return reader->position;
}

static void st_vtl_drive_reader_free(struct st_stream_reader * sr) {
	struct st_vtl_drive_reader * reader = sr->data;

	if (reader->fd > -1)
		st_vtl_drive_reader_close(sr);

	free(reader);
	free(sr);
}

static ssize_t st_vtl_drive_reader_get_block_size(struct st_stream_reader * sr) {
	struct st_vtl_drive_reader * reader = sr->data;
	return reader->block_size;
}

static int st_vtl_drive_reader_last_errno(struct st_stream_reader * sr) {
	struct st_vtl_drive_reader * reader = sr->data;
	return reader->last_errno;
}

static struct st_stream_reader * st_vtl_drive_reader_new(struct st_drive * drive, char * filename, int file_position) {
	int fd = open(filename, O_RDONLY);
	free(filename);

	if (fd < 0)
		return NULL;

	struct st_vtl_drive * vdr = drive->data;
	vdr->file_position = file_position;

	struct st_vtl_drive_reader * reader = malloc(sizeof(struct st_vtl_drive_reader));
	reader->fd = fd;
	reader->buffer = malloc(vdr->format->block_size);
	reader->block_size = vdr->format->block_size;
	reader->buffer_pos = reader->buffer + reader->block_size;
	reader->position = 0;
	reader->end_of_file = false;
	reader->last_errno = 0;

	reader->drive = drive;
	reader->media = drive->slot->media;

	struct st_stream_reader * sr = malloc(sizeof(struct st_stream_reader));
	sr->ops = &st_vtl_drive_reader_ops;
	sr->data = reader;

	drive->slot->media->read_count++;

	return sr;
}

static ssize_t st_vtl_drive_reader_position(struct st_stream_reader * sr) {
	struct st_vtl_drive_reader * reader = sr->data;
	return reader->position;
}

static ssize_t st_vtl_drive_reader_read(struct st_stream_reader * sr, void * buffer, ssize_t length) {
	if (sr == NULL || buffer == NULL || length < 0)
		return -1;

	struct st_vtl_drive_reader * reader = sr->data;
	reader->last_errno = 0;

	if (reader->fd < 0)
		return -1;

	if (length < 1 || reader->end_of_file)
		return 0;

	ssize_t nb_total_read = reader->block_size - (reader->buffer_pos - reader->buffer);
	if (nb_total_read > 0) {
		ssize_t will_copy = nb_total_read > length ? length : nb_total_read;
		memcpy(buffer, reader->buffer_pos, will_copy);

		reader->buffer_pos += will_copy;
		reader->position += will_copy;

		if (will_copy == length)
			return length;
	}

	char * c_buffer = buffer;
	while (length - nb_total_read >= reader->block_size) {
		st_vtl_drive_operation_start(reader->drive->data);
		ssize_t nb_read = read(reader->fd, c_buffer + nb_total_read, reader->block_size);
		st_vtl_drive_operation_stop(reader->drive);

		if (nb_read < 0) {
			reader->last_errno = errno;
			reader->media->nb_read_errors++;
			return nb_read;
		} else if (nb_read > 0) {
			nb_total_read += nb_read;
			reader->position += nb_read;
			reader->media->nb_total_read++;
		} else {
			reader->end_of_file = true;
			return nb_total_read;
		}
	}

	if (nb_total_read == length)
		return length;

	st_vtl_drive_operation_start(reader->drive->data);
	ssize_t nb_read = read(reader->fd, reader->buffer, reader->block_size);
	st_vtl_drive_operation_stop(reader->drive);

	if (nb_read < 0) {
		reader->last_errno = errno;
		reader->media->nb_read_errors++;
		return nb_read;
	} else if (nb_read > 0) {
		ssize_t will_copy = length - nb_total_read;
		memcpy(c_buffer + nb_total_read, reader->buffer, will_copy);
		reader->buffer_pos = reader->buffer + will_copy;
		reader->position += will_copy;
		reader->media->nb_total_read++;
		return length;
	} else {
		reader->end_of_file = true;
		return nb_total_read;
	}
}


static ssize_t st_vtl_drive_writer_before_close(struct st_stream_writer * sw, void * buffer, ssize_t length) {
	if (sw == NULL || buffer == NULL || length < 0)
		return -1;

	struct st_vtl_drive_writer * self = sw->data;
	if (self->buffer_used > 0 && self->buffer_used < self->block_size) {
		ssize_t will_copy = self->block_size - self->buffer_used;
		if (will_copy > length)
			will_copy = length;

		bzero(buffer, will_copy);

		bzero(self->buffer + self->buffer_used, will_copy);
		self->buffer_used += will_copy;
		self->position += will_copy;

		if (self->buffer_used == self->block_size) {
			st_vtl_drive_operation_start(self->drive->data);
			ssize_t nb_write = write(self->fd, self->buffer, self->block_size);
			st_vtl_drive_operation_stop(self->drive);

			if (nb_write < 0) {
				self->last_errno = errno;
				self->media->nb_write_errors++;
				return -1;
			}

			self->buffer_used = 0;
			self->media->free_block--;
			self->media->nb_total_write++;
		}

		return will_copy;
	}

	return 0;
}

static int st_vtl_drive_writer_close(struct st_stream_writer * sw) {
	if (sw == NULL || sw->data == NULL)
		return -1;

	struct st_vtl_drive_writer * self = sw->data;
	self->last_errno = 0;

	if (self->buffer_used > 0) {
		bzero(self->buffer + self->buffer_used, self->block_size - self->buffer_used);

		st_vtl_drive_operation_start(self->drive->data);
		ssize_t nb_write = write(self->fd, self->buffer, self->block_size);
		st_vtl_drive_operation_stop(self->drive);

		if (nb_write < 0) {
			self->last_errno = errno;
			self->media->nb_write_errors++;
			return -1;
		}

		self->position += nb_write;
		self->buffer_used = 0;
		self->media->free_block--;
		self->media->nb_total_write++;
	}

	if (self->fd > -1) {
		st_vtl_drive_operation_start(self->drive->data);
		int failed = close(self->fd);
		st_vtl_drive_operation_stop(self->drive);

		if (failed != 0) {
			self->last_errno = errno;
			self->media->nb_write_errors++;
			return -1;
		}

		self->fd = -1;
		self->media->last_write = time(NULL);

		struct st_vtl_drive * vdr = self->drive->data;
		vdr->file_position++;
	}

	st_vtl_media_update(self->media);

	return 0;
}

static void st_vtl_drive_writer_free(struct st_stream_writer * sw) {
	if (sw == NULL)
		return;

	struct st_vtl_drive_writer * self = sw->data;
	if (self != NULL) {
		self->fd = -1;
		free(self->filename);
		free(self->buffer);
		free(self);
	}

	sw->data = NULL;
	sw->ops = NULL;

	free(sw);
}

static ssize_t st_vtl_drive_writer_get_available_size(struct st_stream_writer * sw) {
	if (sw == NULL)
		return -1;

	struct st_vtl_drive_writer * self = sw->data;
	return self->media->free_block * self->block_size - self->buffer_used;
}

static ssize_t st_vtl_drive_writer_get_block_size(struct st_stream_writer * sw) {
	struct st_vtl_drive_writer * self = sw->data;
	return self->block_size;
}

static int st_vtl_drive_writer_last_errno(struct st_stream_writer * sw) {
	struct st_vtl_drive_writer * self = sw->data;
	return self->last_errno;
}

static struct st_stream_writer * st_vtl_drive_writer_new(struct st_drive * drive, char * filename, int file_position) {
	int fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0600);

	if (fd < 0) {
		free(filename);
		return NULL;
	}

	struct st_vtl_drive * vdr = drive->data;
	vdr->file_position = file_position;

	struct st_vtl_drive_writer * writer = malloc(sizeof(struct st_vtl_drive_writer));
	writer->fd = fd;
	writer->filename = filename;
	writer->file_position = file_position;
	writer->buffer = malloc(vdr->format->block_size);
	writer->buffer_used = 0;
	writer->block_size = vdr->format->block_size;
	writer->position = 0;
	writer->eof_reached = false;
	writer->last_errno = 0;

	writer->drive = drive;
	writer->media = drive->slot->media;

	struct st_stream_writer * sw = malloc(sizeof(struct st_stream_writer));
	sw->ops = &st_vtl_drive_writer_ops;
	sw->data = writer;

	writer->media->nb_volumes++;
	writer->media->write_count++;
	writer->media->last_write = time(NULL);

	return sw;
}

static ssize_t st_vtl_drive_writer_position(struct st_stream_writer * sw) {
	struct st_vtl_drive_writer * self = sw->data;
	return self->position;
}

static struct st_stream_reader * st_vtl_drive_writer_reopen(struct st_stream_writer * sw) {
	if (sw == NULL || sw->data == NULL)
		return NULL;

	st_vtl_drive_writer_close(sw);

	struct st_vtl_drive_writer * self = sw->data;
	return st_vtl_drive_reader_new(self->drive, strdup(self->filename), self->file_position);
}

static ssize_t st_vtl_drive_writer_write(struct st_stream_writer * sw, const void * buffer, ssize_t length) {
	if (sw == NULL || buffer == NULL || length < 0)
		return -1;

	struct st_vtl_drive_writer * self = sw->data;
	self->last_errno = 0;

	ssize_t buffer_available = self->block_size - self->buffer_used;
	if (buffer_available > length) {
		memcpy(self->buffer + self->buffer_used, buffer, length);

		self->buffer_used += length;
		self->position += length;
		return length;
	}

	memcpy(self->buffer + self->buffer_used, buffer, buffer_available);

	st_vtl_drive_operation_start(self->drive->data);
	ssize_t nb_write = write(self->fd, self->buffer, self->block_size);
	st_vtl_drive_operation_stop(self->drive);

	if (nb_write < 0) {
		self->last_errno = errno;
		self->media->nb_write_errors++;
		return -1;
	}

	self->media->free_block--;
	self->media->nb_total_write++;

	ssize_t nb_total_write = buffer_available;
	self->buffer_used = 0;
	self->position += buffer_available;

	const char * c_buffer = buffer;
	while (length - nb_total_write >= self->block_size) {
		st_vtl_drive_operation_start(self->drive->data);
		nb_write = write(self->fd, c_buffer + nb_total_write, self->block_size);
		st_vtl_drive_operation_stop(self->drive);

		if (nb_write < 0) {
			self->last_errno = errno;
			self->media->nb_write_errors++;
			return -1;
		}

		nb_total_write += nb_write;
		self->position += nb_write;
		self->media->free_block--;
		self->media->nb_total_write++;
	}

	if (length == nb_total_write)
		return length;

	self->buffer_used = length - nb_total_write;
	self->position += self->buffer_used;
	memcpy(self->buffer, c_buffer + nb_total_write, self->buffer_used);

	return length;
}

