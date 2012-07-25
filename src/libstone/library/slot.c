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
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Wed, 25 Jul 2012 11:33:16 +0200                         *
\*************************************************************************/

// strcmp
#include <string.h>

#include <libstone/library/slot.h>

static const struct st_slot_type2 {
	const char * name;
	enum st_slot_type type;
} st_slot_types[] = {
	{ "drive slot",         st_slot_type_drive },
	{ "import-export slot", st_slot_type_import_export },
	{ "storage slot",       st_slot_type_storage },
	{ "storage slot",       st_slot_type_storage },
	{ "transport slot",     st_slot_type_import_export },
	{ "unknown slot",       st_slot_type_unkown },

	{ 0, st_slot_type_unkown },
};


const char * st_slot_type_to_string(enum st_slot_type type) {
	const struct st_slot_type2 * ptr;
	for (ptr = st_slot_types; ptr->name; ptr++)
		if (ptr->type == type)
			return ptr->name;

	return "unknown slot";
}

enum st_slot_type st_slot_string_to_type(const char * type) {
	if (!type)
		return st_slot_type_unkown;

	const struct st_slot_type2 * ptr;
	for (ptr = st_slot_types; ptr->name; ptr++)
		if (!strcmp(type, ptr->name))
			return ptr->type;

	return st_slot_type_unkown;
}

