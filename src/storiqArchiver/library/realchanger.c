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
*  Last modified: Mon, 21 Nov 2011 13:45:41 +0100                         *
\*************************************************************************/

#include "scsi.h"

static int sa_realchanger_load(struct sa_changer * ch);
static int sa_realchanger_transfer(struct sa_changer * ch);
static int sa_realchanger_unload(struct sa_changer * ch);

static int sa_realdrive_eject(struct sa_drive * dr);
static int sa_realdrive_rewind(struct sa_drive * drive);
static int sa_realdrive_set_file_position(struct sa_drive * drive, int file_position);

struct sa_changer_ops sa_realchanger_ops = {
	.load     = sa_realchanger_load,
	.transfer = sa_realchanger_transfer,
	.unload   = sa_realchanger_unload,
};

struct sa_drive_ops sa_realdrive_ops = {
	.eject             = sa_realdrive_eject,
	.rewind            = sa_realdrive_rewind,
	.set_file_position = sa_realdrive_set_file_position,
};


int sa_realchanger_load(struct sa_changer * ch) {
	return 0;
}

void sa_realchanger_setup(struct sa_changer * changer) {
	changer->ops = &sa_realchanger_ops;

	unsigned int i;
	for (i = 0; i < changer->nb_drives; i++)
		changer->drives[i].ops = &sa_realdrive_ops;
}

int sa_realchanger_transfer(struct sa_changer * ch) {
	return 0;
}

int sa_realchanger_unload(struct sa_changer * ch) {
	return 0;
}


int sa_realdrive_eject(struct sa_drive * dr) {
	return 0;
}

int sa_realdrive_rewind(struct sa_drive * drive) {
	return 0;
}

int sa_realdrive_set_file_position(struct sa_drive * drive, int file_position) {
	return 0;
}

