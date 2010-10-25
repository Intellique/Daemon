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
*  Last modified: Mon, 25 Oct 2010 16:32:44 +0200                       *
\***********************************************************************/

#ifndef __STORIQARCHIVER_LIBRARY_CHANGER_H__
#define __STORIQARCHIVER_LIBRARY_CHANGER_H__

struct library_drive;

enum library_changer_status {
	library_changer_error,
	library_changer_exporting,
	library_changer_idle,
	library_changer_importing,
	library_changer_loading,
	library_changer_unknown,
	library_changer_unloading,
};

struct library_changer {
	long id;
	char * model;
	char * device;
	enum library_changer_status status;
	char * vendor;
	char * firmwareRev;

	struct library_drive * drives;
	unsigned int nbDrives;
};

enum library_drive_status {
	library_drive_cleaning,
	library_drive_emptyIdle,
	library_drive_erasing,
	library_drive_error,
	library_drive_loadedIdle,
	library_drive_loading,
	library_drive_positioning,
	library_drive_reading,
	library_drive_unloading,
	library_drive_writing,
};

struct library_drive {
	long id;
	char * name;
	char * device;
	enum library_drive_status status;
	struct library_changer * changer;
	char * vendor;
	char * model;
};


const char * library_changer_statusToString(enum library_changer_status status);
enum library_changer_status library_changer_stringToStatus(const char * status);
void library_configureChanger(void);

#endif

