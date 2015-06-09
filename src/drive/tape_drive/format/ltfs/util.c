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
#define _XOPEN_SOURCE
// bool
#include <stdbool.h>
// calloc, free
#include <stdlib.h>
// asprintf, sscanf
#include <stdio.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>
// mktime, strptime
#include <time.h>

#include <libstoriqone/value.h>

#include "ltfs.h"
#include "../../media.h"

static unsigned int sodr_tape_drive_format_ltfs_count_extent(struct so_value * extents);
static unsigned int sodr_tape_drive_format_ltfs_count_files_inner(struct so_value * index);
static struct so_value * sodr_tape_drive_format_ltfs_find(struct so_value * index, const char * node);
static void sodr_tape_drive_format_ltfs_parse_index_inner(struct sodr_tape_drive_format_ltfs * self, struct so_value * index, const char * path, unsigned int * position);


static unsigned int sodr_tape_drive_format_ltfs_count_extent(struct so_value * extents) {
	struct so_value * children = NULL;
	so_value_unpack(extents, "{so}", "children", &children);

	unsigned int nb_extents = 0;
	struct so_value_iterator * iter = so_value_list_get_iterator(children);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * elt = so_value_iterator_get_value(iter, false);

		char * elt_name = NULL;
		so_value_unpack(elt, "{ss}", "name", &elt_name);

		if (elt_name == NULL)
			continue;

		if (strcmp(elt_name, "extent") == 0)
			nb_extents++;

		free(elt_name);
	}
	so_value_iterator_free(iter);

	return nb_extents;
}

unsigned int sodr_tape_drive_format_ltfs_count_files(struct so_value * index) {
	// find root directory
	struct so_value * root = sodr_tape_drive_format_ltfs_find(index, "directory");

	if (root != NULL)
		return sodr_tape_drive_format_ltfs_count_files_inner(root);
	return 0;
}

static unsigned int sodr_tape_drive_format_ltfs_count_files_inner(struct so_value * index) {
	struct so_value * files = sodr_tape_drive_format_ltfs_find(index, "contents");
	if (files == NULL)
		return 0;

	struct so_value * children = NULL;
	if (so_value_unpack(files, "{so}", "children", &children) < 1)
		return 0;

	unsigned int nb_files = 0;
	struct so_value_iterator * iter = so_value_list_get_iterator(children);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * elt = so_value_iterator_get_value(iter, false);

		char * elt_name = NULL;
		so_value_unpack(elt, "{ss}", "name", &elt_name);

		if (elt_name == NULL)
			continue;

		nb_files++;

		if (strcmp(elt_name, "directory") == 0)
			nb_files += sodr_tape_drive_format_ltfs_count_files_inner(elt);

		free(elt_name);
	}
	so_value_iterator_free(iter);

	return nb_files;
}

static struct so_value * sodr_tape_drive_format_ltfs_find(struct so_value * index, const char * node) {
	struct so_value * children = NULL;
	so_value_unpack(index, "{so}", "children", &children);

	struct so_value_iterator * iter = so_value_list_get_iterator(children);
	struct so_value * elt = NULL;
	while (elt == NULL && so_value_iterator_has_next(iter)) {
		elt = so_value_iterator_get_value(iter, false);

		char * elt_name = NULL;
		so_value_unpack(elt, "{ss}", "name", &elt_name);

		if (strcmp(elt_name, node) != 0)
			elt = NULL;

		free(elt_name);
	}
	so_value_iterator_free(iter);

	return elt;
}

void sodr_tape_drive_format_ltfs_parse_index(struct sodr_tape_drive_media * mp, struct so_value * index) {
	// find root directory
	struct so_value * root = sodr_tape_drive_format_ltfs_find(index, "directory");
	if (root == NULL)
		return;

	struct sodr_tape_drive_format_ltfs * self = &mp->data.ltfs;
	self->nb_files = sodr_tape_drive_format_ltfs_count_files_inner(root);
	self->files = calloc(self->nb_files, sizeof(struct sodr_tape_drive_format_ltfs_file));

	struct so_value * contents = sodr_tape_drive_format_ltfs_find(root, "contents");
	if (contents == NULL)
		return;

	struct so_value * files = NULL;
	so_value_unpack(contents, "{so}", "children", &files);

	unsigned int position = 0;

	struct so_value_iterator * iter = so_value_list_get_iterator(files);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * file = so_value_iterator_get_value(iter, false);
		sodr_tape_drive_format_ltfs_parse_index_inner(self, file, "/", &position);
	}
	so_value_iterator_free(iter);
}

static void sodr_tape_drive_format_ltfs_parse_index_inner(struct sodr_tape_drive_format_ltfs * self, struct so_value * index, const char * path, unsigned int * position) {
	struct so_value * children = NULL;
	if (so_value_unpack(index, "{so}", "children", &children) < 1)
		return;

	struct sodr_tape_drive_format_ltfs_file * file = self->files + *position;
	(*position)++;

	struct so_value_iterator * iter = so_value_list_get_iterator(children);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * elt = so_value_iterator_get_value(iter, false);

		char * elt_name = NULL;
		so_value_unpack(elt, "{ss}", "name", &elt_name);

		if (elt_name == NULL)
			continue;

		if (strcmp(elt_name, "name") == 0) {
			char * file_name = NULL;
			so_value_unpack(elt, "{ss}", "value", &file_name);

			if (strcmp(path, "/") == 0)
				path++;

			asprintf(&file->file.filename, "%s/%s", path, file_name);

			free(file_name);
		} else if (strcmp(elt_name, "creationtime") == 0) {
			char * ctime = NULL;
			so_value_unpack(elt, "{ss}", "value", &ctime);
			if (ctime != NULL)
				file->file.ctime = sodr_tape_drive_format_ltfs_parse_time(ctime);
			free(ctime);
		} else if (strcmp(elt_name, "modifytime") == 0) {
			char * mtime = NULL;
			so_value_unpack(elt, "{ss}", "value", &mtime);
			if (mtime != NULL)
				file->file.mtime = sodr_tape_drive_format_ltfs_parse_time(mtime);
			free(mtime);
		} else if (strcmp(elt_name, "length") == 0) {
			char * length = NULL;
			so_value_unpack(elt, "{ss}", "value", &length);
			if (length != NULL)
				sscanf(length, "%zd", &file->file.size);
			free(length);
		} else if (strcmp(elt_name, "extentinfo") == 0) {
			file->nb_extents = sodr_tape_drive_format_ltfs_count_extent(elt);
			file->extents = calloc(file->nb_extents, sizeof(struct sodr_tape_drive_format_ltfs_extent));

			struct so_value * children = NULL;
			so_value_unpack(elt, "{so}", "children", &children);

			struct so_value_iterator * iter = so_value_list_get_iterator(children);
			unsigned int i = 0;
			while (so_value_iterator_has_next(iter)) {
				struct so_value * vextent = so_value_iterator_get_value(iter, false);

				char * child_name = NULL;
				so_value_unpack(vextent, "{ss}", "name", &child_name);

				if (child_name == NULL)
					continue;

				if (strcmp(child_name, "extent") != 0) {
					free(child_name);
					continue;
				}

				free(child_name);

				struct sodr_tape_drive_format_ltfs_extent * ext = file->extents + i;

				struct so_value * vextent_infos = NULL;
				so_value_unpack(vextent, "{so}", "children", &vextent_infos);

				struct so_value_iterator * iter_extent_info = so_value_list_get_iterator(vextent_infos);
				while (so_value_iterator_has_next(iter_extent_info)) {
					struct so_value * info = so_value_iterator_get_value(iter_extent_info, false);

					char * info_name = NULL, * info_value = NULL;
					so_value_unpack(info, "{ssss}", "name", &info_name, "value", &info_value);

					if (info_name == NULL)
						continue;

					if (info_value == NULL) {
						free(info_name);
						continue;
					}

					if (strcmp(info_name, "fileoffset") == 0) {
						sscanf(info_value, "%zd", &file->file.position);
						ext->file_offset = file->file.position;
					} else if (strcmp(info_name, "partition") == 0)
						ext->partition = info_value[0] - 'a';
					else if (strcmp(info_name, "startblock") == 0)
						sscanf(info_value, "%zd", &ext->start_block);
					else if (strcmp(info_name, "byteoffset") == 0)
						sscanf(info_value, "%zd", &ext->byte_offset);
					else if (strcmp(info_name, "bytecount") == 0)
						sscanf(info_value, "%zd", &ext->byte_count);

					free(info_name);
					free(info_value);
				}
				so_value_iterator_free(iter_extent_info);
			}
			so_value_iterator_free(iter);
		} else if (strcmp(elt_name, "symlink") == 0)
			so_value_unpack(elt, "{ss}", "value", &file->file.link);

		free(elt_name);
	}
	so_value_iterator_free(iter);

	char * elt_name = NULL;
	so_value_unpack(index, "{ss}", "name", &elt_name);

	if (strcmp(elt_name, "directory") == 0) {
		struct so_value * contents = sodr_tape_drive_format_ltfs_find(index, "contents");

		struct so_value * files = NULL;
		so_value_unpack(contents, "{so}", "children", &files);

		struct so_value_iterator * iter = so_value_list_get_iterator(files);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * child = so_value_iterator_get_value(iter, false);
			sodr_tape_drive_format_ltfs_parse_index_inner(self, child, file->file.filename, position);
		}
		so_value_iterator_free(iter);
	}

	free(elt_name);
}

time_t sodr_tape_drive_format_ltfs_parse_time(const char * date) {
	struct tm tm;
	bzero(&tm, sizeof(tm));

	strptime(date, "%FT%T", &tm);

	return mktime(&tm) - timezone;
}

