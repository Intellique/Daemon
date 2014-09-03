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
// access
#include <unistd.h>

#include <libstone/drive.h>
#include <libstone/file.h>
#include <libstone/slot.h>
#include <libstone/value.h>
#include <libstone-changer/changer.h>

#include "device.h"
#include "util.h"

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
	.model         = NULL,
	.vendor        = NULL,
	.revision      = NULL,
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
	char * root_dir;
	long long nb_drives, nb_slots;
	st_value_unpack(config, "{sssisi}", "path", &root_dir, "nb drives", &nb_drives, "nb slots", &nb_slots);

	vtl_changer.model = "Stone vtl changer";
	vtl_changer.vendor = "Intellique";
	vtl_changer.revision = "A01";

	vtl_changer.drives = calloc(nb_drives, sizeof(struct st_drive));
	vtl_changer.nb_drives = nb_drives;

	vtl_changer.nb_slots = nb_drives + nb_slots;
	vtl_changer.slots = calloc(vtl_changer.nb_slots, sizeof(struct st_slot));

	if (access(root_dir, F_OK | R_OK | W_OK | X_OK)) {
	} else {
		if (st_file_mkdir(root_dir, 0700))
			goto init_error;

		char * serial_file;
		asprintf(&serial_file, "%s/serial_number", root_dir);

		vtl_changer.serial_number = vtl_util_get_serial(serial_file);
		free(serial_file);

		long long int i;
		for (i = 0; i < nb_drives; i++) {
			char * drive_dir;
			asprintf(&drive_dir, "%s/drives/%Ld", root_dir, i);
			st_file_mkdir(drive_dir, 0700);
			free(drive_dir);

			asprintf(&serial_file, "%s/drives/%Ld/serial_number", root_dir, i);
			char * serial = vtl_util_get_serial(serial_file);
			free(serial_file);
		}

		for (i = 0; i < nb_slots; i++) {
			char * media_dir;
			asprintf(&media_dir, "%s/medias/%Ld", root_dir, i);
			st_file_mkdir(media_dir, 0700);
			free(media_dir);

			char * slot_dir;
			asprintf(&slot_dir, "%s/slots/%Ld", root_dir, i);
			st_file_mkdir(slot_dir, 0700);
			free(slot_dir);
		}
	}

	return 0;

init_error:
	free(root_dir);

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

