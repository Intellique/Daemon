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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Tue, 05 Feb 2013 11:17:55 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// asprintf
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
}

static void st_vtl_changer_free(struct st_changer * ch) {
}

struct st_changer * st_vtl_changer_init(unsigned int nb_drives, unsigned int nb_slots, const char * path, const char * prefix, struct st_media_format * format) {
	char * serial_file;
	asprintf(&serial_file, "%s/serial_number", path);

	char * serial = st_util_file_get_serial(serial_file);
	free(serial_file);

	struct st_vtl_changer * self = malloc(sizeof(struct st_vtl_changer));
	self->path = strdup(path);
	self->medias = calloc(nb_slots, sizeof(struct st_media *));
	self->nb_medias = nb_slots;

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

		sl->lock = st_ressource_new();

		sl->data = NULL;
		sl->db_data = NULL;

		char * sl_dir;
		asprintf(&sl_dir, "%s/slots/%u", path, i);

		sl->media = st_vtl_slot_get_media(ch, sl_dir);

		free(sl_dir);

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
}

static int st_vtl_changer_load_slot(struct st_changer * ch, struct st_slot * from, struct st_drive * to) {
}

static int st_vtl_changer_shut_down(struct st_changer * ch) {
}

static int st_vtl_changer_unload(struct st_changer * ch, struct st_drive * from) {
}

