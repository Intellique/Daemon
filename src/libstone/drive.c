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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// strcasecmp
#include <string.h>

#include "drive.h"

static const struct st_drive_status2 {
	const char * name;
	enum st_drive_status status;
} st_drive_status[] = {
	{ "cleaning",		st_drive_cleaning },
	{ "empty idle",		st_drive_empty_idle },
	{ "erasing",		st_drive_erasing },
	{ "error",			st_drive_error },
	{ "loaded idle",	st_drive_loaded_idle },
	{ "loading",		st_drive_loading },
	{ "positioning",	st_drive_positioning },
	{ "reading",		st_drive_reading },
	{ "rewinding",		st_drive_rewinding },
	{ "unloading",		st_drive_unloading },
	{ "writing",		st_drive_writing },

	{ "unknown", st_drive_unknown },
};


__asm__(".symver st_drive_status_to_string_v1, st_drive_status_to_string@@LIBSTONE_1.2");
const char * st_drive_status_to_string_v1(enum st_drive_status status) {
	unsigned int i;
	for (i = 0; st_drive_status[i].status != st_drive_unknown; i++)
		if (st_drive_status[i].status == status)
			return st_drive_status[i].name;

	return st_drive_status[i].name;
}

__asm__(".symver st_drive_string_to_status_v1, st_drive_string_to_status@@LIBSTONE_1.2");
enum st_drive_status st_drive_string_to_status_v1(const char * status) {
	if (status == NULL)
		return st_drive_unknown;

	unsigned int i;
	for (i = 0; st_drive_status[i].status != st_drive_unknown; i++)
		if (!strcasecmp(status, st_drive_status[i].name))
			return st_drive_status[i].status;

	return st_drive_status[i].status;
}

