/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
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
*  Last modified: Mon, 21 Nov 2011 13:43:50 +0100                         *
\*************************************************************************/

#ifndef __STORIQARCHIVER_LIBRARY_H__
#define __STORIQARCHIVER_LIBRARY_H__

struct sa_drive;
struct sa_slot;
struct sa_tape;

enum sa_changer_status {
	sa_changer_error,
	sa_changer_exporting,
	sa_changer_idle,
	sa_changer_importing,
	sa_changer_loading,
	sa_changer_unknown,
	sa_changer_unloading,
};

struct sa_changer {
	long id;
	char * device;
	enum sa_changer_status status;
	char * model;
	char * vendor;
	int barcode;

	int host;
	int target;
	int channel;
	int bus;

	struct sa_drive * drives;
	unsigned int nb_drives;
	struct sa_slot * slots;
	unsigned int nb_slots;

	struct sa_changer_ops {
		int (*load)(struct sa_changer * ch);
		int (*transfer)(struct sa_changer * ch);
		int (*unload)(struct sa_changer * ch);
	} * ops;
	void * data;
};

enum sa_drive_status {
	sa_drive_cleaning,
	sa_drive_emptyIdle,
	sa_drive_erasing,
	sa_drive_error,
	sa_drive_loadedIdle,
	sa_drive_loading,
	sa_drive_positioning,
	sa_drive_reading,
	sa_drive_unknown,
	sa_drive_unloading,
	sa_drive_writing,
};

struct sa_drive {
	long id;
	char * device;
	char * scsiDevice;
	enum sa_drive_status status;
	char * model;
	char * vendor;

	int host;
	int target;
	int channel;
	int bus;

	struct sa_changer * changer;
	struct sa_slot * slot;

	struct sa_drive_ops {
		int (*eject)(struct sa_drive * drive);
		int (*rewind)(struct sa_drive * drive);
		int (*set_file_position)(struct sa_drive * drive, int file_position);
	} * ops;
	void * data;
};

struct sa_slot {
	long long id;
	struct sa_changer * changer;
	struct sa_drive * drive;
	struct sa_tape * tape;

	char volume_name[37];
	char full;
};


const char * sa_changer_statusToString(enum sa_changer_status status);
enum sa_changer_status sa_changer_stringToStatus(const char * status);
void sa_configureChanger(void);
const char * sa_drive_statusToString(enum sa_drive_status status);
enum sa_drive_status sa_drive_stringToStatus(const char * status);

#endif

