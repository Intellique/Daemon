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

// free
#include <stdlib.h>
// bzero
#include <strings.h>

#include <libstoriqone/format.h>
#include <libstoriqone/value.h>


struct so_value * so_format_file_convert(const struct so_format_file * file) {
	struct so_value * vlink = so_value_new_null();
	if (file->link)
		vlink = so_value_new_string(file->link);

	return so_value_pack("{sssosisisisisisisssisssisisb}",
		"filename", file->filename,
		"link", vlink,

		"position", file->position,
		"size", file->size,

		"dev", (long) file->dev,
		"rdev", (long) file->rdev,
		"mode", (long) file->mode,
		"uid", (long) file->uid,
		"user", file->user,
		"gid", (long) file->gid,
		"group", file->group,

		"ctime", file->ctime,
		"mtime", file->mtime,

		"is label", file->is_label
	);
}

void so_format_file_free(struct so_format_file * file) {
	free(file->filename);
	free(file->link);
	free(file->user);
	free(file->group);
}

void so_format_file_init(struct so_format_file * file) {
	bzero(file, sizeof(struct so_format_file));
}

void so_format_file_sync(struct so_format_file * file, struct so_value * new_file) {
	struct so_value * vlink = NULL;
	long dev, rdev, mode, uid, gid;

	so_value_unpack(new_file, "{sssosisisisisisisssisssisisb}",
		"filename", &file->filename,
		"link", &vlink,

		"position", &file->position,
		"size", &file->size,

		"dev", &dev,
		"rdev", &rdev,
		"mode", &mode,
		"uid", &uid,
		"user", &file->user,
		"gid", &gid,
		"group", &file->group,

		"ctime", &file->ctime,
		"mtime", &file->mtime,

		"is label", &file->is_label
	);

	file->dev = dev;
	file->rdev = rdev;
	file->mode = mode;
	file->uid = uid;
	file->gid = gid;
}

