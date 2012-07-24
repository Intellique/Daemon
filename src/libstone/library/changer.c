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
*  Last modified: Tue, 24 Jul 2012 23:43:26 +0200                         *
\*************************************************************************/

// strcmp
#include <string.h>

#include <libstone/library/changer.h>

static const struct st_changer_status2 {
	const char * name;
	enum st_changer_status status;
} st_library_status[] = {
	{ "error",		ST_CHANGER_ERROR },
	{ "exporting",	ST_CHANGER_EXPORTING },
	{ "idle",		ST_CHANGER_IDLE },
	{ "importing",	ST_CHANGER_IMPORTING },
	{ "loading",	ST_CHANGER_LOADING },
	{ "unknown",	ST_CHANGER_UNKNOWN },
	{ "unloading",	ST_CHANGER_UNLOADING },

	{ 0, ST_CHANGER_UNKNOWN },
};


const char * st_changer_status_to_string(enum st_changer_status status) {
	const struct st_changer_status2 * ptr;
	for (ptr = st_library_status; ptr->name; ptr++)
		if (ptr->status == status)
			return ptr->name;

	return 0;
}

enum st_changer_status st_changer_string_to_status(const char * status) {
	const struct st_changer_status2 * ptr;
	for (ptr = st_library_status; ptr->name; ptr++)
		if (!strcmp(ptr->name, status))
			return ptr->status;

	return ptr->status;
}

