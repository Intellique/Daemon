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
*  Last modified: Wed, 07 Dec 2011 13:54:31 +0100                         *
\*************************************************************************/

// pthread_attr_destroy, pthread_attr_init, pthread_attr_setdetachstate,
//  pthread_create, pthread_join
#include <pthread.h>
// malloc
#include <stdlib.h>
// strncpy
#include <string.h>
// exit
#include <unistd.h>

#include <storiqArchiver/database.h>
#include <storiqArchiver/log.h>
#include <storiqArchiver/library/ressource.h>
#include <storiqArchiver/library/tape.h>

#include "common.h"
#include "scsi.h"

struct sa_realchanger_private {
	int fd;
	struct sa_database_connection * db_con;
};

static int sa_realchanger_load(struct sa_changer * ch, struct sa_slot * from, struct sa_drive * to);
static void * sa_realchanger_setup2(void * drive);
static int sa_realchanger_unload(struct sa_changer * ch, struct sa_drive * from, struct sa_slot * to);
static void sa_realchanger_update_status(struct sa_changer * ch, enum sa_changer_status status);

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

	sa_log_write_all(sa_log_level_info, "Library (%s | %s): loading tape from slot #%ld to drive #%ld", ch->vendor, ch->model, from - ch->slots, to - ch->drives);

	sa_realchanger_update_status(ch, SA_CHANGER_LOADING);
	struct sa_realchanger_private * self = ch->data;
	int failed = sa_scsi_mtx_move(self->fd, ch, from, to->slot);

	if (!failed) {
		if (from->tape)
			from->tape->load_count++;

		to->slot->tape = from->tape;
		from->tape = 0;
		strncpy(to->slot->volume_name, from->volume_name, 37);
		*from->volume_name = '\0';
		from->full = 0;
		to->slot->full = 1;
		to->slot->src_address = from->address;

		sa_realchanger_update_status(ch, SA_CHANGER_IDLE);
	} else {
		sa_realchanger_update_status(ch, SA_CHANGER_ERROR);
	}

	sa_log_write_all(failed ? sa_log_level_error : sa_log_level_debug, "Library (%s | %s): loading tape from slot #%ld to drive #%ld finished with code = %d", ch->vendor, ch->model, from - ch->slots, to - ch->drives, failed);

	return failed;
}

void sa_realchanger_setup(struct sa_changer * changer, int fd) {
	sa_log_write_all(sa_log_level_info, "Library (%s | %s): starting setup", changer->vendor, changer->model);

	struct sa_realchanger_private * ch = malloc(sizeof(struct sa_realchanger_private));
	ch->fd = fd;
	ch->db_con = 0;

	changer->status = SA_CHANGER_IDLE;
	changer->ops = &sa_realchanger_ops;
	changer->data = ch;
	changer->lock = sa_ressource_new();

	unsigned int i;
	for (i = 0; i < changer->nb_drives; i++) {
		struct sa_drive * dr = changer->drives + i;
		sa_drive_setup(dr);

		if (dr->is_door_opened && dr->slot->full) {
			struct sa_slot * sl = 0;
			unsigned int i;
			for (i = changer->nb_drives; !sl && i < changer->nb_slots; i++) {
				struct sa_slot * ptrsl = changer->slots + i;
				if (ptrsl->address == dr->slot->src_address)
					sl = ptrsl;
			}
			for (i = changer->nb_drives; !sl && i < changer->nb_slots; i++) {
				struct sa_slot * ptrsl = changer->slots + i;
				if (!ptrsl->full)
					sl = ptrsl;
			}

			if (sl) {
				sa_realchanger_unload(changer, dr, sl);
			} else {
				sa_log_write_all(sa_log_level_warning, "Library (%s | %s): your drive #%ld is offline and there is no place for unloading it", changer->vendor, changer->model, changer->drives - dr);
				sa_log_write_all(sa_log_level_error, "Library (%s | %s): panic: your library require manual maintenance because there is no free slot for unloading drive", changer->vendor, changer->model);
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
		sa_log_write_all(sa_log_level_error, "Library (%s | %s): panic: your library require manual maintenance because there is not enough free slot for unloading drive", changer->vendor, changer->model);
		exit(1);
	}

	for (i = changer->nb_drives; i < changer->nb_slots; i++)
		changer->slots[i].lock = sa_ressource_new();

	pthread_t * workers = 0;
	if (changer->nb_drives > 1) {
		workers = calloc(changer->nb_drives - 1, sizeof(pthread_t));

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

		for (i = 1; i < changer->nb_drives; i++)
			pthread_create(workers + (i - 1), &attr, sa_realchanger_setup2, changer->drives + i);

		pthread_attr_destroy(&attr);
	}

	sa_realchanger_setup2(changer->drives);

	for (i = 1; i < changer->nb_drives; i++)
		pthread_join(workers[i - 1], 0);

	if (workers)
		free(workers);

	struct sa_database * db = sa_db_get_default_db();
	if (db) {
		ch->db_con = db->ops->connect(db, 0);
		if (ch->db_con) {
			ch->db_con->ops->sync_changer(ch->db_con, changer);
		} else {
			sa_log_write_all(sa_log_level_info, "Library (%s | %s): failed to connect to default database", changer->vendor, changer->model);
		}
	} else {
		sa_log_write_all(sa_log_level_warning, "Library (%s | %s): there is no default database so changer is not able to synchronize with one database", changer->vendor, changer->model);
	}

	sa_log_write_all(sa_log_level_info, "Library (%s | %s): setup terminated", changer->vendor, changer->model);
}

void * sa_realchanger_setup2(void * drive) {
	struct sa_drive * dr = drive;
	struct sa_changer * ch = dr->changer;

	if (!dr->is_door_opened) {
		dr->slot->tape = sa_tape_new(dr);
		sa_tape_detect(dr);

		struct sa_slot * sl = 0;
		unsigned int i;
		for (i = ch->nb_drives; !sl && i < ch->nb_slots; i++) {
			struct sa_slot * ptrsl = ch->slots + i;
			if (ptrsl->lock->ops->trylock(ptrsl->lock))
				continue;
			if (ptrsl->address == dr->slot->src_address)
				sl = ptrsl;
			else
				ptrsl->lock->ops->unlock(ptrsl->lock);
		}
		for (i = ch->nb_drives; !sl && i < ch->nb_slots; i++) {
			struct sa_slot * ptrsl = ch->slots + i;
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
		sa_realchanger_unload(ch, dr, sl);
		ch->lock->ops->unlock(ch->lock);
		sl->lock->ops->unlock(sl->lock);
	}

	unsigned int i;
	for (i = ch->nb_drives; i < ch->nb_slots; i++) {
		struct sa_slot * sl = ch->slots + i;
		if (sl->lock->ops->trylock(sl->lock))
			continue;
		if (sl->full && !sl->tape) {
			ch->lock->ops->lock(ch->lock);
			sa_realchanger_load(ch, sl, dr);
			ch->lock->ops->unlock(ch->lock);

			dr->ops->reset(dr);
			dr->slot->tape = sa_tape_new(dr);
			sa_tape_detect(dr);
			dr->ops->eject(dr);

			ch->lock->ops->lock(ch->lock);
			sa_realchanger_unload(ch, dr, sl);
			ch->lock->ops->unlock(ch->lock);
		}
		sl->lock->ops->unlock(sl->lock);
	}

	return 0;
}

int sa_realchanger_unload(struct sa_changer * ch, struct sa_drive * from, struct sa_slot * to) {
	if (!ch || !from || !to) {
		return 1;
	}

	if (to == from->slot)
		return 0;

	if (from->changer != ch || to->changer != ch)
		return 1;

	sa_log_write_all(sa_log_level_info, "Library (%s | %s): unloading tape from drive #%ld to slot #%ld", ch->vendor, ch->model, from - ch->drives, to - ch->slots);

	sa_realchanger_update_status(ch, SA_CHANGER_UNLOADING);
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

		sa_realchanger_update_status(ch, SA_CHANGER_IDLE);
	} else {
		sa_realchanger_update_status(ch, SA_CHANGER_ERROR);
	}

	sa_log_write_all(failed ? sa_log_level_error : sa_log_level_debug, "Library (%s | %s): unloading tape from drive #%ld to slot #%ld finished with code = %d", ch->vendor, ch->model, from - ch->drives, to - ch->slots, failed);

	return 0;
}

void sa_realchanger_update_status(struct sa_changer * ch, enum sa_changer_status status) {
	struct sa_realchanger_private * self = ch->data;
	ch->status = status;

	if (self->db_con)
		self->db_con->ops->sync_changer(self->db_con, ch);
}

