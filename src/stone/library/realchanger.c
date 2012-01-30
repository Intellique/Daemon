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
*  Last modified: Wed, 25 Jan 2012 22:14:00 +0100                         *
\*************************************************************************/

// open
#include <fcntl.h>
// pthread_attr_destroy, pthread_attr_init, pthread_attr_setdetachstate,
//  pthread_create, pthread_join
#include <pthread.h>
// malloc
#include <stdlib.h>
// strncpy
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// exit
#include <unistd.h>

#include <stone/database.h>
#include <stone/log.h>
#include <stone/library/ressource.h>
#include <stone/library/tape.h>

#include "common.h"
#include "scsi.h"

struct st_realchanger_private {
	int fd;
	struct st_database_connection * db_con;
};

static int st_realchanger_can_load(void);
static struct st_slot * st_realchanger_get_tape(struct st_changer * ch, struct st_tape * tape);
static int st_realchanger_load(struct st_changer * ch, struct st_slot * from, struct st_drive * to);
static void * st_realchanger_setup2(void * drive);
static int st_realchanger_sync_db(struct st_changer * ch);
static int st_realchanger_unload(struct st_changer * ch, struct st_drive * from, struct st_slot * to);
static int st_realchanger_update_status(struct st_changer * ch, enum st_changer_status status);

static struct st_changer_ops st_realchanger_ops = {
	.can_load = st_realchanger_can_load,
	.get_tape = st_realchanger_get_tape,
	.load     = st_realchanger_load,
	.sync_db  = st_realchanger_sync_db,
	.unload   = st_realchanger_unload,
};


int st_realchanger_can_load() {
	return 1;
}

struct st_slot * st_realchanger_get_tape(struct st_changer * ch, struct st_tape * tape) {
	unsigned int i;

	for (i = ch->nb_drives; i < ch->nb_slots; i++) {
		struct st_slot * sl = ch->slots + i;
		struct st_tape * tp = sl->tape;

		if (tp && tp == tape)
			return sl;
	}

	return 0;
}

int st_realchanger_load(struct st_changer * ch, struct st_slot * from, struct st_drive * to) {
	if (!ch || !from || !to)
		return 1;

	if (to->slot == from)
		return 0;

	if (from->changer != ch || to->changer != ch)
		return 1;

	st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: loading tape from slot #%td to drive #%td", ch->vendor, ch->model, from - ch->slots, to - ch->drives);

	st_realchanger_update_status(ch, ST_CHANGER_LOADING);
	struct st_realchanger_private * self = ch->data;
	int failed = st_scsi_mtx_move(self->fd, ch, from, to->slot);

	if (!failed) {
		if (from->tape) {
			from->tape->location = ST_TAPE_LOCATION_INDRIVE;
			from->tape->load_count++;
		}

		to->slot->tape = from->tape;
		from->tape = 0;
		strncpy(to->slot->volume_name, from->volume_name, 37);
		*from->volume_name = '\0';
		from->full = 0;
		to->slot->full = 1;
		to->slot->src_address = from->address;

		st_realchanger_update_status(ch, ST_CHANGER_IDLE);
	} else {
		st_realchanger_update_status(ch, ST_CHANGER_ERROR);
	}

	st_log_write_all(failed ? st_log_level_error : st_log_level_debug, st_log_type_changer, "[%s | %s]: loading tape from slot #%td to drive #%td finished with code = %d", ch->vendor, ch->model, from - ch->slots, to - ch->drives, failed);

	return failed;
}

void st_realchanger_setup(struct st_changer * changer) {
	st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: starting setup", changer->vendor, changer->model);

	int fd = open(changer->device, O_RDWR);

	st_scsi_mtx_status_new(fd, changer);

	struct st_realchanger_private * ch = malloc(sizeof(struct st_realchanger_private));
	ch->fd = fd;
	ch->db_con = 0;

	changer->status = ST_CHANGER_IDLE;
	changer->ops = &st_realchanger_ops;
	changer->data = ch;
	changer->lock = st_ressource_new();

	unsigned int i;
	for (i = 0; i < changer->nb_drives; i++) {
		struct st_drive * dr = changer->drives + i;
		dr->slot->drive = dr;
		st_drive_setup(dr);

		if (dr->is_door_opened && dr->slot->full) {
			struct st_slot * sl = 0;
			unsigned int i;
			for (i = changer->nb_drives; !sl && i < changer->nb_slots; i++) {
				struct st_slot * ptrsl = changer->slots + i;
				if (ptrsl->address == dr->slot->src_address)
					sl = ptrsl;
			}
			for (i = changer->nb_drives; !sl && i < changer->nb_slots; i++) {
				struct st_slot * ptrsl = changer->slots + i;
				if (!ptrsl->full)
					sl = ptrsl;
			}

			if (sl) {
				st_realchanger_unload(changer, dr, sl);
			} else {
				st_log_write_all(st_log_level_warning, st_log_type_changer, "[%s | %s]: your drive #%td is offline and there is no place for unloading it", changer->vendor, changer->model, changer->drives - dr);
				st_log_write_all(st_log_level_error, st_log_type_changer, "[%s | %s]: panic: your library require manual maintenance because there is no free slot for unloading drive", changer->vendor, changer->model);
				exit(1);
			}
		}
	}

	// check if there is enough free slot for unload all loaded drive
	unsigned int nb_full_drive = 0, nb_free_slot = 0;
	for (i = 0; i < changer->nb_drives; i++)
		if (changer->slots[i].full)
			nb_full_drive++;
	for (i = changer->nb_drives; i < changer->nb_slots; i++)
		if (!changer->slots[i].full)
			nb_free_slot++;
	if (nb_full_drive > nb_free_slot) {
		st_log_write_all(st_log_level_error, st_log_type_changer, "[%s | %s]: panic: your library require manual maintenance because there is not enough free slot for unloading drive", changer->vendor, changer->model);
		exit(1);
	}

	for (i = changer->nb_drives; i < changer->nb_slots; i++)
		changer->slots[i].lock = st_ressource_new();

	pthread_t * workers = 0;
	if (changer->nb_drives > 1) {
		workers = calloc(changer->nb_drives - 1, sizeof(pthread_t));

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

		for (i = 1; i < changer->nb_drives; i++)
			pthread_create(workers + (i - 1), &attr, st_realchanger_setup2, changer->drives + i);

		pthread_attr_destroy(&attr);
	}

	st_realchanger_setup2(changer->drives);

	for (i = 1; i < changer->nb_drives; i++)
		pthread_join(workers[i - 1], 0);

	if (workers)
		free(workers);

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

	st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: setup terminated", changer->vendor, changer->model);
}

void * st_realchanger_setup2(void * drive) {
	struct st_drive * dr = drive;
	struct st_changer * ch = dr->changer;

	if (!dr->is_door_opened) {
		dr->slot->tape = st_tape_new(dr);

		struct st_slot * sl = 0;
		unsigned int i;
		for (i = ch->nb_drives; !sl && i < ch->nb_slots; i++) {
			struct st_slot * ptrsl = ch->slots + i;
			if (ptrsl->lock->ops->trylock(ptrsl->lock))
				continue;
			if (ptrsl->address == dr->slot->src_address)
				sl = ptrsl;
			else
				ptrsl->lock->ops->unlock(ptrsl->lock);
		}
		for (i = ch->nb_drives; !sl && i < ch->nb_slots; i++) {
			struct st_slot * ptrsl = ch->slots + i;
			if (ptrsl->lock->ops->trylock(ptrsl->lock))
				continue;
			if (!ptrsl->full)
				sl = ptrsl;
			else
				ptrsl->lock->ops->unlock(ptrsl->lock);
		}

		if (!sl)
			return 0;

		dr->ops->eject(dr);
		ch->lock->ops->lock(ch->lock);
		st_realchanger_unload(ch, dr, sl);
		ch->lock->ops->unlock(ch->lock);
		sl->lock->ops->unlock(sl->lock);
	}

	unsigned int i;
	for (i = ch->nb_drives; i < ch->nb_slots; i++) {
		struct st_slot * sl = ch->slots + i;
		if (sl->lock->ops->trylock(sl->lock))
			continue;
		if (sl->full && !sl->tape) {
			ch->lock->ops->lock(ch->lock);
			st_realchanger_load(ch, sl, dr);
			ch->lock->ops->unlock(ch->lock);

			dr->ops->reset(dr);
			dr->slot->tape = st_tape_new(dr);
			dr->ops->eject(dr);

			ch->lock->ops->lock(ch->lock);
			st_realchanger_unload(ch, dr, sl);
			ch->lock->ops->unlock(ch->lock);
		}
		sl->lock->ops->unlock(sl->lock);
	}

	return 0;
}

int st_realchanger_sync_db(struct st_changer * ch) {
	return st_realchanger_update_status(ch, ch->status);
}

int st_realchanger_unload(struct st_changer * ch, struct st_drive * from, struct st_slot * to) {
	if (!ch || !from || !to) {
		return 1;
	}

	if (to == from->slot)
		return 0;

	if (from->changer != ch || to->changer != ch)
		return 1;

	st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: unloading tape from drive #%td to slot #%td", ch->vendor, ch->model, from - ch->drives, to - ch->slots);

	st_realchanger_update_status(ch, ST_CHANGER_UNLOADING);
	struct st_realchanger_private * self = ch->data;
	int failed = st_scsi_mtx_move(self->fd, ch, from->slot, to);

	if (!failed) {
		to->tape = from->slot->tape;
		from->slot->tape = 0;
		strncpy(to->volume_name, from->slot->volume_name, 37);
		*from->slot->volume_name = '\0';
		from->slot->full = 0;
		to->full = 1;
		from->slot->src_address = 0;

		if (to->tape)
			to->tape->location = ST_TAPE_LOCATION_ONLINE;

		st_realchanger_update_status(ch, ST_CHANGER_IDLE);
	} else {
		st_realchanger_update_status(ch, ST_CHANGER_ERROR);
	}

	st_log_write_all(failed ? st_log_level_error : st_log_level_debug, st_log_type_changer, "[%s | %s]: unloading tape from drive #%td to slot #%td finished with code = %d", ch->vendor, ch->model, from - ch->drives, to - ch->slots, failed);

	return 0;
}

int st_realchanger_update_status(struct st_changer * ch, enum st_changer_status status) {
	struct st_realchanger_private * self = ch->data;
	ch->status = status;

	int failed = 0;
	if (self->db_con)
		failed = self->db_con->ops->sync_changer(self->db_con, ch);

	return failed;
}

