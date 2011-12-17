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
*  Last modified: Sat, 17 Dec 2011 17:32:41 +0100                         *
\*************************************************************************/

#include <string.h>

#include <stone/library/changer.h>
#include <stone/library/drive.h>

static struct sa_changer_status2 {
	const char * name;
	enum sa_changer_status status;
} sa_library_status[] = {
	{ "error",		SA_CHANGER_ERROR },
	{ "exporting",	SA_CHANGER_EXPORTING },
	{ "idle",		SA_CHANGER_IDLE },
	{ "importing",	SA_CHANGER_IMPORTING },
	{ "loading",	SA_CHANGER_LOADING },
	{ "unknown",	SA_CHANGER_UNKNOWN },
	{ "unloading",	SA_CHANGER_UNLOADING },

	{ 0, SA_CHANGER_UNKNOWN },
};

static struct sa_drive_status2 {
	const char * name;
	enum sa_drive_status status;
} sa_drive_status[] = {
	{ "cleaning",		SA_DRIVE_CLEANING },
	{ "empty idle",		SA_DRIVE_EMPTY_IDLE },
	{ "erasing",		SA_DRIVE_ERASING },
	{ "error",			SA_DRIVE_ERROR },
	{ "loaded idle",	SA_DRIVE_LOADED_IDLE },
	{ "loading",		SA_DRIVE_LOADING },
	{ "positioning",	SA_DRIVE_POSITIONING },
	{ "reading",		SA_DRIVE_READING },
	{ "unknown",		SA_DRIVE_UNKNOWN },
	{ "unloading",		SA_DRIVE_UNLOADING },
	{ "writing",		SA_DRIVE_WRITING },

	{ 0, SA_DRIVE_UNKNOWN },
};


const char * sa_changer_status_to_string(enum sa_changer_status status) {
	struct sa_changer_status2 * ptr;
	for (ptr = sa_library_status; ptr->name; ptr++)
		if (ptr->status == status)
			return ptr->name;
	return 0;
}

enum sa_changer_status sa_changer_string_to_status(const char * status) {
	struct sa_changer_status2 * ptr;
	for (ptr = sa_library_status; ptr->name; ptr++)
		if (!strcmp(ptr->name, status))
			return ptr->status;
	return ptr->status;
}

const char * sa_drive_status_to_string(enum sa_drive_status status) {
	struct sa_drive_status2 * ptr;
	for (ptr = sa_drive_status; ptr->name; ptr++)
		if (ptr->status == status)
			return ptr->name;
	return 0;
}

enum sa_drive_status sa_drive_string_to_status(const char * status) {
	struct sa_drive_status2 * ptr;
	for (ptr = sa_drive_status; ptr->name; ptr++)
		if (!strcmp(ptr->name, status))
			return ptr->status;
	return ptr->status;
}

