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
*  Last modified: Thu, 07 Jul 2011 12:51:41 +0200                       *
\***********************************************************************/

#include <storiqArchiver/library.h>

static int sa_fakechanger_load(struct sa_changer * ch);
static int sa_fakechanger_transfer(struct sa_changer * ch);
static int sa_fakechanger_unload(struct sa_changer * ch);

static int sa_fakedrive_eject(struct sa_drive * dr);
static int sa_fakedrive_rewind(struct sa_drive * drive);
static int sa_fakedrive_set_file_position(struct sa_drive * drive, int file_position);

struct sa_changer_ops sa_fakechanger_ops = {
	.load     = sa_fakechanger_load,
	.transfer = sa_fakechanger_transfer,
	.unload   = sa_fakechanger_unload,
};

struct sa_drive_ops sa_fakedrive_ops = {
	.eject             = sa_fakedrive_eject,
	.rewind            = sa_fakedrive_rewind,
	.set_file_position = sa_fakedrive_set_file_position,
};


int sa_fakechanger_load(struct sa_changer * ch) {
	return 0;
}

int sa_fakechanger_transfer(struct sa_changer * ch) {
	return 0;
}

int sa_fakechanger_unload(struct sa_changer * ch) {
	return 0;
}


int sa_fakedrive_eject(struct sa_drive * dr) {
	return 0;
}

int sa_fakedrive_rewind(struct sa_drive * drive) {
	return 0;
}

int sa_fakedrive_set_file_position(struct sa_drive * drive, int file_position) {
	return 0;
}

