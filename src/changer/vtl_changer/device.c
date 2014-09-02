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

// NULL
#include <stddef.h>

#include <libstone-changer/changer.h>

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
	.barcode       = false,

	.status      = st_changer_status_unknown,
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

