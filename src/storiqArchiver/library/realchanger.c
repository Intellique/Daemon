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
*  Last modified: Sun, 27 Nov 2011 20:36:37 +0100                         *
\*************************************************************************/

// malloc
#include <stdlib.h>
// strncpy
#include <string.h>

#include <storiqArchiver/log.h>
#include <storiqArchiver/library/ressource.h>

#include "scsi.h"

struct sa_realchanger_private {
	int fd;
};

struct sa_realdrive_private {
	int fd;
};

static int sa_realchanger_load(struct sa_changer * ch, struct sa_slot * from, struct sa_drive * to);
static int sa_realchanger_unload(struct sa_changer * ch, struct sa_drive * from, struct sa_slot * to);

static int sa_realdrive_eject(struct sa_drive * dr);
static int sa_realdrive_rewind(struct sa_drive * drive);
static int sa_realdrive_set_file_position(struct sa_drive * drive, int file_position);

struct sa_changer_ops sa_realchanger_ops = {
	.load     = sa_realchanger_load,
	.unload   = sa_realchanger_unload,
};

struct sa_drive_ops sa_realdrive_ops = {
	.eject             = sa_realdrive_eject,
	.rewind            = sa_realdrive_rewind,
	.set_file_position = sa_realdrive_set_file_position,
};


int sa_realchanger_load(struct sa_changer * ch, struct sa_slot * from, struct sa_drive * to) {
	if (!ch || !from || !to) {
		return 1;
	}

	if (to->slot == from)
		return 0;

	if (from->changer != ch || to->changer != ch)
		return 1;

	struct sa_realchanger_private * self = ch->data;
	sa_scsi_mtx_move(self->fd, ch, from, to->slot);

	strncpy(to->slot->volume_name, from->volume_name, 37);
	*from->volume_name = '\0';
	from->full = 0;
	to->slot->full = 1;

	sa_log_write_all(sa_log_level_info, "Loading (changer: %s:%s) from slot %ld to drive %ld", ch->vendor, ch->model, from - ch->slots, to - ch->drives);

	return 0;
}

void sa_realchanger_setup(struct sa_changer * changer, int fd) {
	struct sa_realchanger_private * ch = malloc(sizeof(struct sa_realchanger_private));
	ch->fd = fd;

	changer->ops = &sa_realchanger_ops;
	changer->data = ch;
	changer->res = sa_ressource_new();

	unsigned int i;
	for (i = 0; i < changer->nb_drives; i++)
		changer->drives[i].ops = &sa_realdrive_ops;
}

int sa_realchanger_unload(struct sa_changer * ch, struct sa_drive * from, struct sa_slot * to) {
	if (!ch || !from || !to) {
		return 1;
	}

	if (to == from->slot)
		return 0;

	if (from->changer != ch || to->changer != ch)
		return 1;

	struct sa_realchanger_private * self = ch->data;
	sa_scsi_mtx_move(self->fd, ch, from->slot, to);

	strncpy(to->volume_name, from->slot->volume_name, 37);
	*from->slot->volume_name = '\0';
	from->slot->full = 0;
	to->full = 1;

	sa_log_write_all(sa_log_level_info, "Unloading (changer: %s:%s) from drive %ld to slot %ld", ch->vendor, ch->model, from - ch->drives, to - ch->slots);

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

