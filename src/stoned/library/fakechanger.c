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
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 10 Sep 2012 18:52:58 +0200                         *
\*************************************************************************/

// open
#include <fcntl.h>
// malloc
#include <stdlib.h>
// strdup
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// close
#include <unistd.h>

#include <stone/log.h>
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/ressource.h>
#include <stone/library/tape.h>

#include "common.h"
#include "scsi.h"

static bool st_fakechanger_can_load(void);
static struct st_slot * st_fakechanger_get_tape(struct st_changer * ch, struct st_tape * tape);
static int st_fakechanger_load(struct st_changer * ch, struct st_slot * from, struct st_drive * to);
static int st_fakechanger_unload(struct st_changer * ch, struct st_drive * from);
static int st_fakechanger_update_status(struct st_changer * ch);

static struct st_changer_ops st_fakechanger_ops = {
	.can_load      = st_fakechanger_can_load,
	.get_tape      = st_fakechanger_get_tape,
	.load          = st_fakechanger_load,
	.unload        = st_fakechanger_unload,
	.update_status = st_fakechanger_update_status,
};


bool st_fakechanger_can_load(void) {
	return false;
}

struct st_slot * st_fakechanger_get_tape(struct st_changer * ch, struct st_tape * tape) {
	struct st_slot * sl = ch->slots;
	return sl->tape == tape ? sl : 0;
}

int st_fakechanger_load(struct st_changer * ch, struct st_slot * from, struct st_drive * to) {
	return 0;
}

void st_fakechanger_setup(struct st_changer * changer) {
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
	changer->data = NULL;
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
	sl->full = false;
	sl->lock = NULL;

	st_drive_setup(dr);

	if (!dr->is_door_opened) {
		sl->full = true;

		char serial_number[32];
		int fd = open(dr->scsi_device, O_RDWR);

		if (!st_scsi_tape_read_medium_serial_number(fd, serial_number))
			dr->slot->tape = st_tape_get_by_medium_serial_number(serial_number);

		close(fd);

		if (dr->slot->tape == NULL)
			dr->slot->tape = st_tape_new(dr);

		if (dr->slot->tape->format->support_mam)
			dr->ops->read_mam(dr);
	}
}

int st_fakechanger_unload(struct st_changer * changer, struct st_drive * drive) {
	if (drive->slot->tape != NULL)
		drive->ops->eject(drive);
	drive->slot->tape = NULL;

	st_log_write_all(st_log_level_warning, st_log_type_user_message, "[%s | %s | #%td]: Tape has been unloaded", drive->vendor, drive->model, drive - changer->drives);
	return 0;
}

int st_fakechanger_update_status(struct st_changer * ch) {
	struct st_drive * dr = ch->drives;

	if (dr->lock->ops->trylock(dr->lock))
		return 1;

	if (dr->slot->tape == NULL) {
		dr->ops->reset(dr, true, false);

		if (!dr->is_door_opened && dr->slot->tape == NULL) {
			char serial_number[32];
			int fd = open(dr->scsi_device, O_RDWR);

			if (!st_scsi_tape_read_medium_serial_number(fd, serial_number))
				dr->slot->tape = st_tape_get_by_medium_serial_number(serial_number);

			close(fd);

			if (dr->slot->tape == NULL)
				dr->slot->tape = st_tape_new(dr);
			else
				dr->slot->tape->location = ST_TAPE_LOCATION_INDRIVE;
		}
	} else {
		dr->ops->reset(dr, true, true);

		if (dr->is_door_opened && dr->slot->tape != NULL) {
			dr->slot->tape->location = ST_TAPE_LOCATION_OFFLINE;
			dr->slot->tape = NULL;
		}
	}

	dr->lock->ops->unlock(dr->lock);

	return 0;
}

