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

// malloc
#include <stdlib.h>
// bzero
#include <strings.h>

#include <libstoriqone/database.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-changer/changer.h>
#include <libstoriqone-changer/drive.h>

#include "device.h"

static int sochgr_standalone_changer_check(struct so_database_connection * db_connection);
static int sochgr_standalone_changer_init(struct so_value * config, struct so_database_connection * db_connection);
static int sochgr_standalone_changer_load(struct so_slot * from, struct so_drive * to, struct so_database_connection * db_connection);
static int sochgr_standalone_changer_put_offline(struct so_database_connection * db_connection);
static int sochgr_standalone_changer_put_online(struct so_database_connection * db_connection);
static int sochgr_standalone_changer_shut_down(struct so_database_connection * db_connection);
static int sochgr_standalone_changer_unload(struct so_drive * from, struct so_database_connection * db_connection);

struct so_changer_ops sochgr_standalone_changer_ops = {
	.check       = sochgr_standalone_changer_check,
	.init        = sochgr_standalone_changer_init,
	.load        = sochgr_standalone_changer_load,
	.put_offline = sochgr_standalone_changer_put_offline,
	.put_online  = sochgr_standalone_changer_put_online,
	.shut_down   = sochgr_standalone_changer_shut_down,
	.unload      = sochgr_standalone_changer_unload,
};

static struct so_changer sochgr_standalone_changer = {
	.model         = NULL,
	.vendor        = NULL,
	.revision      = NULL,
	.serial_number = NULL,
	.wwn           = NULL,
	.barcode       = false,

	.status      = so_changer_status_unknown,
	.next_action = so_changer_action_none,
	.is_online   = true,
	.enable      = true,

	.drives    = NULL,
	.nb_drives = 1,

	.slots    = NULL,
	.nb_slots = 1,

	.ops = &sochgr_standalone_changer_ops,

	.data = NULL,
	.db_data = NULL,
};


static int sochgr_standalone_changer_check(struct so_database_connection * db_connection) {
	return 0;
}

struct so_changer * sochgr_standalone_changer_get_device() {
	return &sochgr_standalone_changer;
}

static int sochgr_standalone_changer_init(struct so_value * config, struct so_database_connection * db_connection) {
	so_value_unpack(config, "{sssssssbsbsb}",
		"model", &sochgr_standalone_changer.model,
		"vendor", &sochgr_standalone_changer.vendor,
		"serial number", &sochgr_standalone_changer.serial_number,
		"barcode", &sochgr_standalone_changer.barcode,
		"enable", &sochgr_standalone_changer.enable,
		"is online", &sochgr_standalone_changer.is_online
	);

	struct so_drive * dr = sochgr_standalone_changer.drives = malloc(sizeof(struct so_drive));
	bzero(dr, sizeof(struct so_drive));

	struct so_slot * sl = sochgr_standalone_changer.slots = malloc(sizeof(struct so_slot));
	bzero(sl, sizeof(struct so_slot));

	dr->slot = sl;

	sl->changer = &sochgr_standalone_changer;
	sl->index = 0;
	sl->drive = dr;

	sochgr_drive_register(dr, config, "tape_drive", 0);

	return 0;
}

static int sochgr_standalone_changer_load(struct so_slot * from __attribute__((unused)), struct so_drive * to __attribute__((unused)), struct so_database_connection * db_connection __attribute__((unused))) {
	return 1;
}

static int sochgr_standalone_changer_put_offline(struct so_database_connection * db_connection) {
	// TODO
	return 0;
}

static int sochgr_standalone_changer_put_online(struct so_database_connection * db_connection) {
	// TODO
	return 0;
}

static int sochgr_standalone_changer_shut_down(struct so_database_connection * db_connection __attribute__((unused))) {
	return 0;
}

static int sochgr_standalone_changer_unload(struct so_drive * from, struct so_database_connection * db_connection) {
	return 0;
}

