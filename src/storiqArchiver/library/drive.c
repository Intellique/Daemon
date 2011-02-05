/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2010, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Tue, 26 Oct 2010 17:46:18 +0200                       *
\***********************************************************************/

// strcasecmp
#include <string.h>

#include <storiqArchiver/library.h>

static struct drive_status {
	enum library_drive_status status;
	char * name;
} library_drive_status[] = {
	{ library_drive_cleaning,    "cleaning" },
	{ library_drive_emptyIdle,   "empty-idle" },
	{ library_drive_erasing,     "erasing" },
	{ library_drive_error,       "error" },
	{ library_drive_loadedIdle,  "loaded-idle" },
	{ library_drive_loading,     "loading" },
	{ library_drive_positioning, "positioning" },
	{ library_drive_reading,     "reading" },
	{ library_drive_unloading,   "unloading" },
	{ library_drive_writing,     "writing" },

	{ library_drive_unknown, 0 },
};


const char * library_drive_statusToString(enum library_drive_status status) {
	struct drive_status * ptr;
	for (ptr = library_drive_status; ptr->name; ptr++)
		if (ptr->status == status)
			return ptr->name;
	return 0;
}

enum library_drive_status library_drive_stringToStatus(const char * status) {
	struct drive_status * ptr;
	for (ptr = library_drive_status; ptr->name; ptr++)
		if (!strcasecmp(ptr->name, status))
			return ptr->status;
	return 0;
}

