/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
*  it under the terms of the GNU Affero General Public License               *
*  as published by the Free Software Foundation; either version 3            *
*  of the License, or (at your option) any later version.                    *
*                                                                            *
*  This program is distributed in the hope that it will be useful,           *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
*  GNU Affero General Public License for more details.                       *
*                                                                            *
*  You should have received a copy of the GNU Affero General Public License  *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Wed, 30 Jan 2013 10:50:35 +0100                            *
\****************************************************************************/

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

static struct st_drive * st_standalone_drive_find_free_drive(struct st_changer * ch, struct st_media_format * format, bool for_reading, bool for_writing);
static void st_standalone_drive_free(struct st_changer * ch);
static int st_standalone_drive_load_media(struct st_changer * ch, struct st_media * from, struct st_drive * to);
static int st_standalone_drive_load_slot(struct st_changer * ch, struct st_slot * from, struct st_drive * to);
static int st_standalone_drive_shut_down(struct st_changer * ch);
static int st_standalone_drive_unload(struct st_changer * ch, struct st_drive * from);

static struct st_changer_ops st_standalone_drive_ops = {
	.find_free_drive = st_standalone_drive_find_free_drive,
	.free            = st_standalone_drive_free,
	.load_media      = st_standalone_drive_load_media,
	.load_slot       = st_standalone_drive_load_slot,
	.shut_down       = st_standalone_drive_shut_down,
	.unload          = st_standalone_drive_unload,
};


static struct st_drive * st_standalone_drive_find_free_drive(struct st_changer * ch, struct st_media_format * format, bool for_reading, bool for_writing) {
	if (ch == NULL || !ch->enabled)
		return NULL;

	struct st_drive * dr = ch->drives;
	if (!dr->enabled)
		return NULL;

	if (!dr->lock->ops->try_lock(dr->lock))
		return dr;

	return NULL;
}

static void st_standalone_drive_free(struct st_changer * ch) {
	free(ch->device);
	free(ch->model);
	free(ch->vendor);
	free(ch->revision);
	free(ch->serial_number);

	ch->drives->ops->free(ch->drives);
	free(ch->drives);
	ch->drives = NULL;
	ch->nb_drives = 0;

	free(ch->slots->db_data);
	free(ch->slots);
	ch->slots = NULL;
}

static int st_standalone_drive_load_media(struct st_changer * ch __attribute__((unused)), struct st_media * from __attribute__((unused)), struct st_drive * to __attribute__((unused))) {
	return 1;
}

static int st_standalone_drive_load_slot(struct st_changer * ch __attribute__((unused)), struct st_slot * from __attribute__((unused)), struct st_drive * to __attribute__((unused))) {
	return 1;
}

void st_standalone_drive_setup(struct st_changer * changer) {
	st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: starting setup", changer->vendor, changer->model);

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
	slot->full = false;
	slot->type = st_slot_type_drive;

	st_scsi_tape_drive_setup(changer->drives);

	drive->ops->update_media_info(drive);

	st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: setup finish", changer->vendor, changer->model);
}

static int st_standalone_drive_shut_down(struct st_changer * ch __attribute__((unused))) {
	return 0;
}

static int st_standalone_drive_unload(struct st_changer * ch, struct st_drive * from) {
	if (ch == NULL || from == NULL || !ch->enabled)
		return 1;

	from->ops->update_media_info(from);
	if (!from->is_empty)
		from->ops->eject(from);

	return 0;
}

