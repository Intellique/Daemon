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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Wed, 28 Dec 2011 11:57:13 +0100                         *
\*************************************************************************/

#include <string.h>

#include <stone/library/changer.h>
#include <stone/library/drive.h>

static struct st_changer_status2 {
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

static struct st_drive_status2 {
	const char * name;
	enum st_drive_status status;
} st_drive_status[] = {
	{ "cleaning",		ST_DRIVE_CLEANING },
	{ "empty idle",		ST_DRIVE_EMPTY_IDLE },
	{ "erasing",		ST_DRIVE_ERASING },
	{ "error",			ST_DRIVE_ERROR },
	{ "loaded idle",	ST_DRIVE_LOADED_IDLE },
	{ "loading",		ST_DRIVE_LOADING },
	{ "positioning",	ST_DRIVE_POSITIONING },
	{ "reading",		ST_DRIVE_READING },
	{ "rewinding",		ST_DRIVE_REWINDING },
	{ "unknown",		ST_DRIVE_UNKNOWN },
	{ "unloading",		ST_DRIVE_UNLOADING },
	{ "writing",		ST_DRIVE_WRITING },

	{ 0, ST_DRIVE_UNKNOWN },
};


const char * st_changer_status_to_string(enum st_changer_status status) {
	struct st_changer_status2 * ptr;
	for (ptr = st_library_status; ptr->name; ptr++)
		if (ptr->status == status)
			return ptr->name;
	return 0;
}

enum st_changer_status st_changer_string_to_status(const char * status) {
	struct st_changer_status2 * ptr;
	for (ptr = st_library_status; ptr->name; ptr++)
		if (!strcmp(ptr->name, status))
			return ptr->status;
	return ptr->status;
}

const char * st_drive_status_to_string(enum st_drive_status status) {
	struct st_drive_status2 * ptr;
	for (ptr = st_drive_status; ptr->name; ptr++)
		if (ptr->status == status)
			return ptr->name;
	return 0;
}

enum st_drive_status st_drive_string_to_status(const char * status) {
	struct st_drive_status2 * ptr;
	for (ptr = st_drive_status; ptr->name; ptr++)
		if (!strcmp(ptr->name, status))
			return ptr->status;
	return ptr->status;
}

