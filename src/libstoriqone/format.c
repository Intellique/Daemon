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

// free
#include <stdlib.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>

#include <libstoriqone/format.h>
#include <libstoriqone/value.h>


struct so_value * so_format_file_convert(const struct so_format_file * file) {
	struct so_value * vlink = so_value_new_null();
	if (file->link)
		vlink = so_value_new_string(file->link);

	return so_value_pack("{sssoszszsususususssusssIsIsb}",
		"filename", file->filename,
		"link", vlink,

		"position", file->position,
		"size", file->size,

		"dev", file->dev,
		"rdev", file->rdev,
		"mode", file->mode,
		"uid", file->uid,
		"user", file->user,
		"gid", file->gid,
		"group", file->group,

		"ctime", (long) file->ctime,
		"mtime", (long) file->mtime,

		"is label", file->is_label
	);
}

void so_format_file_copy(struct so_format_file * dest, const struct so_format_file * src) {
	if (src->filename != NULL)
		dest->filename = strdup(src->filename);
	if (src->link != NULL)
		dest->link = strdup(src->link);

	dest->position = src->position;
	dest->size = src->size;

	dest->dev = src->dev;
	dest->rdev = src->rdev;
	dest->mode = src->mode;
	dest->uid = src->uid;
	if (src->user != NULL)
		dest->user = strdup(src->user);
	dest->gid = src->gid;
	if (src->group != NULL)
		dest->group = strdup(src->group);

	dest->ctime = src->ctime;
	dest->mtime = src->mtime;

	dest->is_label = src->is_label;
}

void so_format_file_free(struct so_format_file * file) {
	free(file->filename);
	free(file->link);
	free(file->user);
	free(file->group);

	file->filename = NULL;
	file->link = NULL;
	file->user = NULL;
	file->group = NULL;
}

void so_format_file_init(struct so_format_file * file) {
	bzero(file, sizeof(struct so_format_file));
}

void so_format_file_sync(struct so_format_file * file, struct so_value * new_file) {
	so_value_unpack(new_file, "{ssssszszsususususssusssisisb}",
		"filename", &file->filename,
		"link", &file->link,

		"position", &file->position,
		"size", &file->size,

		"dev", &file->dev,
		"rdev", &file->rdev,
		"mode", &file->mode,
		"uid", &file->uid,
		"user", &file->user,
		"gid", &file->gid,
		"group", &file->group,

		"ctime", &file->ctime,
		"mtime", &file->mtime,

		"is label", &file->is_label
	);
}

