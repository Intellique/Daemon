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

#define _GNU_SOURCE
// scandir
#include <dirent.h>
// errno
#include <errno.h>
// open
#include <fcntl.h>
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

#include <libstoriqone/file.h>
#include <libstoriqone/format.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/io.h>

struct soj_format_reader_filesystem_private {
	int last_errno;

	struct soj_format_reader_filesystem_node {
		int fd;
		struct stat st;
		char * path;

		struct dirent ** files;
		int i_file, nb_files;

		struct soj_format_reader_filesystem_node * parent, * child;
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
static int soj_format_reader_filesystem_last_errno(struct so_format_reader * fr);
static ssize_t soj_format_reader_filesystem_position(struct so_format_reader * fr);
static ssize_t soj_format_reader_filesystem_read(struct so_format_reader * fr, void * buffer, ssize_t length);
static ssize_t soj_format_reader_filesystem_read_to_end_of_data(struct so_format_reader * fr);
static enum so_format_reader_header_status soj_format_reader_filesystem_skip_file(struct so_format_reader * fr);

static struct soj_format_reader_filesystem_node * soj_format_reader_filesystem_node_add(struct soj_format_reader_filesystem_node * node);
static void soj_format_reader_filesystem_node_free(struct soj_format_reader_filesystem_node * root, struct soj_format_reader_filesystem_node * node);
static struct soj_format_reader_filesystem_node * soj_format_reader_filesystem_node_new(char * path);
static void soj_format_reader_filesystem_node_sync(struct soj_format_reader_filesystem_node * node, struct so_format_file * file);

static struct so_format_reader_ops soj_format_reader_filesystem_ops = {
		.close               = soj_format_reader_filesystem_close,
		.end_of_file         = soj_format_reader_filesystem_end_of_file,
		.forward             = soj_format_reader_filesystem_forward,
		.free                = soj_format_reader_filesystem_free,
		.get_block_size      = soj_format_reader_filesystem_get_block_size,
		.get_digests         = soj_format_reader_filesystem_get_digests,
		.get_header          = soj_format_reader_filesystem_get_header,
		.last_errno          = soj_format_reader_filesystem_last_errno,
		.position            = soj_format_reader_filesystem_position,
		.read                = soj_format_reader_filesystem_read,
		.read_to_end_of_data = soj_format_reader_filesystem_read_to_end_of_data,
		.skip_file           = soj_format_reader_filesystem_skip_file,
};


struct so_format_reader * soj_io_filesystem_reader(const char * path) {
	if (access(path, F_OK) != 0)
		return NULL;

	struct soj_format_reader_filesystem_private * self = malloc(sizeof(struct soj_format_reader_filesystem_private));
	bzero(self, sizeof(struct soj_format_reader_filesystem_private));
	self->last_errno = 0;

	self->current = self->root = soj_format_reader_filesystem_node_new(strdup(path));
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

	struct soj_format_reader_filesystem_node * ptr = self->root;
	while (ptr != NULL) {
		if (ptr->fd > -1)
			close(ptr->fd);
		free(ptr->path);

		int i;
		for (i = ptr->i_file; i < ptr->nb_files; i++)
			free(ptr->files[i]);
		free(ptr->files);

		struct soj_format_reader_filesystem_node * next = ptr->child;
		free(ptr);
		ptr = next;
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

	if (!self->fetch) {
		self->fetch = true;
		soj_format_reader_filesystem_node_sync(self->root, file);
		return so_format_reader_header_ok;
	}

	while (self->current != NULL) {
		if (self->current->i_file < self->current->nb_files) {
			self->current = soj_format_reader_filesystem_node_add(self->current);
			soj_format_reader_filesystem_node_sync(self->current, file);
			return so_format_reader_header_ok;
		}

		struct soj_format_reader_filesystem_node * parent = self->current->parent;
		soj_format_reader_filesystem_node_free(self->root, self->current);
		self->current = parent;
		if (parent == NULL)
			self->root = NULL;

	}

	return so_format_reader_header_not_found;
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

static enum so_format_reader_header_status soj_format_reader_filesystem_skip_file(struct so_format_reader * fr) {
	struct soj_format_reader_filesystem_private * self = fr->data;

	if (self->current == NULL)
		return so_format_reader_header_not_found;

	if (self->current->fd > -1)
		close(self->current->fd);
	self->current->fd = -1;
	return so_format_reader_header_ok;
}


static struct soj_format_reader_filesystem_node * soj_format_reader_filesystem_node_add(struct soj_format_reader_filesystem_node * node) {
	char * path;
	asprintf(&path, "%s/%s", node->path, node->files[node->i_file]->d_name);
	free(node->files[node->i_file]);
	node->i_file++;

	node->child = soj_format_reader_filesystem_node_new(path);
	node->child->parent = node;

	return node->child;
}

static void soj_format_reader_filesystem_node_free(struct soj_format_reader_filesystem_node * root, struct soj_format_reader_filesystem_node * node) {
	if (node->fd > -1)
		close(node->fd);
	free(node->path);

	int i;
	for (i = node->i_file; i < node->nb_files; i++)
		free(node->files[i]);
	free(node->files);

	if (node != root)
		node->parent->child = NULL;
	free(node);
}

static struct soj_format_reader_filesystem_node * soj_format_reader_filesystem_node_new(char * path) {
	struct soj_format_reader_filesystem_node * node = malloc(sizeof(struct soj_format_reader_filesystem_node));
	bzero(node, sizeof(struct soj_format_reader_filesystem_node));

	node->fd = -1;
	node->path = path;
	lstat(node->path, &node->st);

	if (S_ISDIR(node->st.st_mode)) {
		node->nb_files = scandir(node->path, &node->files, so_file_basic_scandir_filter, versionsort);
		node->i_file = 0;
	} else if (S_ISREG(node->st.st_mode)) {
		node->fd = open(node->path, O_RDONLY);
	// } else if (S_ISLNK(child->st.st_mode)) {
	}

	return node;
}

static void soj_format_reader_filesystem_node_sync(struct soj_format_reader_filesystem_node * node, struct so_format_file * file) {
	file->filename = strdup(node->path);
	file->link = NULL;

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
		file->link = malloc(file->size + 1);
		readlink(file->filename, file->link, file->size + 1);
	}
}

