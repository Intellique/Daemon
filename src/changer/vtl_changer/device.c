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
\****************************************************************************/

#define _GNU_SOURCE
// asprintf
#include <stdio.h>
// free
#include <stdlib.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>
// access, symlink
#include <unistd.h>
// time
#include <time.h>

#include <libstone/file.h>
#include <libstone/slot.h>
#include <libstone/value.h>
#include <libstone-changer/changer.h>
#include <libstone-changer/drive.h>

#include "device.h"
#include "util.h"

static char * vtl_root_dir = NULL;

static int vtl_changer_init(struct st_value * config, struct st_database_connection * db_connection);
static int vtl_changer_load(struct st_slot * from, struct st_drive * to, struct st_database_connection * db_connection);
static int vtl_changer_put_offline(struct st_database_connection * db_connection);
static int vtl_changer_put_online(struct st_database_connection * db_connection);
static int vtl_changer_shut_down(struct st_database_connection * db_connection);
static int vtl_changer_unload(struct st_drive * from, struct st_database_connection * db_connection);

struct st_changer_ops vtl_changer_ops = {
	.init        = vtl_changer_init,
	.load        = vtl_changer_load,
	.put_offline = vtl_changer_put_offline,
	.put_online  = vtl_changer_put_online,
	.shut_down   = vtl_changer_shut_down,
	.unload      = vtl_changer_unload,
};

static struct st_changer vtl_changer = {
	.model         = "Stone vtl changer",
	.vendor        = "Intellique",
	.revision      = "A01",
	.serial_number = NULL,
	.wwn           = NULL,
	.barcode       = true,

	.status      = st_changer_status_idle,
	.next_action = st_changer_action_none,
	.is_online   = true,
	.enable      = true,

	.drives    = NULL,
	.nb_drives = 0,

	.slots    = NULL,
	.nb_slots = 0,

	.ops = &vtl_changer_ops,

	.data = NULL,
	.db_data = NULL,
};


struct st_changer * vtl_changer_get_device() {
	return &vtl_changer;
}

static int vtl_changer_init(struct st_value * config, struct st_database_connection * db_connection) {
	long long nb_drives, nb_slots;
	struct st_value * drives = NULL, * vformat = NULL;
	char * prefix;
	st_value_unpack(config, "{sssisisososs}", "path", &vtl_root_dir, "nb drives", &nb_drives, "nb slots", &nb_slots, "drives", &drives, "format", &vformat, "prefix", &prefix);

	struct st_media_format * format = malloc(sizeof(struct st_media_format));
	bzero(format, sizeof(struct st_media_format));
	st_media_format_sync(format, vformat);

	vtl_changer.drives = calloc(nb_drives, sizeof(struct st_drive));
	vtl_changer.nb_drives = nb_drives;

	vtl_changer.nb_slots = nb_drives + nb_slots;
	vtl_changer.slots = calloc(vtl_changer.nb_slots, sizeof(struct st_slot));

	if (access(vtl_root_dir, F_OK | R_OK | W_OK | X_OK) == 0) {
		char * serial_file;
		asprintf(&serial_file, "%s/serial_number", vtl_root_dir);

		vtl_changer.serial_number = st_file_read_all_from(serial_file);
		free(serial_file);

		long long int i;
		for (i = 0; i < nb_drives; i++) {
			char * drive_dir;
			asprintf(&drive_dir, "%s/drives/%Ld", vtl_root_dir, i);
			asprintf(&serial_file, "%s/drives/%Ld/serial_number", vtl_root_dir, i);

			struct st_drive * drive = vtl_changer.drives + i;
			drive->model = "Stone vtl drive";
			drive->vendor = "Intellique";
			drive->revision = "A01";
			drive->serial_number = st_file_read_all_from(serial_file);

			drive->status = st_drive_status_unknown;
			drive->enable = true;

			drive->density_code = format->density_code;
			drive->mode = st_media_format_mode_disk;

			drive->changer = &vtl_changer;
			drive->index = i;

			struct st_slot * sl = vtl_changer.slots + i;
			sl->changer = &vtl_changer;
			sl->index = i;
			sl->drive = drive;
			drive->slot = sl;

			sl->full = false;
			sl->enable = true;

			struct st_value * vdrive = st_value_list_get(drives, i, false);
			st_value_hashtable_put2(vdrive, "device", st_value_new_string(drive_dir), true);
			st_value_hashtable_put2(vdrive, "serial number", st_value_new_string(drive->serial_number), true);

			char * media_link;
			asprintf(&media_link, "%s/drives/%Ld/media", vtl_root_dir, i);
			st_file_rm(media_link);

			free(drive_dir);
			free(serial_file);
			free(media_link);

			// stchgr_drive_register(drive, vdrive, "vtl_drive");
		}

		for (i = 0; i < nb_slots; i++) {
			char * serial_file;
			asprintf(&serial_file, "%s/medias/%s%03Ld/serial_number", vtl_root_dir, prefix, i);

			struct st_slot * sl = vtl_changer.slots + nb_drives + i;
			sl->changer = &vtl_changer;
			sl->index = nb_drives + i;

			struct vtl_changer_slot * vtl_sl = sl->data = malloc(sizeof(struct vtl_changer_slot));
			bzero(vtl_sl, sizeof(struct vtl_changer_slot));
			asprintf(&vtl_sl->path, "%s/slots/%Ld", vtl_root_dir, i);

			char * media_link, * link;
			asprintf(&media_link, "../../medias/%s%03Ld", prefix, i);
			asprintf(&link, "%s/slots/%Ld/media", vtl_root_dir, i);
			st_file_rm(link);
			symlink(media_link, link);

			struct st_media * media = sl->media = malloc(sizeof(struct st_media));
			bzero(media, sizeof(struct st_media));
			asprintf(&media->label, "%s%03Ld", prefix, i);
			media->medium_serial_number = vtl_util_get_serial(serial_file);
			media->name = strdup(media->label);
			media->status = st_media_status_new;
			media->first_used = time(NULL);
			media->block_size = format->block_size;
			media->free_block = media->total_block = format->capacity / format->block_size;
			media->append = true;
			media->type = st_media_type_rewritable;
			media->format = format;

			free(link);
			free(media_link);
			free(serial_file);
		}
	} else {
		if (st_file_mkdir(vtl_root_dir, 0700))
			goto init_error;

		char * serial_file;
		asprintf(&serial_file, "%s/serial_number", vtl_root_dir);

		vtl_changer.serial_number = vtl_util_get_serial(serial_file);
		free(serial_file);

		long long int i;
		for (i = 0; i < nb_drives; i++) {
			char * drive_dir;
			asprintf(&drive_dir, "%s/drives/%Ld", vtl_root_dir, i);
			st_file_mkdir(drive_dir, 0700);

			asprintf(&serial_file, "%s/drives/%Ld/serial_number", vtl_root_dir, i);

			struct st_drive * drive = vtl_changer.drives + i;
			drive->model = "Stone vtl drive";
			drive->vendor = "Intellique";
			drive->revision = "A01";
			drive->serial_number = vtl_util_get_serial(serial_file);

			drive->status = st_drive_status_unknown;
			drive->enable = true;

			drive->density_code = format->density_code;
			drive->mode = st_media_format_mode_disk;

			drive->changer = &vtl_changer;
			drive->index = i;

			struct st_slot * sl = vtl_changer.slots + i;
			sl->changer = &vtl_changer;
			sl->index = i;
			sl->drive = drive;
			drive->slot = sl;

			sl->full = false;
			sl->enable = true;

			struct st_value * vdrive = st_value_list_get(drives, i, false);
			st_value_hashtable_put2(vdrive, "device", st_value_new_string(drive_dir), true);
			st_value_hashtable_put2(vdrive, "serial number", st_value_new_string(drive->serial_number), true);

			free(drive_dir);
			free(serial_file);

			// stchgr_drive_register(drive, vdrive, "vtl_drive");
		}

		for (i = 0; i < nb_slots; i++) {
			char * media_dir;
			asprintf(&media_dir, "%s/medias/%s%03Ld", vtl_root_dir, prefix, i);
			st_file_mkdir(media_dir, 0700);

			char * serial_file;
			asprintf(&serial_file, "%s/medias/%s%03Ld/serial_number", vtl_root_dir, prefix, i);

			struct st_slot * sl = vtl_changer.slots + nb_drives + i;
			sl->changer = &vtl_changer;
			sl->index = nb_drives + i;

			struct vtl_changer_slot * vtl_sl = sl->data = malloc(sizeof(struct vtl_changer_slot));
			bzero(vtl_sl, sizeof(struct vtl_changer_slot));
			asprintf(&vtl_sl->path, "%s/slots/%Ld", vtl_root_dir, i);
			st_file_mkdir(vtl_sl->path, 0700);

			char * media_link, * link;
			asprintf(&media_link, "../../medias/%s%03Ld", prefix, i);
			asprintf(&link, "%s/slots/%Ld/media", vtl_root_dir, i);
			symlink(media_link, link);

			struct st_media * media = sl->media = malloc(sizeof(struct st_media));
			bzero(media, sizeof(struct st_media));
			asprintf(&media->label, "%s%03Ld", prefix, i);
			media->medium_serial_number = vtl_util_get_serial(serial_file);
			media->name = strdup(media->label);
			media->status = st_media_status_new;
			media->first_used = time(NULL);
			media->block_size = format->block_size;
			media->free_block = media->total_block = format->capacity / format->block_size;
			media->append = true;
			media->type = st_media_type_rewritable;
			media->format = format;

			free(link);
			free(media_dir);
			free(media_link);
			free(serial_file);
		}
	}

	return 0;

init_error:
	free(vtl_root_dir);

	return 1;
}

static int vtl_changer_load(struct st_slot * from, struct st_drive * to, struct st_database_connection * db_connection) {
}

static int vtl_changer_put_offline(struct st_database_connection * db_connection) {
}

static int vtl_changer_put_online(struct st_database_connection * db_connection) {
}

static int vtl_changer_shut_down(struct st_database_connection * db_connection) {
}

static int vtl_changer_unload(struct st_drive * from, struct st_database_connection * db_connection) {
}

