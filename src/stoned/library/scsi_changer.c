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
*  Last modified: Thu, 17 Oct 2013 09:10:48 +0200                            *
\****************************************************************************/

// open
#include <fcntl.h>
// pthread_attr_destroy, pthread_attr_init, pthread_attr_setdetachstate,
// pthread_create, pthread_join
#include <pthread.h>
// free, malloc
#include <stdlib.h>
// strncpy
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// close, exit
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/log.h>
#include <libstone/library/media.h>
#include <libstone/library/ressource.h>

#include "common.h"
#include "scsi.h"

struct st_scsi_changer_private {
	int fd;
	int transport_address;
	struct st_ressource * lock;
};

static struct st_drive * st_scsi_changer_find_free_drive(struct st_changer * ch, struct st_media_format * format, bool for_reading, bool for_writing);
static void st_scsi_changer_free(struct st_changer * ch);
static int st_scsi_changer_load_media(struct st_changer * ch, struct st_media * from, struct st_drive * to);
static int st_scsi_changer_load_slot(struct st_changer * ch, struct st_slot * from, struct st_drive * to);
static void * st_scsi_changer_setup2(void * drive);
static int st_scsi_changer_shut_down(struct st_changer * ch);
static int st_scsi_changer_unload(struct st_changer * ch, struct st_drive * from);
static int st_scsi_changer_update_status(struct st_changer * ch);

static struct st_changer_ops st_scsi_changer_ops = {
	.find_free_drive = st_scsi_changer_find_free_drive,
	.free            = st_scsi_changer_free,
	.load_media      = st_scsi_changer_load_media,
	.load_slot       = st_scsi_changer_load_slot,
	.shut_down       = st_scsi_changer_shut_down,
	.unload          = st_scsi_changer_unload,
	.update_status   = st_scsi_changer_update_status,
};


static struct st_drive * st_scsi_changer_find_free_drive(struct st_changer * ch, struct st_media_format * format __attribute__((unused)), bool for_reading __attribute__((unused)), bool for_writing __attribute__((unused))) {
	if (ch == NULL)
		return NULL;

	unsigned int i;
	for (i = 0; i < ch->nb_drives; i++) {
		struct st_drive * dr = ch->drives + i;

		if (!dr->enabled || dr->slot->media != NULL)
			continue;

		if (!dr->lock->ops->try_lock(dr->lock))
			return dr;
	}

	for (i = 0; i < ch->nb_drives; i++) {
		struct st_drive * dr = ch->drives + i;

		if (!dr->enabled)
			continue;

		if (!dr->lock->ops->try_lock(dr->lock))
			return dr;
	}

	return NULL;
}

static void st_scsi_changer_free(struct st_changer * ch) {
	struct st_scsi_changer_private * self = ch->data;
	close(self->fd);
	self->lock->ops->free(self->lock);
	free(self);
	ch->data = NULL;
	free(ch->db_data);

	free(ch->device);
	free(ch->model);
	free(ch->vendor);
	free(ch->revision);
	free(ch->serial_number);

	unsigned int i;
	for (i = 0; i < ch->nb_drives; i++) {
		struct st_drive * dr = ch->drives + i;
		if (dr->enabled)
			dr->ops->free(dr);
	}
	free(ch->drives);
	ch->drives = NULL;
	ch->nb_drives = 0;

	for (i = 0; i < ch->nb_slots; i++) {
		struct st_slot * sl = ch->slots + i;

		sl->changer = NULL;
		sl->drive = NULL;
		sl->media = NULL;

		free(sl->volume_name);
		if (sl->lock != NULL)
			sl->lock->ops->free(sl->lock);

		free(sl->data);
		free(sl->db_data);
	}
	free(ch->slots);
	ch->nb_slots = 0;
}

static int st_scsi_changer_load_media(struct st_changer * ch, struct st_media * from, struct st_drive * to) {
	if (!ch->enabled)
		return 1;

	unsigned int i;
	struct st_slot * slot = NULL;
	for (i = ch->nb_drives; i < ch->nb_slots; i++) {
		slot = ch->slots + i;

		if (slot->media == from && !slot->lock->ops->lock(slot->lock)) {
			int failed = st_scsi_changer_load_slot(ch, slot, to);
			slot->lock->ops->unlock(slot->lock);
			return failed;
		}
	}

	return 1;
}

static int st_scsi_changer_load_slot(struct st_changer * ch, struct st_slot * from, struct st_drive * to) {
	if (ch == NULL || from == NULL || to == NULL || !ch->enabled || !from->enable)
		return 1;

	if (to->slot == from)
		return 0;

	if (from->changer != ch || to->changer != ch)
		return 1;

	st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: loading media '%s' from slot #%td to drive #%td", ch->vendor, ch->model, from->volume_name, from - ch->slots, to - ch->drives);

	struct st_scsi_changer_private * self = ch->data;
	self->lock->ops->lock(self->lock);
	st_scsi_loader_ready(self->fd);

	int failed = st_scsi_loader_move(self->fd, self->transport_address, from, to->slot);

	if (failed) {
		struct st_slot tmp_from = *from;
		st_scsi_loader_check_slot(self->fd, ch, &tmp_from);

		struct st_slot tmp_to = *to->slot;
		st_scsi_loader_check_slot(self->fd, ch, &tmp_to);

		failed = tmp_from.full || !tmp_to.full;
	}

	if (!failed) {
		struct st_media * media = to->slot->media = from->media;
		from->media = NULL;
		to->slot->volume_name = from->volume_name;
		from->volume_name = NULL;
		from->full = false;
		to->slot->full = true;

		if (media != NULL) {
			media->location = st_media_location_indrive;
			media->load_count++;
		}

		struct st_scsislot * sfrom = from->data;
		struct st_scsislot * sto = to->slot->data;
		sto->src_address = sfrom->address;
		sto->src_slot = from;

		st_log_write_all(st_log_level_debug, st_log_type_changer, "[%s | %s]: loading media '%s' from slot #%td to drive #%td finished with code = OK", ch->vendor, ch->model, to->slot->volume_name, from - ch->slots, to - ch->drives);

		to->ops->update_media_info(to);
	} else
		st_log_write_all(st_log_level_error, st_log_type_changer, "[%s | %s]: loading media '%s' from slot #%td to drive #%td finished with code = %d", ch->vendor, ch->model, to->slot->volume_name, from - ch->slots, to - ch->drives, failed);

	self->lock->ops->unlock(self->lock);

	return failed;
}

void st_scsi_changer_setup(struct st_changer * changer, struct st_database_connection * connection) {
	st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: starting setup", changer->vendor, changer->model);

	int fd = open(changer->device, O_RDWR);

	st_scsi_loader_ready(fd);

	st_scsi_loader_medium_removal(fd, false);

	struct st_scsi_changer_private * ch = malloc(sizeof(struct st_scsi_changer_private));
	ch->fd = fd;
	ch->lock = st_ressource_new(false);

	st_scsi_loader_status_new(fd, changer, &ch->transport_address);

	connection->ops->slots_are_enabled(connection, changer);

	changer->status = st_changer_idle;
	changer->ops = &st_scsi_changer_ops;
	changer->data = ch;

	unsigned int i;
	for (i = changer->nb_drives; i < changer->nb_slots; i++)
		if (changer->slots[i].enable)
			changer->slots[i].lock = st_ressource_new(true);

	unsigned int nb_enabled_drives = 0;
	for (i = 0; i < changer->nb_drives; i++) {
		struct st_drive * dr = changer->drives + i;
		dr->slot->drive = dr;

		if (!dr->enabled)
			continue;
		nb_enabled_drives++;

		st_scsi_tape_drive_setup(dr);

		if (dr->is_empty && dr->slot->full) {
			struct st_slot * slot_from = dr->slot;
			struct st_scsislot * slot_from_data = slot_from->data;

			unsigned int j;
			struct st_slot * slot_to = NULL;
			for (j = changer->nb_drives; j < changer->nb_slots && slot_to == NULL && slot_from_data->src_address; j++) {
				slot_to = changer->slots + j;

				struct st_scsislot * slot_to_data = slot_to->data;
				if (slot_from_data->src_address != slot_to_data->address) {
					slot_to = NULL;
					continue;
				}

				if (slot_to->lock->ops->try_lock(slot_to->lock)) {
					slot_to = NULL;
					break;
				}
			}

			for (j = changer->nb_drives; j < changer->nb_slots && slot_to == NULL; j++) {
				slot_to = changer->slots + j;

				if (slot_to->full) {
					slot_to = NULL;
					continue;
				}

				if (slot_to->lock->ops->try_lock(slot_to->lock)) {
					slot_to = NULL;
					break;
				}
			}

			if (st_scsi_loader_move(ch->fd, ch->transport_address, slot_from, slot_to)) {
				st_log_write_all(st_log_level_warning, st_log_type_changer, "[%s | %s]: your drive #%td is offline but there is a media inside and there is no place for unloading it", changer->vendor, changer->model, changer->drives - dr);
				st_log_write_all(st_log_level_error, st_log_type_changer, "[%s | %s]: panic: your library require manual maintenance because there is no free slot for unloading drive", changer->vendor, changer->model);
				exit(1);
			} else {
				slot_to->media = dr->slot->media;
				dr->slot->media = NULL;
				slot_to->volume_name = dr->slot->volume_name;
				dr->slot->volume_name = NULL;
				dr->slot->full = false;
				slot_to->full = true;
				slot_from_data->src_address = 0;

				if (slot_to->media != NULL)
					slot_to->media->location = st_media_location_online;
			}
		}
	}

	struct st_drive * first_enabled_dr = NULL;
	for (i = 0; i < changer->nb_drives && first_enabled_dr == NULL; i++)
		if (changer->drives[i].enabled)
			first_enabled_dr = changer->drives + i;

	bool need_init = false;
	for (i = changer->nb_drives; i < changer->nb_slots; i++) {
		struct st_slot * sl = changer->slots + i;

		if (!sl->enable || !sl->full)
			continue;

		sl->media = st_media_get_by_label(sl->volume_name);
		if (sl->media == NULL)
			need_init = true;
	}

	if (need_init) {
		pthread_t * workers = NULL;
		if (nb_enabled_drives > 1) {
			workers = calloc(nb_enabled_drives - 1, sizeof(pthread_t));

			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

			unsigned int j;
			for (j = 0; i < changer->nb_drives; i++) {
				struct st_drive * dr = changer->drives + i;
				if (dr->enabled) {
					pthread_create(workers + j, &attr, st_scsi_changer_setup2, dr);
					j++;
				}
			}

			pthread_attr_destroy(&attr);
		}

		st_scsi_changer_setup2(first_enabled_dr);

		for (i = 0; i < nb_enabled_drives - 1; i++)
			pthread_join(workers[i], NULL);

		free(workers);
	}

	st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: setup terminated", changer->vendor, changer->model);
}

static void * st_scsi_changer_setup2(void * drive) {
	struct st_drive * dr = drive;
	struct st_changer * ch = dr->changer;

	if (!dr->is_empty) {
		dr->ops->update_media_info(dr);
		if (st_scsi_changer_unload(ch, dr))
			return NULL;
	}

	unsigned int i;
	for (i = ch->nb_drives; i < ch->nb_slots; i++) {
		struct st_slot * sl = ch->slots + i;

		if (!sl->enable || sl->lock->ops->try_lock(sl->lock))
			continue;

		if (sl->media != NULL || !sl->full) {
			sl->lock->ops->unlock(sl->lock);
			continue;
		}

		sl->media = st_media_get_by_label(sl->volume_name);
		if (sl->media != NULL) {
			sl->media->location = st_media_location_online;
			sl->lock->ops->unlock(sl->lock);
			continue;
		}

		st_scsi_changer_load_slot(ch, sl, dr);

		st_scsi_changer_unload(ch, dr);

		sl->lock->ops->unlock(sl->lock);
	}

	return NULL;
}

static int st_scsi_changer_shut_down(struct st_changer * ch) {
	struct st_scsi_changer_private * self = ch->data;
	st_scsi_loader_medium_removal(self->fd, true);
	return 0;
}

static int st_scsi_changer_unload(struct st_changer * ch, struct st_drive * from) {
	if (ch == NULL || from == NULL || !ch->enabled)
		return 1;

	from->ops->update_media_info(from);
	struct st_media * loaded_media = from->slot->media;
	if (!from->is_empty)
		from->ops->eject(from);

	struct st_scsi_changer_private * self = ch->data;
	self->lock->ops->lock(self->lock);

	st_scsi_loader_ready(self->fd);

	st_log_write_all(st_log_level_debug, st_log_type_changer, "[%s | %s]: unloading media, step 1 : looking for slot", ch->vendor, ch->model);

	struct st_scsislot * slot_from = from->slot->data;
	struct st_slot * to = slot_from->src_slot;

	if (to != NULL && to->enable) {
		if (!to->lock->ops->timed_lock(to->lock, 2000)) {
			st_log_write_all(st_log_level_debug, st_log_type_changer, "[%s | %s]: unloading media, step 1 : found origin slot '#%td' of media '%s'", ch->vendor, ch->model, to - ch->slots, from->slot->volume_name);
		} else {
			st_log_write_all(st_log_level_debug, st_log_type_changer, "[%s | %s]: unloading media, step 1 : origin slot '#%td' is already locked", ch->vendor, ch->model, to - ch->slots);
			to = NULL;
		}
	}

	unsigned int i, nb_free_slots = 0;
	for (i = ch->nb_drives; i < ch->nb_slots && to == NULL; i++) {
		to = ch->slots + i;

		if (!to->enable || to->media != NULL) {
			to = NULL;
			continue;
		}

		nb_free_slots++;

		if (to->lock->ops->try_lock(to->lock))
			to = NULL;
		else
			st_log_write_all(st_log_level_debug, st_log_type_changer, "[%s | %s]: unloading media, step 1 : found free slot '#%td' of media '%s'", ch->vendor, ch->model, to - ch->slots, from->slot->volume_name);
	}

	if (to == NULL && nb_free_slots > 0) {
		// TODO: special action when to free slots are free to use
	}

	if (to == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_changer, "[%s | %s]: unloading media '%s' failed because there is no free slots", ch->vendor, ch->model, from->slot->volume_name);
		return 1;
	}

	st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: unloading media '%s' from drive #%td to slot #%td", ch->vendor, ch->model, from->slot->volume_name, from - ch->drives, to - ch->slots);

	int failed = st_scsi_loader_move(self->fd, self->transport_address, from->slot, to);

	if (failed) {
		struct st_slot tmp_from = *from->slot;
		st_scsi_loader_check_slot(self->fd, ch, &tmp_from);

		struct st_slot tmp_to = *to;
		st_scsi_loader_check_slot(self->fd, ch, &tmp_to);

		failed = tmp_from.full || !tmp_to.full;
	}

	if (!failed) {
		to->media = loaded_media;
		from->slot->media = NULL;
		to->volume_name = from->slot->volume_name;
		from->slot->volume_name = NULL;
		from->slot->full = false;
		to->full = true;
		slot_from->src_address = 0;

		/**
		 * while setup of changer, loaded_media can be NULL
		 */
		if (loaded_media != NULL)
			loaded_media->location = st_media_location_online;
	}

	to->lock->ops->unlock(to->lock);
	self->lock->ops->unlock(self->lock);

	st_log_write_all(failed ? st_log_level_error : st_log_level_debug, st_log_type_changer, "[%s | %s]: unloading media '%s' from drive #%td to slot #%td finished with code = %d", ch->vendor, ch->model, to->volume_name, from - ch->drives, to - ch->slots, failed);

	return failed;
}

static int st_scsi_changer_update_status(struct st_changer * ch __attribute__((unused))) {
	return 0;
}

