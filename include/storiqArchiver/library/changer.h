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
*  Last modified: Mon, 05 Dec 2011 16:40:06 +0100                         *
\*************************************************************************/

#ifndef __STORIQARCHIVER_LIBRARY_CHANGER_H__
#define __STORIQARCHIVER_LIBRARY_CHANGER_H__

struct sa_drive;
struct sa_ressource;
struct sa_slot;
struct sa_tape;

enum sa_changer_status {
	SA_CHANGER_ERROR,
	SA_CHANGER_EXPORTING,
	SA_CHANGER_IDLE,
	SA_CHANGER_IMPORTING,
	SA_CHANGER_LOADING,
	SA_CHANGER_UNKNOWN,
	SA_CHANGER_UNLOADING,
};

struct sa_changer {
	long id;
	char * device;
	enum sa_changer_status status;
	char * model;
	char * vendor;
    char * revision;
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
		int (*load)(struct sa_changer * ch, struct sa_slot * from, struct sa_drive * to);
		int (*unload)(struct sa_changer * ch, struct sa_drive * from, struct sa_slot * to);
	} * ops;
	void * data;

    struct sa_ressource * res;

    // for scsi use only
    int transport_address;
};

struct sa_slot {
	long id;
	struct sa_changer * changer;
	struct sa_drive * drive;
	struct sa_tape * tape;

	char volume_name[37];
	char full;

	struct sa_ressource * res;

	// for scsi use only
	int address;
	int src_address;
};


const char * sa_changer_status_to_string(enum sa_changer_status status);
enum sa_changer_status sa_changer_string_to_status(const char * status);
void sa_changer_setup(void);

#endif

