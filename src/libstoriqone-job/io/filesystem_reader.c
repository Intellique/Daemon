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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// scandir
#include <dirent.h>
// errno
#include <errno.h>
// open
#include <fcntl.h>
// dgettext
#include <libintl.h>
// bool
#include <stdbool.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>
// lstat, open
#include <sys/stat.h>
// lseek, lstat, open
#include <sys/types.h>
// access, close, lseek, lstat, read, readlink
#include <unistd.h>

#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/format.h>
#include <libstoriqone/log.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/io.h>

#include "config.h"

struct soj_format_reader_filesystem_private {
	char * path;
	int last_errno;

	struct soj_format_reader_filesystem_node {
		int fd;
		struct stat st;
		char * path;
		u_int64_t nb_file_to_backup;

		struct soj_format_reader_filesystem_node * parent, * first_child, * last_child, * next_sibling;
	} * root, * current;
	bool fetch;
};

static int soj_format_reader_filesystem_close(struct so_format_reader * fr);
static bool soj_format_reader_filesystem_end_of_file(struct so_format_reader * fr);
static enum so_format_reader_header_status soj_format_reader_filesystem_forward(struct so_format_reader * fr, off_t offset);
static void soj_format_reader_filesystem_free(struct so_format_reader * fr);
static ssize_t soj_format_reader_filesystem_get_block_size(struct so_format_reader * fr);
static struct so_value * soj_format_reader_filesystem_get_digests(struct so_format_reader * fr);
static enum so_format_reader_header_status soj_format_reader_filesystem_get_header(struct so_format_reader * fr, struct so_format_file * file);
static char * soj_format_reader_filesystem_get_root(struct so_format_reader * fr);
static int soj_format_reader_filesystem_last_errno(struct so_format_reader * fr);
static ssize_t soj_format_reader_filesystem_position(struct so_format_reader * fr);
static ssize_t soj_format_reader_filesystem_read(struct so_format_reader * fr, void * buffer, ssize_t length);
static ssize_t soj_format_reader_filesystem_read_to_end_of_data(struct so_format_reader * fr);
static int soj_format_reader_filesystem_rewind(struct so_format_reader * fr);
static enum so_format_reader_header_status soj_format_reader_filesystem_skip_file(struct so_format_reader * fr);

static struct soj_format_reader_filesystem_node * soj_format_reader_filesystem_node_new(char * path, struct so_archive * archive, struct so_database_connection * db_connection);
static void soj_format_reader_filesystem_node_sync(struct soj_format_reader_filesystem_node * node, struct so_format_file * file);

static struct so_format_reader_ops soj_format_reader_filesystem_ops = {
		.close               = soj_format_reader_filesystem_close,
		.end_of_file         = soj_format_reader_filesystem_end_of_file,
		.forward             = soj_format_reader_filesystem_forward,
		.free                = soj_format_reader_filesystem_free,
		.get_block_size      = soj_format_reader_filesystem_get_block_size,
		.get_digests         = soj_format_reader_filesystem_get_digests,
		.get_header          = soj_format_reader_filesystem_get_header,
		.get_root            = soj_format_reader_filesystem_get_root,
		.last_errno          = soj_format_reader_filesystem_last_errno,
		.position            = soj_format_reader_filesystem_position,
		.read                = soj_format_reader_filesystem_read,
		.read_to_end_of_data = soj_format_reader_filesystem_read_to_end_of_data,
		.rewind              = soj_format_reader_filesystem_rewind,
		.skip_file           = soj_format_reader_filesystem_skip_file,
};


struct so_format_reader * soj_io_filesystem_reader(const char * path, struct so_archive * archive, struct so_database_connection * db_connection) {
	if (access(path, F_OK) != 0)
		return NULL;

	struct soj_format_reader_filesystem_private * self = malloc(sizeof(struct soj_format_reader_filesystem_private));
	bzero(self, sizeof(struct soj_format_reader_filesystem_private));
	self->path = strdup(path);
	self->last_errno = 0;

	char * new_path = strdup(path);
	so_string_delete_double_char(new_path, '/');

	self->current = self->root = soj_format_reader_filesystem_node_new(new_path, archive, db_connection);
	self->fetch = false;

	struct so_format_reader * reader = malloc(sizeof(struct so_format_reader));
	bzero(reader, sizeof(struct so_format_reader));
	reader->ops = &soj_format_reader_filesystem_ops;
	reader->data = self;

	return reader;
}


static int soj_format_reader_filesystem_close(struct so_format_reader * fr) {
	struct soj_format_reader_filesystem_private * self = fr->data;
	self->current = NULL;
	return 0;
}

static bool soj_format_reader_filesystem_end_of_file(struct so_format_reader * fr) {
	struct soj_format_reader_filesystem_private * self = fr->data;
	return self->current == NULL;
}

static enum so_format_reader_header_status soj_format_reader_filesystem_forward(struct so_format_reader * fr, off_t offset) {
	struct soj_format_reader_filesystem_private * self = fr->data;

	if (self->current == NULL)
		return so_format_reader_header_not_found;

	off_t position = lseek(self->current->fd, offset * self->root->st.st_blksize, SEEK_CUR);
	if (position == (off_t) -1) {
		self->last_errno = errno;
		return so_format_reader_header_not_found;
	}

	return so_format_reader_header_ok;
}

static void soj_format_reader_filesystem_free(struct so_format_reader * fr) {
	struct soj_format_reader_filesystem_private * self = fr->data;

	free(self->path);

	struct soj_format_reader_filesystem_node * node = self->root;
	while (node != NULL) {
		if (node->fd > -1) {
			close(node->fd);
			node->fd = -1;
		}

		if (node->path != NULL) {
			free(node->path);
			node->path = NULL;
		}

		if (node->first_child != NULL) {
			struct soj_format_reader_filesystem_node * child = node->first_child;
			node->first_child = NULL;
			node = child;
		} else if (node->next_sibling != NULL) {
			struct soj_format_reader_filesystem_node * sibling = node->next_sibling;
			free(node);
			node = sibling;
		} else {
			struct soj_format_reader_filesystem_node * parent = node->parent;
			free(node);
			node = parent;
		}
	}

	free(self);
	free(fr);
}

static ssize_t soj_format_reader_filesystem_get_block_size(struct so_format_reader * fr) {
	struct soj_format_reader_filesystem_private * self = fr->data;
	if (self->root != NULL)
		return self->root->st.st_blksize;
	return -1;
}

static struct so_value * soj_format_reader_filesystem_get_digests(struct so_format_reader * fr __attribute__((unused))) {
	return so_value_new_linked_list();
}

static enum so_format_reader_header_status soj_format_reader_filesystem_get_header(struct so_format_reader * fr, struct so_format_file * file) {
	struct soj_format_reader_filesystem_private * self = fr->data;

	if (self->root->nb_file_to_backup == 0)
		return so_format_reader_header_not_found;

	if (!self->fetch) {
		self->fetch = true;
		soj_format_reader_filesystem_node_sync(self->root, file);
		return so_format_reader_header_ok;
	}

	while (self->current != NULL) {
		if (self->current->first_child != NULL) {
			self->current = self->current->first_child;
			soj_format_reader_filesystem_node_sync(self->current, file);
			return so_format_reader_header_ok;
		} else if (self->current->next_sibling != NULL) {
			self->current = self->current->next_sibling;
			soj_format_reader_filesystem_node_sync(self->current, file);
			return so_format_reader_header_ok;
		} else {
			do {
				self->current = self->current->parent;
				if (self->current == NULL)
					return so_format_reader_header_not_found;
				else if (self->current->next_sibling != NULL) {
					self->current = self->current->next_sibling;
					soj_format_reader_filesystem_node_sync(self->current, file);
					return so_format_reader_header_ok;
				}
			} while (self->current != NULL);
		}

		self->current = self->current->parent;
	}

	return so_format_reader_header_not_found;
}

static char * soj_format_reader_filesystem_get_root(struct so_format_reader * fr) {
	struct soj_format_reader_filesystem_private * self = fr->data;
	return strdup(self->path);
}

static int soj_format_reader_filesystem_last_errno(struct so_format_reader * fr) {
	struct soj_format_reader_filesystem_private * self = fr->data;
	return self->last_errno;
}

static ssize_t soj_format_reader_filesystem_position(struct so_format_reader * fr __attribute__((unused))) {
	return -1;
}

static ssize_t soj_format_reader_filesystem_read(struct so_format_reader * fr, void * buffer, ssize_t length) {
	struct soj_format_reader_filesystem_private * self = fr->data;

	if (self->current == NULL)
		return -1;

	if (self->current->fd < 0) {
		self->current->fd = open(self->current->path, O_RDONLY);
		if (self->current->fd < 0) {
			self->last_errno = errno;
			return -1;
		}
	}

	ssize_t nb_read = read(self->current->fd, buffer, length);
	if (nb_read < 0)
		self->last_errno = errno;

	return nb_read;
}

static ssize_t soj_format_reader_filesystem_read_to_end_of_data(struct so_format_reader * fr) {
	struct soj_format_reader_filesystem_private * self = fr->data;
	self->current = NULL;
	return 0;
}

static int soj_format_reader_filesystem_rewind(struct so_format_reader * fr) {
	struct soj_format_reader_filesystem_private * self = fr->data;

	self->current = self->root;
	self->fetch = false;

	return 0;
}

static enum so_format_reader_header_status soj_format_reader_filesystem_skip_file(struct so_format_reader * fr) {
	struct soj_format_reader_filesystem_private * self = fr->data;

	if (self->current == NULL)
		return so_format_reader_header_not_found;

	if (self->current->fd > -1)
		close(self->current->fd);
	self->current->fd = -1;
	return so_format_reader_header_ok;
}


static struct soj_format_reader_filesystem_node * soj_format_reader_filesystem_node_new(char * path, struct so_archive * archive, struct so_database_connection * db_connection) {
	struct soj_format_reader_filesystem_node * node = malloc(sizeof(struct soj_format_reader_filesystem_node));
	bzero(node, sizeof(struct soj_format_reader_filesystem_node));

	node->fd = -1;
	node->path = path;
	lstat(node->path, &node->st);
	node->nb_file_to_backup = 0;

	if (S_ISREG(node->st.st_mode)) {
		if (archive == NULL || db_connection->ops->check_archive_file_up_to_date(db_connection, archive, path))
			node->nb_file_to_backup++;
	} else if (S_ISDIR(node->st.st_mode)) {
		struct dirent ** files = NULL;
		int i, nb_files = scandir(path, &files, so_file_basic_scandir_filter, versionsort);

		for (i = 0; i < nb_files; i++) {
			char * sub_path = NULL;
			asprintf(&sub_path, "%s/%s", path, files[i]->d_name);
			so_string_delete_double_char(sub_path, '/');

			struct soj_format_reader_filesystem_node * child = NULL;
			if (archive != NULL && db_connection->ops->check_archive_file_up_to_date(db_connection, archive, sub_path))
				free(sub_path);
			else {
				child = soj_format_reader_filesystem_node_new(sub_path, archive, db_connection);
				child->parent = node;

				if (node->first_child == NULL)
					node->first_child = node->last_child = child;
				else
					node->last_child = node->last_child->next_sibling = child;

				do {
					child->nb_file_to_backup++;
					child = child->parent;
				} while (child != NULL);
			}

			free(files[i]);
		}
		free(files);
	}

	return node;
}

static void soj_format_reader_filesystem_node_sync(struct soj_format_reader_filesystem_node * node, struct so_format_file * file) {
	bool fixed = false;

	file->filename = so_string_dup_and_fix(node->path, &fixed);
	file->link = NULL;

	if (fixed) {
		so_log_write(so_log_level_warning,
			dgettext("libstoriqone-job", "Filename '%s' contains an invalid UTF-8 sequence"),
			file->filename);
	}

	file->position = 0;
	file->size = 0;

	file->dev = node->st.st_dev;
	file->rdev = node->st.st_rdev;
	file->mode = node->st.st_mode;
	file->uid = node->st.st_uid;
	file->user = so_file_uid2name(node->st.st_uid);
	file->gid = node->st.st_gid;
	file->group = so_file_gid2name(node->st.st_gid);

	file->ctime = node->st.st_ctime;
	file->mtime = node->st.st_mtime;

	file->is_label = false;

	if (S_ISREG(node->st.st_mode)) {
		file->size = node->st.st_size;
	} else if (S_ISLNK(node->st.st_mode)) {
		file->size = node->st.st_size;
		char * raw_link = malloc(file->size + 1);
		ssize_t nb_write = readlink(node->path, raw_link, file->size + 1);

		if (nb_write < 0) {
			so_log_write(so_log_level_error,
				dgettext("libstoriqone-job", "Failed to read link of file '%s' because %m"),
				file->filename);

			file->link = raw_link;
			file->link[0] = '\0';
		} else {
			raw_link[nb_write] = '\0';

			fixed = false;
			file->link = so_string_dup_and_fix(raw_link, &fixed);

			if (fixed) {
				so_log_write(so_log_level_warning,
					dgettext("libstoriqone-job", "Symbolic link '%s' -> '%s' contains an invalid UTF-8 sequence"),
					file->filename, file->link);
			}

			free(raw_link);
		}
	}
}
