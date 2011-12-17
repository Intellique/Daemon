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
*  Last modified: Sat, 17 Dec 2011 19:20:17 +0100                         *
\*************************************************************************/

// malloc
#include <stdlib.h>

#include <stone/library/changer.h>
#include <stone/library/ressource.h>

#include "common.h"

struct st_fakechanger_private {
	struct st_database_connection * db_con;
};

static int st_fakechanger_can_load(void);
static int st_fakechanger_load(struct st_changer * ch, struct st_slot * from, struct st_drive * to);
static int st_fakechanger_unload(struct st_changer * ch, struct st_drive * from, struct st_slot * to);

static struct st_changer_ops st_fakechanger_ops = {
	.can_load = st_fakechanger_can_load,
	.load     = st_fakechanger_load,
	.unload   = st_fakechanger_unload,
};


int st_fakechanger_can_load() {
	return 0;
}

int st_fakechanger_load(struct st_changer * ch, struct st_slot * from, struct st_drive * to) {
	return 0;
}

void st_fakechanger_setup(struct st_changer * changer) {
	struct st_fakechanger_private * ch = malloc(sizeof(struct st_fakechanger_private));
	ch->db_con = 0;

	changer->status = ST_CHANGER_IDLE;
	changer->ops = &st_fakechanger_ops;
	changer->data = ch;
	changer->lock = st_ressource_new();

	changer->slots = malloc(sizeof(struct st_slot));
	changer->nb_slots = 1;

	struct st_slot * sl = changer->slots = malloc(sizeof(struct st_slot));
	changer->nb_slots = 1;

	sl->id = -1;
	sl->changer = changer;
	sl->drive = changer->drives;
	sl->tape = 0;
	*sl->volume_name = '\0';
	sl->full = 0;
	sl->lock = 0;

	st_drive_setup(changer->drives);
}

int st_fakechanger_unload(struct st_changer * ch, struct st_drive * from, struct st_slot * to) {
	return 0;
}

