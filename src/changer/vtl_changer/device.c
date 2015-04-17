/****************************************************************************\
*                    ______           _      ____                            *
*                   / __/ /____  ____(_)__ _/ __ \___  ___                   *
*                  _\ \/ __/ _ \/ __/ / _ `/ /_/ / _ \/ -_)                  *
*                 /___/\__/\___/_/ /_/\_, /\____/_//_/\__/                   *
*                                      /_/                                   *
*  ------------------------------------------------------------------------  *
*  This file is a part of Storiq One                                         *
*                                                                            *
*  Storiq One is free software; you can redistribute it and/or modify        *
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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// asprintf, rename
#include <stdio.h>
// calloc, free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>
// access, sleep, symlink
#include <unistd.h>
// time
#include <time.h>

#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-changer/changer.h>
#include <libstoriqone-changer/drive.h>

#include "device.h"
#include "util.h"

static char * vtl_prefix = NULL;
static char * vtl_root_dir = NULL;

static int sochgr_vtl_changer_init(struct so_value * config, struct so_database_connection * db_connection);
static int sochgr_vtl_changer_load(struct so_slot * from, struct so_drive * to, struct so_database_connection * db_connection);
static int sochgr_vtl_changer_put_offline(struct so_database_connection * db_connection);
static int sochgr_vtl_changer_put_online(struct so_database_connection * db_connection);
static int sochgr_vtl_changer_shut_down(struct so_database_connection * db_connection);
static int sochgr_vtl_changer_unload(struct so_drive * from, struct so_database_connection * db_connection);

struct so_changer_ops sochgr_vtl_changer_ops = {
	.init        = sochgr_vtl_changer_init,
	.load        = sochgr_vtl_changer_load,
	.put_offline = sochgr_vtl_changer_put_offline,
	.put_online  = sochgr_vtl_changer_put_online,
	.shut_down   = sochgr_vtl_changer_shut_down,
	.unload      = sochgr_vtl_changer_unload,
};

static struct so_changer sochgr_vtl_changer = {
	.model         = "Stone vtl changer",
	.vendor        = "Intellique",
	.revision      = "A01",
	.serial_number = NULL,
	.wwn           = NULL,
	.barcode       = true,

	.status      = so_changer_status_idle,
	.next_action = so_changer_action_none,
	.is_online   = true,
	.enable      = true,

	.drives    = NULL,
	.nb_drives = 0,

	.slots    = NULL,
	.nb_slots = 0,

	.ops = &sochgr_vtl_changer_ops,

	.data = NULL,
	.db_data = NULL,
};


struct so_changer * sochgr_vtl_changer_get_device() {
	return &sochgr_vtl_changer;
}

static int sochgr_vtl_changer_init(struct so_value * config, struct so_database_connection * db_connection) {
	long long nb_drives, nb_slots;
	struct so_value * drives = NULL, * vformat = NULL;
	so_value_unpack(config, "{sssisisosossss}", "path", &vtl_root_dir, "nb drives", &nb_drives, "nb slots", &nb_slots, "drives", &drives, "format", &vformat, "prefix", &vtl_prefix, "uuid", &sochgr_vtl_changer.serial_number);

	struct so_media_format * format = malloc(sizeof(struct so_media_format));
	bzero(format, sizeof(struct so_media_format));
	so_media_format_sync(format, vformat);

	sochgr_vtl_changer.drives = calloc(nb_drives, sizeof(struct so_drive));
	sochgr_vtl_changer.nb_drives = nb_drives;

	sochgr_vtl_changer.nb_slots = nb_drives + nb_slots;
	sochgr_vtl_changer.slots = calloc(sochgr_vtl_changer.nb_slots, sizeof(struct so_slot));

	if (access(vtl_root_dir, F_OK | R_OK | W_OK | X_OK) == 0) {
		long long int i;
		for (i = 0; i < nb_drives; i++) {
			char * drive_dir;
			char * serial_file;
			asprintf(&drive_dir, "%s/drives/%Ld", vtl_root_dir, i);
			asprintf(&serial_file, "%s/drives/%Ld/serial_number", vtl_root_dir, i);

			struct so_drive * drive = sochgr_vtl_changer.drives + i;
			drive->model = strdup("Stone vtl drive");
			drive->vendor = strdup("Intellique");
			drive->revision = strdup("A01");
			drive->serial_number = so_file_read_all_from(serial_file);

			drive->status = so_drive_status_unknown;
			drive->enable = true;

			drive->density_code = format->density_code;
			drive->mode = so_media_format_mode_disk;

			drive->changer = &sochgr_vtl_changer;
			drive->index = i;

			struct so_slot * sl = sochgr_vtl_changer.slots + i;
			sl->changer = &sochgr_vtl_changer;
			sl->index = i;
			sl->drive = drive;
			drive->slot = sl;

			sl->full = false;
			sl->enable = true;

			struct sochgr_vtl_changer_slot * vtl_sl = sl->data = malloc(sizeof(struct sochgr_vtl_changer_slot));
			bzero(vtl_sl, sizeof(struct sochgr_vtl_changer_slot));
			vtl_sl->path = drive_dir;

			struct so_value * vdrive = so_value_list_get(drives, i, false);
			so_value_hashtable_put2(vdrive, "device", so_value_new_string(drive_dir), true);
			so_value_hashtable_put2(vdrive, "format", vformat, false);
			so_value_hashtable_put2(vdrive, "serial number", so_value_new_string(drive->serial_number), true);

			char * media_link;
			asprintf(&media_link, "%s/drives/%Ld/media", vtl_root_dir, i);
			so_file_rm(media_link);

			free(serial_file);
			free(media_link);
		}

		for (i = 0; i < nb_slots; i++) {
			char * serial_file;
			asprintf(&serial_file, "%s/medias/%s%03Ld/serial_number", vtl_root_dir, vtl_prefix, i);

			struct so_slot * sl = sochgr_vtl_changer.slots + nb_drives + i;
			sl->changer = &sochgr_vtl_changer;
			sl->index = nb_drives + i;
			asprintf(&sl->volume_name, "%s%03Ld", vtl_prefix, i);
			sl->full = true;

			struct sochgr_vtl_changer_slot * vtl_sl = sl->data = malloc(sizeof(struct sochgr_vtl_changer_slot));
			bzero(vtl_sl, sizeof(struct sochgr_vtl_changer_slot));
			asprintf(&vtl_sl->path, "%s/slots/%Ld", vtl_root_dir, i);

			char * media_link, * link;
			asprintf(&media_link, "../../medias/%s%03Ld", vtl_prefix, i);
			asprintf(&link, "%s/slots/%Ld/media", vtl_root_dir, i);
			so_file_rm(link);
			symlink(media_link, link);

			char * serial_number = sochgr_vtl_util_get_serial(serial_file);
			struct so_media * media = db_connection->ops->get_media(db_connection, serial_number, NULL, NULL);

			if (media != NULL)
				free(serial_number);
			else {
				media = malloc(sizeof(struct so_media));
				bzero(media, sizeof(struct so_media));
				asprintf(&media->label, "%s%03Ld", vtl_prefix, i);
				media->medium_serial_number = serial_number;
				media->name = strdup(media->label);
				media->status = so_media_status_new;
				media->first_used = time(NULL);
				media->use_before = media->first_used + format->life_span;
				media->block_size = format->block_size;
				media->free_block = media->total_block = format->capacity / format->block_size;
				media->append = true;
				media->type = so_media_type_rewritable;
				media->format = format;
			}

			sl->media = media;

			free(link);
			free(media_link);
			free(serial_file);
		}
	} else {
		if (so_file_mkdir(vtl_root_dir, 0700))
			goto init_error;

		long long int i;
		for (i = 0; i < nb_drives; i++) {
			char * drive_dir;
			asprintf(&drive_dir, "%s/drives/%Ld", vtl_root_dir, i);
			so_file_mkdir(drive_dir, 0700);

			char * serial_file;
			asprintf(&serial_file, "%s/drives/%Ld/serial_number", vtl_root_dir, i);

			struct so_drive * drive = sochgr_vtl_changer.drives + i;
			drive->model = strdup("Stone vtl drive");
			drive->vendor = strdup("Intellique");
			drive->revision = strdup("A01");
			drive->serial_number = sochgr_vtl_util_get_serial(serial_file);

			drive->status = so_drive_status_unknown;
			drive->enable = true;

			drive->density_code = format->density_code;
			drive->mode = so_media_format_mode_disk;

			drive->changer = &sochgr_vtl_changer;
			drive->index = i;

			struct so_slot * sl = sochgr_vtl_changer.slots + i;
			sl->changer = &sochgr_vtl_changer;
			sl->index = i;
			sl->drive = drive;
			drive->slot = sl;

			sl->full = false;
			sl->enable = true;

			struct sochgr_vtl_changer_slot * vtl_sl = sl->data = malloc(sizeof(struct sochgr_vtl_changer_slot));
			bzero(vtl_sl, sizeof(struct sochgr_vtl_changer_slot));
			vtl_sl->path = drive_dir;

			struct so_value * vdrive = so_value_list_get(drives, i, false);
			so_value_hashtable_put2(vdrive, "device", so_value_new_string(drive_dir), true);
			so_value_hashtable_put2(vdrive, "format", vformat, false);
			so_value_hashtable_put2(vdrive, "serial number", so_value_new_string(drive->serial_number), true);

			free(serial_file);
		}

		for (i = 0; i < nb_slots; i++) {
			char * media_dir;
			asprintf(&media_dir, "%s/medias/%s%03Ld", vtl_root_dir, vtl_prefix, i);
			so_file_mkdir(media_dir, 0700);

			char * serial_file;
			asprintf(&serial_file, "%s/medias/%s%03Ld/serial_number", vtl_root_dir, vtl_prefix, i);

			struct so_slot * sl = sochgr_vtl_changer.slots + nb_drives + i;
			sl->changer = &sochgr_vtl_changer;
			sl->index = nb_drives + i;
			asprintf(&sl->volume_name, "%s%03Ld", vtl_prefix, i);
			sl->full = true;

			struct sochgr_vtl_changer_slot * vtl_sl = sl->data = malloc(sizeof(struct sochgr_vtl_changer_slot));
			bzero(vtl_sl, sizeof(struct sochgr_vtl_changer_slot));
			asprintf(&vtl_sl->path, "%s/slots/%Ld", vtl_root_dir, i);
			so_file_mkdir(vtl_sl->path, 0700);

			char * media_link, * link;
			asprintf(&media_link, "../../medias/%s%03Ld", vtl_prefix, i);
			asprintf(&link, "%s/slots/%Ld/media", vtl_root_dir, i);
			symlink(media_link, link);

			char * serial_number = sochgr_vtl_util_get_serial(serial_file);
			struct so_media * media = db_connection->ops->get_media(db_connection, serial_number, NULL, NULL);

			if (media != NULL)
				free(serial_number);
			else {
				media = malloc(sizeof(struct so_media));
				bzero(media, sizeof(struct so_media));
				asprintf(&media->label, "%s%03Ld", vtl_prefix, i);
				media->medium_serial_number = serial_number;
				media->name = strdup(media->label);
				media->status = so_media_status_new;
				media->first_used = time(NULL);
				media->use_before = media->first_used + format->life_span;
				media->block_size = format->block_size;
				media->free_block = media->total_block = format->capacity / format->block_size;
				media->append = true;
				media->type = so_media_type_rewritable;
				media->format = format;
			}

			sl->media = media;

			free(link);
			free(media_dir);
			free(media_link);
			free(serial_file);
		}
	}

	db_connection->ops->sync_changer(db_connection, &sochgr_vtl_changer, so_database_sync_init);

	unsigned int i;
	for (i = 0; i < sochgr_vtl_changer.nb_drives; i++) {
		struct so_drive * drive = sochgr_vtl_changer.drives + i;
		struct so_value * vdrive = so_value_list_get(drives, i, false);
		sochgr_drive_register(drive, vdrive, "vtl_drive");
	}

	return 0;

init_error:
	free(vtl_root_dir);

	return 1;
}

static int sochgr_vtl_changer_load(struct so_slot * from, struct so_drive * to, struct so_database_connection * db_connection) {
	sochgr_vtl_changer.status = so_changer_status_loading;
	db_connection->ops->sync_changer(db_connection, &sochgr_vtl_changer, so_database_sync_default);

	struct sochgr_vtl_changer_slot * vtl_from = from->data;
	struct sochgr_vtl_changer_slot * vtl_to = to->slot->data;

	char * sfrom, * sto;
	asprintf(&sfrom, "%s/media", vtl_from->path);
	asprintf(&sto, "%s/media", vtl_to->path);

	int failed = rename(sfrom, sto);

	free(sfrom);
	free(sto);

	if (failed == 0) {
		vtl_to->origin = from;

		struct so_media * media = from->media;
		so_slot_swap(from, to->slot);

		media->load_count++;

		db_connection->ops->sync_media(db_connection, media, so_database_sync_default);

		to->ops->load_media(to, media);
	}

	to->ops->update_status(to);

	sochgr_vtl_changer.status = so_changer_status_idle;
	db_connection->ops->sync_changer(db_connection, &sochgr_vtl_changer, so_database_sync_default);

	return failed;
}

static int sochgr_vtl_changer_put_offline(struct so_database_connection * db_connection) {
	unsigned int i;
	for (i = 0; i < sochgr_vtl_changer.nb_drives; i++) {
		struct so_drive * dr = sochgr_vtl_changer.drives + i;

		while (!dr->ops->is_free(dr))
			sleep(5);

		if (dr->slot->full)
			sochgr_vtl_changer_unload(dr, db_connection);
	}

	for (i = sochgr_vtl_changer.nb_drives; i < sochgr_vtl_changer.nb_slots; i++) {
		struct so_slot * sl = sochgr_vtl_changer.slots + i;
		so_media_free(sl->media);
		sl->media = NULL;
		free(sl->volume_name);
		sl->volume_name = NULL;
		sl->full = false;
	}

	sochgr_vtl_changer.is_online = false;
	sochgr_vtl_changer.next_action = so_changer_action_none;
	db_connection->ops->sync_changer(db_connection, &sochgr_vtl_changer, so_database_sync_default);

	return 0;
}

static int sochgr_vtl_changer_put_online(struct so_database_connection * db_connection) {
	unsigned int i, j;
	for (i = 0, j = sochgr_vtl_changer.nb_drives; j < sochgr_vtl_changer.nb_slots; i++, j++) {
		char * serial_file;
		asprintf(&serial_file, "%s/medias/%s%03u/serial_number", vtl_root_dir, vtl_prefix, i);

		struct so_slot * sl = sochgr_vtl_changer.slots + j;
		asprintf(&sl->volume_name, "%s%03u", vtl_prefix, i);
		sl->full = true;

		char * serial_number = sochgr_vtl_util_get_serial(serial_file);
		sl->media = db_connection->ops->get_media(db_connection, serial_number, NULL, NULL);

		free(serial_number);
		free(serial_file);
	}

	sochgr_vtl_changer.is_online = true;
	sochgr_vtl_changer.next_action = so_changer_action_none;
	db_connection->ops->sync_changer(db_connection, &sochgr_vtl_changer, so_database_sync_default);

	return 0;
}

static int sochgr_vtl_changer_shut_down(struct so_database_connection * db_connection) {
	unsigned int i;
	for (i = 0; i < sochgr_vtl_changer.nb_drives; i++) {
		struct so_drive * dr = sochgr_vtl_changer.drives + i;

		while (!dr->ops->is_free(dr))
			sleep(5);

		if (dr->slot->full)
			sochgr_vtl_changer_unload(dr, db_connection);
	}

	db_connection->ops->sync_changer(db_connection, &sochgr_vtl_changer, so_database_sync_default);

	return 0;
}

static int sochgr_vtl_changer_unload(struct so_drive * from, struct so_database_connection * db_connection) {
	sochgr_vtl_changer.status = so_changer_status_unloading;
	db_connection->ops->sync_changer(db_connection, &sochgr_vtl_changer, so_database_sync_default);

	struct sochgr_vtl_changer_slot * vtl_from = from->slot->data;
	struct sochgr_vtl_changer_slot * vtl_to = vtl_from->origin->data;

	char * sfrom, * sto;
	asprintf(&sfrom, "%s/media", vtl_from->path);
	asprintf(&sto, "%s/media", vtl_to->path);

	int failed = rename(sfrom, sto);

	free(sfrom);
	free(sto);

	if (failed == 0) {
		so_slot_swap(from->slot, vtl_from->origin);

		vtl_from->origin = NULL;

		from->ops->reset(from);
	}

	from->ops->update_status(from);

	sochgr_vtl_changer.status = so_changer_status_idle;
	db_connection->ops->sync_changer(db_connection, &sochgr_vtl_changer, so_database_sync_default);

	return failed;
}

