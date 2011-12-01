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
*  Last modified: Thu, 01 Dec 2011 16:59:35 +0100                         *
\*************************************************************************/

// malloc
#include <stdlib.h>
// strncpy
#include <string.h>

#include <storiqArchiver/log.h>
#include <storiqArchiver/library/ressource.h>
#include <storiqArchiver/library/tape.h>

#include "common.h"
#include "scsi.h"

struct sa_realchanger_private {
	int fd;
};

static int sa_realchanger_load(struct sa_changer * ch, struct sa_slot * from, struct sa_drive * to);
static int sa_realchanger_unload(struct sa_changer * ch, struct sa_drive * from, struct sa_slot * to);

struct sa_changer_ops sa_realchanger_ops = {
	.load     = sa_realchanger_load,
	.unload   = sa_realchanger_unload,
};


int sa_realchanger_load(struct sa_changer * ch, struct sa_slot * from, struct sa_drive * to) {
	if (!ch || !from || !to) {
		return 1;
	}

	if (to->slot == from)
		return 0;

	if (from->changer != ch || to->changer != ch)
		return 1;

	sa_log_write_all(sa_log_level_debug, "Library: start loading (changer: %s:%s) from slot %ld to drive %ld", ch->vendor, ch->model, from - ch->slots, to - ch->drives);

	struct sa_realchanger_private * self = ch->data;
	int failed = sa_scsi_mtx_move(self->fd, ch, from, to->slot);

	if (!failed) {
		to->slot->tape = from->tape;
		from->tape = 0;
		strncpy(to->slot->volume_name, from->volume_name, 37);
		*from->volume_name = '\0';
		from->full = 0;
		to->slot->full = 1;
		to->slot->src_address = from->address;
	}

	sa_log_write_all(failed ? sa_log_level_error : sa_log_level_info, "Library: loading (changer: %s:%s) from slot %ld to drive %ld finished with code=%d", ch->vendor, ch->model, from - ch->slots, to - ch->drives, failed);

	return failed;
}

void sa_realchanger_setup(struct sa_changer * changer, int fd) {
	sa_log_write_all(sa_log_level_info, "Library: starting setup of library (%s:%s)", changer->vendor, changer->model);

	struct sa_realchanger_private * ch = malloc(sizeof(struct sa_realchanger_private));
	ch->fd = fd;

	changer->ops = &sa_realchanger_ops;
	changer->data = ch;
	changer->res = sa_ressource_new();

	unsigned int i;
	for (i = 0; i < changer->nb_drives; i++)
		sa_drive_setup(changer->drives + i);

	for (i = 0; i < changer->nb_drives; i++) {
		struct sa_drive * dr = changer->drives + i;
		if (!dr->is_door_opened) {
			dr->slot->tape = sa_tape_new(dr);
			sa_tape_detect(dr);
		}
	}

	struct sa_drive * dr = changer->drives;
	if (!dr->is_door_opened) {
		struct sa_slot * sl = 0;
		unsigned int i;
		for (i = changer->nb_drives; !sl && i < changer->nb_slots; i++)
			if (changer->slots[i].address == dr->slot->src_address)
				sl = changer->slots + i;
		for (i = changer->nb_drives; !sl && i < changer->nb_slots; i++)
			if (!changer->slots[i].full)
				sl = changer->slots + i;
		if (sl) {
			dr->ops->rewind(dr);
			sa_realchanger_unload(changer, dr, sl);
		}
	}

	for (i = changer->nb_drives; i < changer->nb_slots; i++) {
		struct sa_slot * sl = changer->slots + i;
		if (sl->full && !sl->tape) {
			sa_realchanger_load(changer, sl, dr);

			dr->slot->tape = sa_tape_new(dr);
			sa_tape_detect(dr);

			dr->ops->rewind(dr);
			sa_realchanger_unload(changer, dr, sl);
		}
	}

	sa_log_write_all(sa_log_level_info, "Library: setup terminated");
}

int sa_realchanger_unload(struct sa_changer * ch, struct sa_drive * from, struct sa_slot * to) {
	if (!ch || !from || !to) {
		return 1;
	}

	if (to == from->slot)
		return 0;

	if (from->changer != ch || to->changer != ch)
		return 1;

	sa_log_write_all(sa_log_level_debug, "Library: start unloading (changer: %s:%s) from drive %ld to slot %ld", ch->vendor, ch->model, from - ch->drives, to - ch->slots);

	struct sa_realchanger_private * self = ch->data;
	int failed = sa_scsi_mtx_move(self->fd, ch, from->slot, to);

	if (!failed) {
		to->tape = from->slot->tape;
		from->slot->tape = 0;
		strncpy(to->volume_name, from->slot->volume_name, 37);
		*from->slot->volume_name = '\0';
		from->slot->full = 0;
		to->full = 1;
		from->slot->src_address = 0;
	}

	sa_log_write_all(failed ? sa_log_level_error : sa_log_level_info, "Library: unloading (changer: %s:%s) from drive %ld to slot %ld finished with code=%d", ch->vendor, ch->model, from - ch->drives, to - ch->slots, failed);

	return 0;
}

