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
*  Last modified: Tue, 21 Aug 2012 09:48:28 +0200                         *
\*************************************************************************/

// malloc
#include <stdlib.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>

#include <libstone/log.h>
#include <libstone/library/ressource.h>

#include "common.h"
#include "scsi.h"

static struct st_drive * st_standalone_drive_find_free_drive(struct st_changer * ch);
static int st_standalone_drive_load_media(struct st_changer * ch, struct st_media * from, struct st_drive * to);
static int st_standalone_drive_load_slot(struct st_changer * ch, struct st_slot * from, struct st_drive * to);
static int st_standalone_drive_unload(struct st_changer * ch, struct st_drive * from);

static struct st_changer_ops st_standalone_drive_ops = {
	.find_free_drive = st_standalone_drive_find_free_drive,
	.load_media      = st_standalone_drive_load_media,
	.load_slot       = st_standalone_drive_load_slot,
	.unload          = st_standalone_drive_unload,
};


static struct st_drive * st_standalone_drive_find_free_drive(struct st_changer * ch) {
	if (ch == NULL || !ch->enabled)
		return NULL;

	struct st_drive * dr = ch->drives;
	if (!dr->enabled)
		return NULL;

	if (!dr->lock->ops->trylock(dr->lock))
		return dr;

	return NULL;
}

static int st_standalone_drive_load_media(struct st_changer * ch __attribute__((unused)), struct st_media * from __attribute__((unused)), struct st_drive * to __attribute__((unused))) {
	return 1;
}

static int st_standalone_drive_load_slot(struct st_changer * ch __attribute__((unused)), struct st_slot * from __attribute__((unused)), struct st_drive * to __attribute__((unused))) {
	return 1;
}

void st_standalone_drive_setup(struct st_changer * changer) {
	st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: starting setup", changer->vendor, changer->model);

	changer->device = strdup("");
	changer->status = st_changer_idle;
	changer->ops = &st_standalone_drive_ops;
	changer->data = NULL;

	struct st_slot * slot = changer->slots = malloc(sizeof(struct st_slot));
	bzero(slot, sizeof(struct st_slot));
	changer->nb_slots = 1;

	slot->changer = changer;
	struct st_drive * drive = slot->drive = changer->drives;
	slot->media = NULL;
	drive->slot = slot;

	slot->volume_name = NULL;
	slot->full = 0;
	slot->type = st_slot_type_drive;

	st_scsi_tape_drive_setup(changer->drives);

	changer->model = strdup(drive->model);
	changer->vendor = strdup(drive->vendor);
	changer->revision = strdup(drive->revision);
	changer->serial_number = strdup(drive->serial_number);
	changer->barcode = 0;

	drive->ops->update_media_info(drive);

	st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: setup finish", changer->vendor, changer->model);
}

static int st_standalone_drive_unload(struct st_changer * ch, struct st_drive * from) {
	if (ch == NULL || from == NULL || !ch->enabled)
		return 1;

	from->ops->update_media_info(from);
	if (!from->is_empty)
		from->ops->eject(from);

	return 0;
}

