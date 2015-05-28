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

#define _XOPEN_SOURCE
// free
#include <stdlib.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>
// mktime, strptime
#include <time.h>

#include <libstoriqone/value.h>

#include "ltfs.h"

static unsigned int sodr_tape_drive_format_ltfs_count_files_inner(struct so_value * index);
static struct so_value * sodr_tape_drive_format_ltfs_find_root(struct so_value * index);


unsigned int sodr_tape_drive_format_ltfs_count_files(struct so_value * index) {
	// find root directory
	struct so_value * root = sodr_tape_drive_format_ltfs_find_root(index);

	if (root != NULL)
		return sodr_tape_drive_format_ltfs_count_files_inner(root);
	return 0;
}

static unsigned int sodr_tape_drive_format_ltfs_count_files_inner(struct so_value * index) {
	struct so_value * children = NULL;
	so_value_unpack(index, "{so}", &children);

	unsigned int nb_files = 0;
	struct so_value_iterator * iter = so_value_list_get_iterator(children);
	while (so_value_iterator_has_next(iter)) {
		nb_files++;
	}
	so_value_iterator_free(iter);

	return nb_files;
}

static struct so_value * sodr_tape_drive_format_ltfs_find_root(struct so_value * index) {
	struct so_value * children = NULL;
	so_value_unpack(index, "{so}", &children);

	struct so_value_iterator * iter = so_value_list_get_iterator(children);
	struct so_value * root = NULL;
	while (root == NULL && so_value_iterator_has_next(iter)) {
		root = so_value_iterator_get_value(iter, false);

		char * elt_name = NULL;
		so_value_unpack(root, "{ss}", "name", &elt_name);

		if (strcmp(elt_name, "directory") != 0)
			root = NULL;

		free(elt_name);
	}
	so_value_iterator_free(iter);

	return root;
}

time_t sodr_tape_drive_format_ltfs_parse_time(const char * date) {
	struct tm tm;
	bzero(&tm, sizeof(tm));

	strptime(date, "%FT%T", &tm);

	return mktime(&tm) - timezone;
}

