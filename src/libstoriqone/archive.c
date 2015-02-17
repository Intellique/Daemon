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

// dgettext, gettext
#include <libintl.h>
#include <stddef.h>

#define gettext_noop(String) String

#include <libstoriqone/archive.h>
#include <libstoriqone/string.h>

static struct so_archive_file_type2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_archive_file_type type;
} so_archive_file_types[] = {
	[so_archive_file_type_block_device]     = { 0, 0, gettext_noop("block device"),     NULL, so_archive_file_type_block_device },
	[so_archive_file_type_character_device] = { 0, 0, gettext_noop("character device"), NULL, so_archive_file_type_character_device },
	[so_archive_file_type_directory]        = { 0, 0, gettext_noop("directory"),        NULL, so_archive_file_type_directory },
	[so_archive_file_type_fifo]             = { 0, 0, gettext_noop("fifo"),             NULL, so_archive_file_type_fifo },
	[so_archive_file_type_regular_file]     = { 0, 0, gettext_noop("regular file"),     NULL, so_archive_file_type_regular_file },
	[so_archive_file_type_socket]           = { 0, 0, gettext_noop("socket"),           NULL, so_archive_file_type_socket },
	[so_archive_file_type_symbolic_link]    = { 0, 0, gettext_noop("symbolic link"),    NULL, so_archive_file_type_symbolic_link },

	[so_archive_file_type_unknown] = { 0, 0, gettext_noop("unknown file type"), NULL, so_archive_file_type_unknown },
};
static const unsigned int so_archive_file_nb_data_types = sizeof(so_archive_file_types) / sizeof(*so_archive_file_types);


enum so_archive_file_type so_archive_file_string_to_type(const char * type, bool translate) {
	if (type == NULL)
		return so_archive_file_type_unknown;

	unsigned int i;
	unsigned long long hash = so_string_compute_hash2(type);

	if (translate) {
		for (i = 0; i < so_archive_file_nb_data_types; i++)
			if (hash == so_archive_file_types[i].hash_translated)
				return so_archive_file_types[i].type;
	} else {
		for (i = 0; i < so_archive_file_nb_data_types; i++)
			if (hash == so_archive_file_types[i].hash)
				return so_archive_file_types[i].type;
	}

	return so_archive_file_types[i].type;
}

const char * so_archive_file_type_to_string(enum so_archive_file_type type, bool translate) {
	if (translate)
		return so_archive_file_types[type].translation;
	else
		return so_archive_file_types[type].name;
}

