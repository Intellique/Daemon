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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Thu, 16 Aug 2012 22:26:17 +0200                         *
\*************************************************************************/

// strcasecmp
#include <string.h>

#include <libstone/library/changer.h>

static const struct st_changer_status2 {
	const char * name;
	enum st_changer_status status;
} st_library_status[] = {
	{ "error",		st_changer_error },
	{ "exporting",	st_changer_exporting },
	{ "idle",		st_changer_idle },
	{ "importing",	st_changer_importing },
	{ "loading",	st_changer_loading },
	{ "unloading",	st_changer_unloading },

	{ "unknown", st_changer_unknown },
};


const char * st_changer_status_to_string(enum st_changer_status status) {
	unsigned int i;
	for (i = 0; st_library_status[i].status != st_changer_unknown; i++)
		if (st_library_status[i].status == status)
			return st_library_status[i].name;

	return st_library_status[i].name;
}

enum st_changer_status st_changer_string_to_status(const char * status) {
	if (status == NULL)
		return st_changer_unknown;

	unsigned int i;
	for (i = 0; st_library_status[i].status != st_changer_unknown; i++)
		if (!strcasecmp(status, st_library_status[i].name))
			return st_library_status[i].status;

	return st_library_status[i].status;
}

