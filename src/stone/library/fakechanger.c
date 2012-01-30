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
*  Last modified: Thu, 26 Jan 2012 09:16:49 +0100                         *
\*************************************************************************/

// malloc
#include <stdlib.h>
// strdup
#include <string.h>

#include <stone/database.h>
#include <stone/log.h>
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/ressource.h>
#include <stone/library/tape.h>

#include "common.h"

struct st_fakechanger_private {
	struct st_database_connection * db_con;
};

static int st_fakechanger_can_load(void);
static struct st_slot * st_fakechanger_get_tape(struct st_changer * ch, struct st_tape * tape);
static int st_fakechanger_load(struct st_changer * ch, struct st_slot * from, struct st_drive * to);
static int st_fakechanger_sync_db(struct st_changer * ch);
static int st_fakechanger_unload(struct st_changer * ch, struct st_drive * from, struct st_slot * to);

static struct st_changer_ops st_fakechanger_ops = {
	.can_load = st_fakechanger_can_load,
	.get_tape = st_fakechanger_get_tape,
	.load     = st_fakechanger_load,
	.sync_db  = st_fakechanger_sync_db,
	.unload   = st_fakechanger_unload,
};


int st_fakechanger_can_load() {
	return 0;
}

struct st_slot * st_fakechanger_get_tape(struct st_changer * ch, struct st_tape * tape) {
	struct st_slot * sl = ch->slots;
	return sl->tape == tape ? sl : 0;
}

int st_fakechanger_load(struct st_changer * ch, struct st_slot * from, struct st_drive * to) {
	return 0;
}

void st_fakechanger_setup(struct st_changer * changer) {
	struct st_fakechanger_private * ch = malloc(sizeof(struct st_fakechanger_private));
	ch->db_con = 0;

	struct st_drive * dr = changer->drives;

	changer->id = -1;
	changer->device = "";
	changer->status = ST_CHANGER_IDLE;
	changer->model = strdup(dr->model);
	changer->vendor = strdup(dr->vendor);
	changer->revision = strdup(dr->revision);
	changer->serial_number = strdup(dr->serial_number);
	changer->barcode = 0;

	st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: starting setup", changer->vendor, changer->model);

	changer->ops = &st_fakechanger_ops;
	changer->data = ch;
	changer->lock = st_ressource_new();

	changer->slots = malloc(sizeof(struct st_slot));
	changer->nb_slots = 1;

	struct st_slot * sl = dr->slot = changer->slots = malloc(sizeof(struct st_slot));
	changer->nb_slots = 1;

	sl->id = -1;
	sl->changer = changer;
	sl->drive = dr;
	sl->tape = 0;
	*sl->volume_name = '\0';
	sl->full = 0;
	sl->lock = 0;

	st_drive_setup(dr);

	if (!dr->is_door_opened)
		dr->slot->tape = st_tape_new(dr);

	struct st_database * db = st_db_get_default_db();
	if (db) {
		ch->db_con = db->ops->connect(db, 0);
		if (ch->db_con) {
			ch->db_con->ops->sync_changer(ch->db_con, changer);
		} else {
			st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: failed to connect to default database", changer->vendor, changer->model);
		}
	} else {
		st_log_write_all(st_log_level_warning, st_log_type_changer, "[%s | %s]: there is no default database so changer is not able to synchronize with one database", changer->vendor, changer->model);
	}
}

int st_fakechanger_sync_db(struct st_changer * changer) {
	struct st_fakechanger_private * ch = changer->data;

	int failed = 0;
	if (ch->db_con)
		failed = ch->db_con->ops->sync_changer(ch->db_con, changer);

	return failed;
}

int st_fakechanger_unload(struct st_changer * changer, struct st_drive * drive, struct st_slot * to __attribute__((unused))) {
	st_log_write_all(st_log_level_warning, st_log_type_user_message, "[%s | %s | #%td]: Tape has been unloaded", drive->vendor, drive->model, drive - changer->drives);
	return 0;
}

