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
*  Last modified: Fri, 22 Feb 2013 11:53:05 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// asprintf, rename
#include <stdio.h>
// calloc, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// sleep, symlink
#include <unistd.h>

#include <libstone/library/changer.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>
#include <libstone/util/file.h>

#include "common.h"

static struct st_drive * st_vtl_changer_find_free_drive(struct st_changer * ch, struct st_media_format * format, bool for_reading, bool for_writing);
static void st_vtl_changer_free(struct st_changer * ch);
static int st_vtl_changer_load_media(struct st_changer * ch, struct st_media * from, struct st_drive * to);
static int st_vtl_changer_load_slot(struct st_changer * ch, struct st_slot * from, struct st_drive * to);
static int st_vtl_changer_shut_down(struct st_changer * ch);
static int st_vtl_changer_unload(struct st_changer * ch, struct st_drive * from);

static struct st_changer_ops st_vtl_changer_ops = {
	.find_free_drive = st_vtl_changer_find_free_drive,
	.free            = st_vtl_changer_free,
	.load_media      = st_vtl_changer_load_media,
	.load_slot       = st_vtl_changer_load_slot,
	.shut_down       = st_vtl_changer_shut_down,
	.unload          = st_vtl_changer_unload,
};


static struct st_drive * st_vtl_changer_find_free_drive(struct st_changer * ch, struct st_media_format * format, bool for_reading, bool for_writing) {
	if (ch == NULL)
		return NULL;

	unsigned int i;
	for (i = 0; i < ch->nb_drives; i++) {
		struct st_drive * dr = ch->drives + i;

		if (!dr->lock->ops->try_lock(dr->lock))
			return dr;
	}

	return NULL;
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

struct st_changer * st_vtl_changer_init(unsigned int nb_drives, unsigned int nb_slots, const char * path, char * prefix, struct st_media_format * format) {
	char * serial_file;
	asprintf(&serial_file, "%s/serial_number", path);

	char * serial = st_util_file_get_serial(serial_file);
	free(serial_file);

	struct st_vtl_changer * self = malloc(sizeof(struct st_vtl_changer));
	self->path = strdup(path);
	self->medias = calloc(nb_slots, sizeof(struct st_media *));
	self->nb_medias = nb_slots;
	self->lock = st_ressource_new(false);

	unsigned int i;
	for (i = 0; i < nb_slots; i++) {
		char * md_dir;
		asprintf(&md_dir, "%s/medias/%s%03u", path, prefix, i);

		self->medias[i] = st_vtl_media_init(md_dir, prefix, i, format);
	}

	struct st_changer * ch = malloc(sizeof(struct st_changer));
	ch->device = NULL;
	ch->status = st_changer_idle;
	ch->enabled = true;

	ch->model = strdup("Stone vtl changer");
	ch->vendor = strdup("Intellique");
	ch->revision = strdup("A00");
	ch->serial_number = serial;
	ch->barcode = true;

	ch->drives = calloc(nb_drives, sizeof(struct st_drive));
	ch->nb_drives = nb_drives;

	ch->nb_slots = nb_slots + nb_drives;
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
		asprintf(&dr_dir, "%s/drives/%u", path, i);

		sl->media = st_vtl_slot_get_media(ch, dr_dir);
		st_vtl_drive_init(ch->drives + i, sl, dr_dir, format);

		if (sl->media != NULL) {
			sl->volume_name = strdup(sl->media->label);
			sl->full = true;
		}
	}

	for (i = 0; i < nb_slots; i++) {
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
		asprintf(&sl_dir, "%s/slots/%u", path, i);

		struct st_vtl_slot * vsl = sl->data = malloc(sizeof(struct st_vtl_slot));
		vsl->path = sl_dir;

		sl->media = st_vtl_slot_get_media(ch, sl_dir);

		if (sl->media != NULL) {
			sl->volume_name = strdup(sl->media->label);
			sl->full = true;
		}
	}

	for (i = 0; i < nb_slots; i++) {
		struct st_media * md = self->medias[i];
		struct st_vtl_media * vmd = md->data;

		if (!vmd->used) {
			unsigned int j;
			for (j = 0; j < nb_slots; j++) {
				char * link_file;
				asprintf(&link_file, "%s/slots/%u/media", path, j);

				if (!access(link_file, F_OK)) {
					free(link_file);
					continue;
				}

				char * media_file;
				asprintf(&media_file, "../../medias/%s%03u", prefix, i);

				symlink(media_file, link_file);

				free(media_file);
				free(link_file);

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

		media->load_count++;

		to->status = st_drive_loaded_idle;
		to->is_empty = false;

		struct st_vtl_media * vmd = media->data;
		vmd->slot = from;
	}

	self->lock->ops->unlock(self->lock);

	return failed;
}

static int st_vtl_changer_shut_down(struct st_changer * ch __attribute__((unused))) {
	return 0;
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

