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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Thu, 07 Jul 2011 12:19:52 +0200                       *
\***********************************************************************/

#include <string.h>

#include <storiqArchiver/library.h>

static struct sa_changer_status2 {
	const char * name;
	enum sa_changer_status status;
} sa_library_status[] = {
	{ "error",		sa_changer_error },
	{ "exporting",	sa_changer_exporting },
	{ "idle",		sa_changer_idle },
	{ "importing",	sa_changer_importing },
	{ "loading",	sa_changer_loading },
	{ "unknown",	sa_changer_unknown },
	{ "unloading",	sa_changer_unloading },

	{ 0, sa_changer_unknown },
};

static struct sa_drive_status2 {
	const char * name;
	enum sa_drive_status status;
} sa_drive_status[] = {
	{ "cleaning",		sa_drive_cleaning },
	{ "empty idle",		sa_drive_emptyIdle },
	{ "erasing",		sa_drive_erasing },
	{ "error",			sa_drive_error },
	{ "loaded idle",	sa_drive_loadedIdle },
	{ "loading",		sa_drive_loading },
	{ "positioning",	sa_drive_positioning },
	{ "reading",		sa_drive_reading },
	{ "unknown",		sa_drive_unknown },
	{ "unloading",		sa_drive_unloading },
	{ "writing",		sa_drive_writing },

	{ 0, sa_drive_unknown },
};


const char * sa_changer_statusToString(enum sa_changer_status status) {
	struct sa_changer_status2 * ptr;
	for (ptr = sa_library_status; ptr->name; ptr++)
		if (ptr->status == status)
			return ptr->name;
	return 0;
}

enum sa_changer_status sa_changer_stringToStatus(const char * status) {
	struct sa_changer_status2 * ptr;
	for (ptr = sa_library_status; ptr->name; ptr++)
		if (!strcmp(ptr->name, status))
			return ptr->status;
	return ptr->status;
}

const char * sa_drive_statusToString(enum sa_drive_status status) {
	struct sa_drive_status2 * ptr;
	for (ptr = sa_drive_status; ptr->name; ptr++)
		if (ptr->status == status)
			return ptr->name;
	return 0;
}

enum sa_drive_status sa_drive_stringToStatus(const char * status) {
	struct sa_drive_status2 * ptr;
	for (ptr = sa_drive_status; ptr->name; ptr++)
		if (!strcmp(ptr->name, status))
			return ptr->status;
	return ptr->status;
}

