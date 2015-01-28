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
*  Last modified: Sat, 13 Oct 2012 00:55:48 +0200                            *
\****************************************************************************/

// strcasecmp
#include <string.h>

#include <libstone/library/slot.h>

static const struct st_slot_type2 {
	const char * name;
	enum st_slot_type type;
} st_slot_types[] = {
	{ "drive slot",         st_slot_type_drive },
	{ "import-export slot", st_slot_type_import_export },
	{ "storage slot",       st_slot_type_storage },
	{ "transport slot",     st_slot_type_import_export },

	{ "unknown slot", st_slot_type_unkown },
};


const char * st_slot_type_to_string(enum st_slot_type type) {
	unsigned int i;
	for (i = 0; st_slot_types[i].type != st_slot_type_unkown; i++)
		if (st_slot_types[i].type == type)
			return st_slot_types[i].name;

	return st_slot_types[i].name;
}

enum st_slot_type st_slot_string_to_type(const char * type) {
	if (type == NULL)
		return st_slot_type_unkown;

	unsigned int i;
	for (i = 0; st_slot_types[i].type != st_slot_type_unkown; i++)
		if (!strcasecmp(type, st_slot_types[i].name))
			return st_slot_types[i].type;

	return st_slot_types[i].type;
}

