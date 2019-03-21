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
*  Copyright (C) 2013-2019, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// dirname
#include <libgen.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strdup, strlen
#include <string.h>
// S_ISREG
#include <sys/stat.h>
// bzero
#include <strings.h>
// time
#include <time.h>
// close
#include <unistd.h>

#include <libstoriqone/config.h>
#include <libstoriqone/format.h>
#include <libstoriqone/json.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>
#include <libstoriqone-drive/drive.h>

#include "ltfs.h"
#include "../../media.h"
#include "../../io/io.h"
#include "../../util/scsi.h"
#include "../../util/st.h"
#include "../../util/xml.h"

struct sodr_tape_drive_format_ltfs_writer_private {
	int fd;
	int scsi_fd;

	ssize_t total_wrote;

	struct sodr_tape_drive_format_ltfs_file * current_file;
	const char * alternate_path;

	int last_errno;

	struct so_drive * drive;
	struct so_media * media;
	unsigned int volume_number;
	struct sodr_tape_drive_format_ltfs * ltfs_info;
	struct so_stream_writer * writer;
};

static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_add_file(struct so_format_writer * fw, const struct so_format_file * file, const char * selected_path);
static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_add_label(struct so_format_writer * fw, const char * label);
static int sodr_tape_drive_format_ltfs_writer_close(struct so_format_writer * fw, bool change_volume);
static ssize_t sodr_tape_drive_format_ltfs_writer_compute_size_of_file(struct so_format_writer * fw, const struct so_format_file * file);
static ssize_t sodr_tape_drive_format_ltfs_writer_end_of_file(struct so_format_writer * fw);
static void sodr_tape_drive_format_ltfs_writer_free(struct so_format_writer * fw);
static char * sodr_tape_drive_format_ltfs_writer_get_alternate_path(struct so_format_writer * fw);
static ssize_t sodr_tape_drive_format_ltfs_writer_get_available_size(struct so_format_writer * fw);
static ssize_t sodr_tape_drive_format_ltfs_writer_get_block_size(struct so_format_writer * fw);
static struct so_value * sodr_tape_drive_format_ltfs_writer_get_digests(struct so_format_writer * fw);
static int sodr_tape_drive_format_ltfs_writer_file_position(struct so_format_writer * fw);
static int sodr_tape_drive_format_ltfs_writer_last_errno(struct so_format_writer * fw);
static ssize_t sodr_tape_drive_format_ltfs_writer_position(struct so_format_writer * fw);
static struct so_format_reader * sodr_tape_drive_format_ltfs_writer_reopen(struct so_format_writer * fw);
static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_restart_file(struct so_format_writer * fw, const struct so_format_file * file);
static ssize_t sodr_tape_drive_format_ltfs_writer_write(struct so_format_writer * fw, const void * buffer, ssize_t length);
static ssize_t sodr_tape_drive_format_ltfs_writer_write_metadata(struct so_format_writer * fw, struct so_value * metadata);

static struct so_format_writer_ops sodr_tape_drive_format_ltfs_writer_ops = {
	.add_file             = sodr_tape_drive_format_ltfs_writer_add_file,
	.add_label            = sodr_tape_drive_format_ltfs_writer_add_label,
	.close                = sodr_tape_drive_format_ltfs_writer_close,
	.compute_size_of_file = sodr_tape_drive_format_ltfs_writer_compute_size_of_file,
	.end_of_file          = sodr_tape_drive_format_ltfs_writer_end_of_file,
	.free                 = sodr_tape_drive_format_ltfs_writer_free,
	.get_alternate_path   = sodr_tape_drive_format_ltfs_writer_get_alternate_path,
	.get_available_size   = sodr_tape_drive_format_ltfs_writer_get_available_size,
	.get_block_size       = sodr_tape_drive_format_ltfs_writer_get_block_size,
	.get_digests          = sodr_tape_drive_format_ltfs_writer_get_digests,
	.file_position        = sodr_tape_drive_format_ltfs_writer_file_position,
	.last_errno           = sodr_tape_drive_format_ltfs_writer_last_errno,
	.position             = sodr_tape_drive_format_ltfs_writer_position,
	.reopen               = sodr_tape_drive_format_ltfs_writer_reopen,
	.restart_file         = sodr_tape_drive_format_ltfs_writer_restart_file,
	.write                = sodr_tape_drive_format_ltfs_writer_write,
	.write_metadata       = sodr_tape_drive_format_ltfs_writer_write_metadata,
};


struct so_format_writer * sodr_tape_drive_format_ltfs_new_writer(struct so_drive * drive, int fd, int scsi_fd, unsigned int volume_number) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = malloc(sizeof(struct sodr_tape_drive_format_ltfs_writer_private));
	bzero(self, sizeof(struct sodr_tape_drive_format_ltfs_writer_private));

	struct so_media * media = drive->slot->media;
	struct sodr_tape_drive_media * mp = media->private_data;

	self->fd = fd;
	self->scsi_fd = scsi_fd;

	self->total_wrote = 0;
	self->current_file = NULL;
	self->alternate_path = NULL;

	self->drive = drive;
	self->media = media;
	self->volume_number = volume_number;
	self->ltfs_info = &mp->data.ltfs;
	self->writer = sodr_tape_drive_writer_get_raw_writer2(drive, fd, 1, -1, false, NULL);
	self->total_wrote = 0;

	struct so_format_writer * writer = malloc(sizeof(struct so_format_writer));
	writer->ops = &sodr_tape_drive_format_ltfs_writer_ops;
	writer->data = self;

	drive->status = so_drive_status_writing;

	return writer;
}


static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_add_file(struct so_format_writer * fw, const struct so_format_file * file, const char * selected_path) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;

	self->alternate_path = NULL;

	struct sodr_tape_drive_scsi_position position;
	int failed = sodr_tape_drive_scsi_read_position(self->scsi_fd, &position);
	if (failed != 0)
		return so_format_writer_error;

	char * selected_path_dup = strdup(selected_path);
	dirname(selected_path_dup);
	ssize_t selected_path_length = strlen(selected_path_dup);
	free(selected_path_dup);

	char * path_dup = strdup(file->filename);
	char * ptr_path_begin = path_dup + selected_path_length;
	while (*ptr_path_begin == '/')
		ptr_path_begin++;

	const unsigned long long hash_selected_path = so_string_compute_hash2(selected_path);

	struct sodr_tape_drive_format_ltfs_file * ptr_node = &self->ltfs_info->root;
	for (;;) {
		char * ptr_path_end = strchr(ptr_path_begin, '/');
		if (ptr_path_end != NULL)
			*ptr_path_end = '\0';
		const unsigned long long hash_name = so_string_compute_hash2(ptr_path_begin);

		struct sodr_tape_drive_format_ltfs_file * child_node;
		unsigned int next_index = 0;
		for (child_node = ptr_node->first_child; child_node != NULL; child_node = child_node->next_sibling)
			if (hash_name == child_node->hash_name) {
				if (hash_selected_path != child_node->hash_selected_path)
					next_index++;
				else
					break;
			}

		if (child_node == NULL) {
			child_node = malloc(sizeof(struct sodr_tape_drive_format_ltfs_file));
			bzero(child_node, sizeof(struct sodr_tape_drive_format_ltfs_file));

			if (next_index > 0) {
				asprintf(&child_node->name, "%s_%u", ptr_path_begin, next_index - 1);
				self->alternate_path = child_node->name;
			} else
				child_node->name = strdup(ptr_path_begin);

			child_node->hash_name = hash_name;
			child_node->hash_selected_path = hash_selected_path;

			child_node->file.filename = strdup(path_dup);
			child_node->file.dev = 0;
			child_node->file.rdev = 0;
			child_node->file.mode = S_IFDIR | (file->mode & 07777);
			child_node->file.uid = file->uid;
			if (file->user != NULL)
				child_node->file.user = strdup(file->user);
			child_node->file.gid = file->gid;
			if (file->group != NULL)
				child_node->file.group = strdup(file->group);
			child_node->file.ctime = file->ctime;
			child_node->file.mtime = file->mtime;
			child_node->file.is_label = false;

			child_node->file_uid = ++self->ltfs_info->highest_file_uid;

			child_node->volume_number = self->volume_number;

			child_node->parent = ptr_node;

			if (ptr_node->first_child == NULL)
				ptr_node->first_child = ptr_node->last_child = child_node;
			else {
				ptr_node->last_child->next_sibling = child_node;
				child_node->previous_sibling = ptr_node->last_child;
				ptr_node->last_child = child_node;
			}
		}

		ptr_node = child_node;

		bool finished = false;
		if (ptr_path_end != NULL) {
			*ptr_path_end = '/';
			ptr_path_begin = ptr_path_end;
			while(*ptr_path_begin == '/')
				ptr_path_begin++;
		} else
			finished = true;

		if (finished) {
			if (S_ISREG(file->mode)) {
				child_node->file.mode = S_IFREG | (file->mode & 07777);
				child_node->file.size = file->size;

				child_node->extents = malloc(sizeof(struct sodr_tape_drive_format_ltfs_extent));
				bzero(child_node->extents, sizeof(struct sodr_tape_drive_format_ltfs_extent));
				child_node->nb_extents = 1;

				child_node->extents->partition = position.partition;
				child_node->extents->start_block = position.block_position;
			}

			self->current_file = child_node;

			break;
		}
	}

	free(path_dup);

	return so_format_writer_ok;
}

static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_add_label(struct so_format_writer * fw __attribute__((unused)), const char * label __attribute__((unused))) {
	return so_format_writer_ok;
}

static int sodr_tape_drive_format_ltfs_writer_close(struct so_format_writer * fw, bool change_volume) {
	if (!change_volume)
		return 0;

	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;

	int failed = self->writer->ops->close(self->writer);
	if (failed != 0)
		return failed;

	unsigned int volume_change_reference = 0;
	failed = sodr_tape_drive_scsi_read_volume_change_reference(self->scsi_fd, &volume_change_reference);
	if (failed != 0)
		return failed;

	struct sodr_tape_drive_scsi_position position;
	failed = sodr_tape_drive_scsi_read_position(self->scsi_fd, &position);
	if (failed != 0)
		return failed;

	self->ltfs_info->data.volume_change_reference = volume_change_reference;
	self->ltfs_info->data.generation_number++;
	self->ltfs_info->data.block_position_of_last_index = position.block_position;

	// write index
	struct sodr_tape_drive_ltfs_volume_coherency previous_vcr = self->ltfs_info->data;
	struct so_value * index = sodr_tape_drive_format_ltfs_convert_index(self->media, "b", &previous_vcr, &position);

	failed = self->writer->ops->create_new_file(self->writer);
	if (failed != 0) {
		so_value_free(index);
		return failed;
	}

	ssize_t nb_write = sodr_tape_drive_xml_encode_stream(self->writer, index);
	if (nb_write < 0) {
		so_value_free(index);
		return failed;
	}

	failed = self->writer->ops->close(self->writer);
	if (failed != 0) {
		so_value_free(index);
		return failed;
	}

	failed = sodr_tape_drive_st_set_position(self->drive, self->fd, 0, -1, true, NULL);
	if (failed != 0) {
		so_value_free(index);
		return failed;
	}

	failed = sodr_tape_drive_scsi_read_position(self->scsi_fd, &position);
	if (failed != 0) {
		so_value_free(index);
		return failed;
	}

	self->ltfs_info->index.volume_change_reference = volume_change_reference;
	self->ltfs_info->index.generation_number++;
	self->ltfs_info->index.block_position_of_last_index = position.block_position;

	sodr_tape_drive_format_ltfs_update_index(index, &position);

	nb_write = sodr_tape_drive_xml_encode_stream(self->writer, index);
	so_value_free(index);

	if (nb_write < 0)
		return -1;

	failed = self->writer->ops->close(self->writer);
	if (failed != 0)
		return failed;

	failed = sodr_tape_drive_format_ltfs_update_volume_coherency_info(self->scsi_fd, self->drive, self->media->uuid, 0, &self->ltfs_info->index, NULL);
	if (failed != 0)
		return failed;

	failed = sodr_tape_drive_format_ltfs_update_volume_coherency_info(self->scsi_fd, self->drive, self->media->uuid, 1, &self->ltfs_info->data, NULL);
	if (failed != 0)
		return failed;

	close(self->scsi_fd);

	self->fd = self->scsi_fd = -1;

	return 0;
}

static ssize_t sodr_tape_drive_format_ltfs_writer_compute_size_of_file(struct so_format_writer * fw __attribute__((unused)), const struct so_format_file * file) {
	return file->size;
}

static ssize_t sodr_tape_drive_format_ltfs_writer_end_of_file(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;

	ssize_t nb_write = sodr_tape_drive_writer_flush(self->writer);
	if (nb_write < 0)
		return -1;

	return self->writer->ops->create_new_file(self->writer);
}

static void sodr_tape_drive_format_ltfs_writer_free(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;

	if (self->scsi_fd >= 0)
		close(self->scsi_fd);

	if (self->writer != NULL)
		self->writer->ops->free(self->writer);
	self->writer = NULL;

	self->fd = self->scsi_fd = -1;

	free(self);
	free(fw);
}

static char * sodr_tape_drive_format_ltfs_writer_get_alternate_path(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;

	if (self->alternate_path != NULL)
		return strdup(self->alternate_path);
	else
		return NULL;
}

static ssize_t sodr_tape_drive_format_ltfs_writer_get_available_size(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;
	return self->writer->ops->get_available_size(self->writer);
}

static ssize_t sodr_tape_drive_format_ltfs_writer_get_block_size(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;
	return self->writer->ops->get_block_size(self->writer);
}

static struct so_value * sodr_tape_drive_format_ltfs_writer_get_digests(struct so_format_writer * fw __attribute__((unused))) {
	return so_value_new_linked_list();
}

static int sodr_tape_drive_format_ltfs_writer_file_position(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;
	return self->writer->ops->file_position(self->writer);
}

static int sodr_tape_drive_format_ltfs_writer_last_errno(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;
	return self->writer->ops->last_errno(self->writer);
}

static ssize_t sodr_tape_drive_format_ltfs_writer_position(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;
	return self->total_wrote;
}

static struct so_format_reader * sodr_tape_drive_format_ltfs_writer_reopen(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;

	return self->drive->ops->get_reader(0, NULL, NULL);
}

static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_restart_file(struct so_format_writer * fw __attribute__((unused)), const struct so_format_file * file __attribute__((unused))) {
	return so_format_writer_unsupported;
}

static ssize_t sodr_tape_drive_format_ltfs_writer_write(struct so_format_writer * fw, const void * buffer, ssize_t length) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;

	ssize_t nb_write = self->writer->ops->write(self->writer, buffer, length);
	if (nb_write > 0) {
		self->current_file->extents->byte_count += nb_write;
		self->total_wrote += nb_write;
	}

	return nb_write;
}

static ssize_t sodr_tape_drive_format_ltfs_writer_write_metadata(struct so_format_writer * fw, struct so_value * metadata) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;

	struct so_value * config = so_config_get();
	const char * base_directory = "/mnt/ltfs";

	so_value_unpack(config, "{s{s{sS}}}",
		"format",
			"ltfs",
				"base directory", &base_directory
	);

	struct sodr_tape_drive_scsi_position position;
	int failed = sodr_tape_drive_scsi_read_position(self->scsi_fd, &position);
	if (failed != 0)
		return -1;

	const char * archive_name = NULL;
	so_value_unpack(metadata, "{sS}", "name", &archive_name);

	char * filename = NULL;
	int size = asprintf(&filename, "%s.meta", archive_name);
	if (size < 0)
		return -1;

	const unsigned long long hash_name = so_string_compute_hash2(filename);

	struct sodr_tape_drive_format_ltfs_file * ptr_node = &self->ltfs_info->root;
	struct sodr_tape_drive_format_ltfs_file * child_node;
	for (child_node = ptr_node->first_child; child_node != NULL; child_node = child_node->next_sibling)
		if (hash_name == child_node->hash_name)
			break;

	if (child_node == NULL) {
		child_node = malloc(sizeof(struct sodr_tape_drive_format_ltfs_file));
		bzero(child_node, sizeof(struct sodr_tape_drive_format_ltfs_file));

		child_node->name = filename;

		child_node->hash_name = hash_name;

		asprintf(&child_node->file.filename, "%s/%s.meta", base_directory, archive_name);
		child_node->file.mode = S_IFREG | 0644;
		child_node->file.ctime = child_node->file.mtime = time(NULL);

		child_node->extents = malloc(sizeof(struct sodr_tape_drive_format_ltfs_extent));
		bzero(child_node->extents, sizeof(struct sodr_tape_drive_format_ltfs_extent));
		child_node->nb_extents = 1;

		self->ltfs_info->highest_file_uid++;
		child_node->file_uid = self->ltfs_info->highest_file_uid;

		child_node->ignored = true;

		child_node->parent = ptr_node;

		if (ptr_node->first_child == NULL)
			ptr_node->first_child = ptr_node->last_child = child_node;
		else {
			ptr_node->last_child->next_sibling = child_node;
			child_node->previous_sibling = ptr_node->last_child;
			ptr_node->last_child = child_node;
		}
	} else {
		free(filename);

		self->ltfs_info->highest_file_uid++;
		child_node->file_uid = self->ltfs_info->highest_file_uid;

		child_node->file.mtime = time(NULL);
	}

	child_node->extents->partition = position.partition;
	child_node->extents->start_block = position.block_position;


	char * json = so_json_encode_to_string(metadata);
	ssize_t json_length = strlen(json);

	child_node->file.size = json_length;
	child_node->extents->byte_count = json_length;

	ssize_t nb_write = self->writer->ops->write(self->writer, json, json_length);

	free(json);

	failed = self->writer->ops->close(self->writer);
	if (failed != 0)
		return failed;

	unsigned int volume_change_reference = 0;
	failed = sodr_tape_drive_scsi_read_volume_change_reference(self->scsi_fd, &volume_change_reference);
	if (failed != 0) {
		return failed;
	}

	failed = sodr_tape_drive_scsi_read_position(self->scsi_fd, &position);
	if (failed != 0) {
		return failed;
	}

	self->ltfs_info->data.volume_change_reference = volume_change_reference;
	self->ltfs_info->data.generation_number++;
	self->ltfs_info->data.block_position_of_last_index = position.block_position;

	// write index
	struct sodr_tape_drive_ltfs_volume_coherency previous_vcr = self->ltfs_info->data;
	struct so_value * index = sodr_tape_drive_format_ltfs_convert_index(self->media, "b", &previous_vcr, &position);

	failed = self->writer->ops->create_new_file(self->writer);
	if (failed != 0) {
		so_value_free(index);
		return failed;
	}

	nb_write = sodr_tape_drive_xml_encode_stream(self->writer, index);
	if (nb_write < 0) {
		so_value_free(index);
		return failed;
	}

	failed = self->writer->ops->close(self->writer);
	if (failed != 0) {
		so_value_free(index);
		return failed;
	}

	failed = sodr_tape_drive_st_set_position(self->drive, self->fd, 0, -1, true, NULL);
	if (failed != 0) {
		so_value_free(index);
		return failed;
	}

	failed = sodr_tape_drive_scsi_read_position(self->scsi_fd, &position);
	if (failed != 0) {
		so_value_free(index);
		return failed;
	}

	self->ltfs_info->index.volume_change_reference = volume_change_reference;
	self->ltfs_info->index.generation_number++;
	self->ltfs_info->index.block_position_of_last_index = position.block_position;

	sodr_tape_drive_format_ltfs_update_index(index, &position);

	nb_write = sodr_tape_drive_xml_encode_stream(self->writer, index);
	so_value_free(index);

	if (nb_write < 0) {
		return -1;
	}

	failed = self->writer->ops->close(self->writer);
	if (failed != 0) {
		return failed;
	}

	failed = sodr_tape_drive_format_ltfs_update_volume_coherency_info(self->scsi_fd, self->drive, self->media->uuid, 0, &self->ltfs_info->index, NULL);
	if (failed != 0)
		return failed;

	failed = sodr_tape_drive_format_ltfs_update_volume_coherency_info(self->scsi_fd, self->drive, self->media->uuid, 1, &self->ltfs_info->data, NULL);
	if (failed != 0)
		return failed;

	close(self->scsi_fd);

	self->fd = self->scsi_fd = -1;

	return nb_write;
}
