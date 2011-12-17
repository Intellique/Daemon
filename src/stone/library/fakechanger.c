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
*  Last modified: Sat, 17 Dec 2011 17:32:29 +0100                         *
\*************************************************************************/

// malloc
#include <stdlib.h>

#include <stone/library/changer.h>
#include <stone/library/ressource.h>

#include "common.h"

struct sa_fakechanger_private {
	struct sa_database_connection * db_con;
};

static int sa_fakechanger_can_load(void);
static int sa_fakechanger_load(struct sa_changer * ch, struct sa_slot * from, struct sa_drive * to);
static int sa_fakechanger_unload(struct sa_changer * ch, struct sa_drive * from, struct sa_slot * to);

static struct sa_changer_ops sa_fakechanger_ops = {
	.can_load = sa_fakechanger_can_load,
	.load     = sa_fakechanger_load,
	.unload   = sa_fakechanger_unload,
};


int sa_fakechanger_can_load() {
	return 0;
}

int sa_fakechanger_load(struct sa_changer * ch, struct sa_slot * from, struct sa_drive * to) {
	return 0;
}

void sa_fakechanger_setup(struct sa_changer * changer) {
	struct sa_fakechanger_private * ch = malloc(sizeof(struct sa_fakechanger_private));
	ch->db_con = 0;

	changer->status = SA_CHANGER_IDLE;
	changer->ops = &sa_fakechanger_ops;
	changer->data = ch;
	changer->lock = sa_ressource_new();

	changer->slots = malloc(sizeof(struct sa_slot));
	changer->nb_slots = 1;

	struct sa_slot * sl = changer->slots = malloc(sizeof(struct sa_slot));
	changer->nb_slots = 1;

	sl->id = -1;
	sl->changer = changer;
	sl->drive = changer->drives;
	sl->tape = 0;
	*sl->volume_name = '\0';
	sl->full = 0;
	sl->lock = 0;

	sa_drive_setup(changer->drives);
}

int sa_fakechanger_unload(struct sa_changer * ch, struct sa_drive * from, struct sa_slot * to) {
	return 0;
}

