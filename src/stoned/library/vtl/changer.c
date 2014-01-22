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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Wed, 18 Dec 2013 21:31:25 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// asprintf, rename
#include <stdio.h>
// calloc, malloc
#include <stdlib.h>
// memmove, strcmp, strdup
#include <string.h>
// sleep, symlink
#include <unistd.h>

#include <libstone/library/changer.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/library/vtl.h>
#include <libstone/log.h>
#include <libstone/util/file.h>

#include "common.h"

static struct st_drive * st_vtl_changer_find_free_drive(struct st_changer * ch, struct st_media_format * format, bool for_reading, bool for_writing);
static void st_vtl_changer_free(struct st_changer * ch);
static int st_vtl_changer_load_media(struct st_changer * ch, struct st_media * from, struct st_drive * to);
static int st_vtl_changer_load_slot(struct st_changer * ch, struct st_slot * from, struct st_drive * to);
static int st_vtl_changer_shut_down(struct st_changer * ch);
static int st_vtl_changer_unload(struct st_changer * ch, struct st_drive * from);
static int st_vtl_changer_update_status(struct st_changer * ch);

static struct st_changer_ops st_vtl_changer_ops = {
	.find_free_drive = st_vtl_changer_find_free_drive,
	.free            = st_vtl_changer_free,
	.load_media      = st_vtl_changer_load_media,
	.load_slot       = st_vtl_changer_load_slot,
	.shut_down       = st_vtl_changer_shut_down,
	.unload          = st_vtl_changer_unload,
	.update_status   = st_vtl_changer_update_status,
};


static struct st_drive * st_vtl_changer_find_free_drive(struct st_changer * ch, struct st_media_format * format __attribute__((unused)), bool for_reading __attribute__((unused)), bool for_writing __attribute__((unused))) {
	if (ch == NULL)
		return NULL;

	unsigned int i;
	struct st_drive * dr = NULL;
	for (i = 0; dr == NULL && i < ch->nb_drives; i++) {
		dr = ch->drives + i;

		if (!dr->enabled || dr->slot->media != NULL) {
			dr = NULL;
			continue;
		}

		if (dr->lock->ops->try_lock(dr->lock))
			dr = NULL;
	}

	for (i = 0; dr == NULL && i < ch->nb_drives; i++) {
		dr = ch->drives + i;

		if (!dr->enabled || dr->lock->ops->try_lock(dr->lock))
			dr = NULL;
	}

	return dr;
}

static void st_vtl_changer_free(struct st_changer * ch) {
	struct st_vtl_changer * vch = ch->data;

	unsigned int i;
	for (i = 0; i < vch->nb_medias; i++) {
		struct st_media * media = vch->medias[i];
		struct st_vtl_media * vm = media->data;

		free(vm->path);
		vm->path = NULL;
		vm->slot = NULL;
		free(vm->prefix);
		vm->prefix = NULL;
		free(vm);

		media->data = NULL;
	}

	free(vch->path);
	free(vch->medias);
	vch->lock->ops->free(vch->lock);
	free(vch);

	free(ch->model);
	free(ch->vendor);
	free(ch->revision);
	free(ch->serial_number);

	for (i = 0; i < ch->nb_drives; i++) {
		struct st_drive * dr = ch->drives + i;
		dr->ops->free(dr);
	}
	free(ch->drives);

	for (i = 0; i < ch->nb_slots; i++) {
		struct st_slot * sl = ch->slots + i;
		free(sl->volume_name);
		if (sl->lock != NULL)
			sl->lock->ops->free(sl->lock);

		struct st_vtl_slot * vsl = sl->data;
		if (vsl != NULL) {
			free(vsl->path);
			free(vsl);
		}

		free(sl->db_data);
	}
	free(ch->slots);

	free(ch->db_data);
}

struct st_changer * st_vtl_changer_init(struct st_vtl_config * cfg) {
	char * serial_file;
	asprintf(&serial_file, "%s/serial_number", cfg->path);

	char * serial = st_util_file_get_serial(serial_file);
	free(serial_file);

	struct st_vtl_changer * self = malloc(sizeof(struct st_vtl_changer));
	self->path = strdup(cfg->path);
	self->medias = calloc(cfg->nb_slots, sizeof(struct st_media *));
	self->nb_medias = cfg->nb_slots;
	self->lock = st_ressource_new(false);

	unsigned int i;
	for (i = 0; i < cfg->nb_slots; i++) {
		char * md_dir;
		asprintf(&md_dir, "%s/medias/%s%03u", cfg->path, cfg->prefix, i);

		self->medias[i] = st_vtl_media_init(md_dir, cfg->prefix, i, cfg->format);
	}

	struct st_changer * ch = malloc(sizeof(struct st_changer));
	ch->device = NULL;
	ch->status = st_changer_idle;
	ch->enabled = true;

	ch->model = strdup("Stone vtl changer");
	ch->vendor = strdup("Intellique");
	ch->revision = strdup("A00");
	ch->serial_number = serial;
	ch->wwn = NULL;
	ch->barcode = true;

	ch->drives = calloc(cfg->nb_drives, sizeof(struct st_drive));
	ch->nb_drives = cfg->nb_drives;

	ch->nb_slots = cfg->nb_slots + cfg->nb_drives;
	ch->slots = calloc(ch->nb_slots, sizeof(struct st_slot));

	ch->ops = &st_vtl_changer_ops;
	ch->data = self;
	ch->db_data = NULL;

	for (i = 0; i < ch->nb_drives; i++) {
		struct st_slot * sl = ch->slots + i;
		sl->changer = ch;
		sl->drive = ch->drives + i;
		sl->media = NULL;

		sl->volume_name = NULL;
		sl->full = false;
		sl->type = st_slot_type_drive;
		sl->enable = true;

		sl->lock = NULL;

		sl->data = NULL;
		sl->db_data = NULL;

		char * dr_dir;
		asprintf(&dr_dir, "%s/drives/%u", cfg->path, i);

		sl->media = st_vtl_slot_get_media(ch, dr_dir);
		st_vtl_drive_init(ch->drives + i, sl, dr_dir, cfg->format);

		if (sl->media != NULL) {
			sl->volume_name = strdup(sl->media->label);
			sl->full = true;
		}
	}

	for (i = 0; i < cfg->nb_slots; i++) {
		struct st_slot * sl = ch->slots + ch->nb_drives + i;
		sl->changer = ch;
		sl->drive = NULL;
		sl->media = NULL;

		sl->volume_name = NULL;
		sl->full = false;
		sl->type = st_slot_type_storage;
		sl->enable = true;

		sl->lock = st_ressource_new(false);

		sl->db_data = NULL;

		char * sl_dir;
		asprintf(&sl_dir, "%s/slots/%u", cfg->path, i);

		struct st_vtl_slot * vsl = sl->data = malloc(sizeof(struct st_vtl_slot));
		vsl->path = sl_dir;

		sl->media = st_vtl_slot_get_media(ch, sl_dir);

		if (sl->media != NULL) {
			sl->volume_name = strdup(sl->media->label);
			sl->full = true;
		}
	}

	for (i = 0; i < cfg->nb_slots; i++) {
		struct st_media * md = self->medias[i];
		struct st_vtl_media * vmd = md->data;
		struct st_slot * sl = ch->slots + ch->nb_drives + i;

		if (!vmd->used) {
			unsigned int j;
			for (j = 0; j < cfg->nb_slots; j++) {
				char * link_file;
				asprintf(&link_file, "%s/slots/%u/media", cfg->path, j);

				if (!access(link_file, F_OK)) {
					free(link_file);
					continue;
				}

				char * media_file;
				asprintf(&media_file, "../../medias/%s%03u", cfg->prefix, i);

				symlink(media_file, link_file);

				free(media_file);
				free(link_file);

				char * sl_dir;
				asprintf(&sl_dir, "%s/slots/%u", cfg->path, i);
				sl->media = st_vtl_slot_get_media(ch, sl_dir);
				free(sl_dir);

				if (sl->media != NULL) {
					sl->volume_name = strdup(sl->media->label);
					sl->full = true;
				}

				break;
			}
		}
	}

	return ch;
}

static int st_vtl_changer_load_media(struct st_changer * ch, struct st_media * from, struct st_drive * to) {
	if (!ch->enabled)
		return 1;

	unsigned int i;
	struct st_slot * slot = NULL;
	for (i = ch->nb_drives; i < ch->nb_slots; i++) {
		slot = ch->slots + i;

		if (slot->media == from && !slot->lock->ops->try_lock(slot->lock)) {
			int failed = st_vtl_changer_load_slot(ch, slot, to);
			slot->lock->ops->unlock(slot->lock);
			return failed;
		}
	}

	return 1;
}

static int st_vtl_changer_load_slot(struct st_changer * ch, struct st_slot * from, struct st_drive * to) {
	if (!ch->enabled)
		return 1;

	if (to->slot == from)
		return 0;

	if (from->changer != ch || to->changer != ch)
		return 1;

	st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: loading media '%s' from slot #%td to drive #%td", ch->vendor, ch->model, from->volume_name, from - ch->slots, to - ch->drives);

	struct st_vtl_changer * self = ch->data;
	self->lock->ops->lock(self->lock);

	struct st_vtl_slot * vsl = from->data;
	struct st_vtl_drive * vdr = to->data;

	char * vsl_path, * vdr_path;
	asprintf(&vsl_path, "%s/media", vsl->path);
	asprintf(&vdr_path, "%s/media", vdr->path);

	int failed = rename(vsl_path, vdr_path);
	free(vsl_path);
	free(vdr_path);

	if (!failed) {
		struct st_slot * sto = to->slot;

		struct st_media * media = sto->media = from->media;
		from->media = NULL;
		sto->volume_name = from->volume_name;
		from->volume_name = NULL;
		sto->full = true;
		from->full = false;

		media->location = st_media_location_indrive;
		media->load_count++;

		to->ops->update_media_info(to);

		struct st_vtl_media * vmd = media->data;
		vmd->slot = from;
	}

	self->lock->ops->unlock(self->lock);

	return failed;
}

static int st_vtl_changer_shut_down(struct st_changer * ch __attribute__((unused))) {
	return 0;
}

void st_vtl_changer_sync(struct st_changer * ch, struct st_vtl_config * cfg) {
	struct st_vtl_changer * self = ch->data;

	if (strcmp(self->path, cfg->path)) {
		st_util_file_mkdir(cfg->path, 0755);
		st_util_file_rm(cfg->path);
		st_util_file_mv(self->path, cfg->path);

		free(self->path);
		self->path = strdup(cfg->path);
	}

	unsigned int i;
	void * new_addr;
	// add new medias & slots
	if (ch->nb_slots - ch->nb_drives < cfg->nb_slots) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "Vtl: add new slots { before: %u, now: %u }", ch->nb_slots - ch->nb_drives, cfg->nb_slots);

		new_addr = realloc(self->medias, cfg->nb_slots * sizeof(struct st_media *));
		if (new_addr == NULL) {
			st_log_write_all(st_log_level_debug, st_log_type_changer, "[%s | %s]: Not enough memory to grow vtl with %u slots", ch->vendor, ch->model, cfg->nb_slots);
			return;
		}

		self->medias = new_addr;

		// create new medias
		for (i = self->nb_medias; i < cfg->nb_slots; i++) {
			char * md_dir;
			asprintf(&md_dir, "%s/medias/%s%03u", cfg->path, cfg->prefix, i);

			if (access(md_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(md_dir, 0700)) {
				st_log_write_all(st_log_level_error, st_log_type_daemon, "VTL: Failed to create media directory #%u: %s", i, md_dir);
				free(md_dir);
			}

			self->medias[i] = st_vtl_media_init(md_dir, cfg->prefix, i, cfg->format);

			free(md_dir);
		}
		self->nb_medias = cfg->nb_slots;

		new_addr = realloc(ch->slots, (cfg->nb_slots + ch->nb_drives) * sizeof(struct st_slot));

		if (new_addr == NULL) {
			st_log_write_all(st_log_level_debug, st_log_type_changer, "[%s | %s]: Not enough memory to grow vtl with %u slots", ch->vendor, ch->model, cfg->nb_slots);
			return;
		}

		ch->slots = new_addr;

		// re-link drive to slot
		for (i = 0; i < ch->nb_drives; i++) {
			struct st_drive * dr = ch->drives + i;
			dr->slot = ch->slots + i;
		}

		// create new slots
		for (i = ch->nb_slots - ch->nb_drives; i < cfg->nb_slots; i++) {
			char * sl_dir;
			asprintf(&sl_dir, "%s/slots/%u", cfg->path, i);

			if (access(sl_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(sl_dir, 0700)) {
				st_log_write_all(st_log_level_error, st_log_type_daemon, "VTL: Failed to create slot directory #%u: %s", i, sl_dir);
				free(sl_dir);
				return;
			}

			struct st_slot * sl = ch->slots + ch->nb_drives + i;
			sl->changer = ch;
			sl->drive = NULL;
			sl->media = NULL;

			sl->volume_name = NULL;
			sl->full = false;
			sl->type = st_slot_type_storage;
			sl->enable = true;

			sl->lock = st_ressource_new(false);

			sl->db_data = NULL;

			struct st_vtl_slot * vsl = sl->data = malloc(sizeof(struct st_vtl_slot));
			vsl->path = sl_dir;

			char * link_file;
			asprintf(&link_file, "%s/slots/%u/media", cfg->path, i);

			char * media_file;
			asprintf(&media_file, "../../medias/%s%03u", cfg->prefix, i);

			symlink(media_file, link_file);

			free(media_file);
			free(link_file);

			sl->media = st_vtl_slot_get_media(ch, sl_dir);
			free(sl_dir);

			if (sl->media != NULL) {
				sl->volume_name = strdup(sl->media->label);
				sl->full = true;
			}
		}

		ch->nb_slots = cfg->nb_slots + ch->nb_drives;
	}

	// add & remove drives
	if (ch->nb_drives < cfg->nb_drives) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "Vtl: add new drive { before: %u, now: %u }", ch->nb_drives, cfg->nb_drives);

		new_addr = realloc(ch->slots, (ch->nb_slots + cfg->nb_drives - ch->nb_drives) * sizeof(struct st_slot));

		if (new_addr == NULL) {
			st_log_write_all(st_log_level_debug, st_log_type_changer, "[%s | %s]: Not enough memory to grow vtl with %u drives", ch->vendor, ch->model, cfg->nb_drives);
			return;
		}

		ch->slots = new_addr;
		ch->nb_slots += cfg->nb_drives - ch->nb_drives;

		// re-link drive to slot
		for (i = 0; i < ch->nb_drives; i++) {
			struct st_drive * dr = ch->drives + i;
			dr->slot = ch->slots + i;
		}

		memmove(ch->slots + cfg->nb_drives, ch->slots + ch->nb_drives, (ch->nb_slots - cfg->nb_drives) * sizeof(struct st_slot));

		new_addr = realloc(ch->drives, cfg->nb_drives * sizeof(struct st_drive));
		if (new_addr == NULL) {
			st_log_write_all(st_log_level_debug, st_log_type_changer, "[%s | %s]: Not enough memory to grow vtl with %u drives", ch->vendor, ch->model, cfg->nb_drives);
			return;
		}
		ch->drives = new_addr;

		// re-link slot to drive
		for (i = 0; i < ch->nb_drives; i++) {
			struct st_slot * sl = ch->slots + i;
			sl->drive = ch->drives + i;
		}

		// create new drives
		for (i = ch->nb_drives; i < cfg->nb_drives; i++) {
			char * dr_dir;
			asprintf(&dr_dir, "%s/drives/%u", self->path, i);

			if (access(dr_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(dr_dir, 0700)) {
				st_log_write_all(st_log_level_error, st_log_type_daemon, "VTL: Failed to create drive directory #%u: %s", i, dr_dir);
				free(dr_dir);
				return;
			}

			struct st_slot * sl = ch->slots + i;
			sl->changer = ch;
			sl->drive = ch->drives + i;
			sl->media = NULL;

			sl->volume_name = NULL;
			sl->full = false;
			sl->type = st_slot_type_drive;
			sl->enable = true;

			sl->lock = NULL;

			sl->data = NULL;
			sl->db_data = NULL;

			st_vtl_drive_init(ch->drives + i, sl, dr_dir, cfg->format);

			if (sl->media != NULL) {
				sl->volume_name = strdup(sl->media->label);
				sl->full = true;
			}
		}

		ch->nb_drives = cfg->nb_drives;
	} else if (ch->nb_drives > cfg->nb_drives) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "Vtl: remove new drive { before: %u, now: %u }", ch->nb_drives, cfg->nb_drives);

		// remove old drives
		for (i = ch->nb_drives - 1; i >= cfg->nb_drives; i--) {
			struct st_drive * dr = ch->drives + i;
			if (dr->slot->media != NULL)
				st_vtl_changer_unload(ch, dr);

			dr->ops->free(dr);

			char * dr_dir;
			asprintf(&dr_dir, "%s/drives/%u", self->path, i);

			st_util_file_rm(dr_dir);

			free(dr_dir);
		}

		new_addr = realloc(ch->drives, cfg->nb_drives * sizeof(struct st_drive));
		if (new_addr != NULL)
			ch->drives = new_addr;

		memmove(ch->slots + cfg->nb_drives, ch->slots + ch->nb_drives, (ch->nb_slots - ch->nb_drives));

		new_addr = realloc(ch->slots, (ch->nb_slots + cfg->nb_drives - ch->nb_drives) * sizeof(struct st_slot));
		if (new_addr)
			ch->slots = new_addr;
		ch->nb_slots -= ch->nb_drives - cfg->nb_drives;
		ch->nb_drives = cfg->nb_drives;

		// re-link drive to slot
		for (i = 0; i < ch->nb_drives; i++) {
			struct st_drive * dr = ch->drives + i;
			dr->slot = ch->slots + i;
		}
	}

	// remove media & slot
	if (ch->nb_slots - ch->nb_drives > cfg->nb_slots) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "Vtl: remove slots { before: %u, now: %u }", ch->nb_slots - ch->nb_drives, cfg->nb_slots);

		for (i = 0; i < ch->nb_drives; i++) {
			struct st_drive * dr = ch->drives + i;
			if (dr->slot->media != NULL)
				st_vtl_changer_unload(ch, dr);
		}

		for (i = cfg->nb_slots + ch->nb_drives; i < ch->nb_slots; i++) {
			struct st_slot * sl = ch->slots + i;
			struct st_media * media = sl->media;
			free(sl->volume_name);
			if (sl->lock != NULL)
				sl->lock->ops->free(sl->lock);

			struct st_vtl_slot * vsl = sl->data;
			if (vsl != NULL) {
				st_util_file_rm(vsl->path);
				free(vsl->path);
				free(vsl);
			}

			free(sl->db_data);

			if (media != NULL) {
				struct st_vtl_media * vtl_media = media->data;
				st_util_file_rm(vtl_media->path);
			}
		}

		new_addr = realloc(ch->slots, (ch->nb_drives + cfg->nb_slots) * sizeof(struct st_slot));
		if (new_addr != NULL) {
			ch->slots = new_addr;

			// re-link drive to slot
			for (i = 0; i < ch->nb_drives; i++) {
				struct st_drive * dr = ch->drives + i;
				dr->slot = ch->slots + i;
			}
		}

		ch->nb_slots = cfg->nb_slots + ch->nb_drives;
	}
}

static int st_vtl_changer_unload(struct st_changer * ch, struct st_drive * from) {
	if (ch == NULL || from == NULL || !ch->enabled)
		return 1;

	if (from->slot->media == NULL)
		return 0;

	struct st_vtl_changer * self = ch->data;
	self->lock->ops->lock(self->lock);

	st_log_write_all(st_log_level_debug, st_log_type_changer, "[%s | %s]: unloading media, step 1 : looking for slot", ch->vendor, ch->model);

	struct st_media * media = from->slot->media;
	struct st_vtl_media * vmd = media->data;

	struct st_slot * sl_to = vmd->slot;
	if (sl_to != NULL && sl_to->lock->ops->try_lock(sl_to->lock))
		sl_to = NULL;

	unsigned int i;
	for (i = ch->nb_drives; i < ch->nb_slots && sl_to == NULL; i++) {
		sl_to = ch->slots + i;

		if (sl_to->media != NULL) {
			sl_to = NULL;
			continue;
		}

		if (sl_to->lock->ops->try_lock(sl_to->lock))
			sl_to = NULL;
		else
			st_log_write_all(st_log_level_debug, st_log_type_changer, "[%s | %s]: unloading media, step 1 : found free slot '#%td' of media '%s'", ch->vendor, ch->model, sl_to - ch->slots, from->slot->volume_name);
	}

	int failed = 1;
	if (sl_to != NULL) {
		st_log_write_all(st_log_level_info, st_log_type_changer, "[%s | %s]: unloading media '%s' from drive #%td to slot #%td", ch->vendor, ch->model, from->slot->volume_name, from - ch->drives, sl_to - ch->slots);

		struct st_vtl_drive * vdr = from->data;
		struct st_vtl_slot * vsl = sl_to->data;

		char * vdr_path, * vsl_path;
		asprintf(&vdr_path, "%s/media", vdr->path);
		asprintf(&vsl_path, "%s/media", vsl->path);

		failed = rename(vdr_path, vsl_path);
		free(vdr_path);
		free(vsl_path);

		if (!failed) {
			struct st_slot * sl_from = from->slot;
			sl_to->media = sl_from->media;
			sl_from->media = NULL;
			sl_to->volume_name = sl_from->volume_name;
			sl_from->volume_name = NULL;
			sl_to->full = true;
			sl_from->full = false;
		}
	}

	self->lock->ops->unlock(self->lock);
	return failed;
}

static int st_vtl_changer_update_status(struct st_changer * ch __attribute__((unused))) {
	return 0;
}

