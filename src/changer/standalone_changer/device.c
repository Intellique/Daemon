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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// dngettext
#include <libintl.h>
// malloc
#include <stdlib.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>

#include <libstoriqone/database.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/log.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-changer/changer.h>
#include <libstoriqone-changer/drive.h>
#include <libstoriqone-changer/media.h>

#include "device.h"

static int sochgr_standalone_changer_check(unsigned int nb_clients, struct so_database_connection * db_connection);
static ssize_t sochgr_standalone_changer_get_reserved_space(struct so_media_format * format);
static int sochgr_standalone_changer_init(struct so_value * config, struct so_database_connection * db_connection);
static int sochgr_standalone_changer_load(struct sochgr_peer * peer, struct so_slot * from, struct so_drive * to, struct so_database_connection * db_connection);
static int sochgr_standalone_changer_remain_online(struct so_database_connection * db_connection);
static int sochgr_standalone_changer_shut_down(struct so_database_connection * db_connection);
static int sochgr_standalone_changer_unload(struct sochgr_peer * peer, struct so_drive * from, struct so_database_connection * db_connection);

struct so_changer_ops sochgr_standalone_changer_ops = {
	.check              = sochgr_standalone_changer_check,
	.get_reserved_space =sochgr_standalone_changer_get_reserved_space,
	.init               = sochgr_standalone_changer_init,
	.load               = sochgr_standalone_changer_load,
	.put_offline        = sochgr_standalone_changer_remain_online,
	.put_online         = sochgr_standalone_changer_remain_online,
	.shut_down          = sochgr_standalone_changer_shut_down,
	.unload             = sochgr_standalone_changer_unload,
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


static int sochgr_standalone_changer_check(unsigned int nb_clients __attribute__((unused)), struct so_database_connection * db_connect) {
	struct so_drive * dr = sochgr_standalone_changer.drives;

	if (!dr->enable || !dr->ops->is_free(dr))
		return 0;

	static bool has_media = false;
	if (has_media != !dr->is_empty) {
		struct so_slot * slot = sochgr_standalone_changer.slots;
		if (dr->is_empty) {
			sochgr_media_release(&sochgr_standalone_changer);

			so_media_free(slot->media);
			slot->media = NULL;

			free(slot->volume_name);
			slot->volume_name = NULL;

			slot->full = false;

			has_media = false;
		} else {
			slot->media = db_connect->ops->get_media(db_connect, dr->slot->media->medium_serial_number, NULL, NULL);

			if (slot->media->label != NULL)
				slot->volume_name = strdup(slot->media->label);
			else if (slot->media->name != NULL)
				slot->volume_name = strdup(slot->media->name);
			else if (slot->media->medium_serial_number != NULL)
				slot->volume_name = strdup(slot->media->medium_serial_number);

			slot->full = true;

			has_media = true;

			sochgr_media_init_slot(slot);
		}

		sochgr_standalone_changer.status = dr->is_empty ? so_changer_status_offline : so_changer_status_idle;
		db_connect->ops->sync_changer(db_connect, &sochgr_standalone_changer, so_database_sync_default);
	}

	return 0;
}

struct so_changer * sochgr_standalone_changer_get_device() {
	return &sochgr_standalone_changer;
}

static ssize_t sochgr_standalone_changer_get_reserved_space(struct so_media_format * format) {
	return format->capacity / 100;
}

static int sochgr_standalone_changer_init(struct so_value * config, struct so_database_connection * db_connection) {
	struct so_value * drive = NULL;

	so_value_unpack(config, "{sssssssss[o]sbsbsb}",
		"model", &sochgr_standalone_changer.model,
		"vendor", &sochgr_standalone_changer.vendor,
		"firmwarerev", &sochgr_standalone_changer.revision,
		"serial number", &sochgr_standalone_changer.serial_number,
		"drives", &drive,
		"barcode", &sochgr_standalone_changer.barcode,
		"enable", &sochgr_standalone_changer.enable,
		"is online", &sochgr_standalone_changer.is_online
	);

	if (!sochgr_standalone_changer.enable) {
		so_log_write(so_log_level_critical,
			dgettext("storiqone-changer-standalone", "Critical, standalone drive %s %s (serial: %s) is not enabled"),
			sochgr_standalone_changer.vendor, sochgr_standalone_changer.model, sochgr_standalone_changer.serial_number);
		return 1;
	}

	struct so_drive * dr = sochgr_standalone_changer.drives = malloc(sizeof(struct so_drive));
	bzero(dr, sizeof(struct so_drive));

	struct so_slot * sl = sochgr_standalone_changer.slots = malloc(sizeof(struct so_slot));
	bzero(sl, sizeof(struct so_slot));

	dr->slot = sl;

	sl->changer = &sochgr_standalone_changer;
	sl->index = 0;
	sl->drive = dr;

	struct so_value * vslot = NULL;
	so_value_unpack(drive, "{sssssssssbso}",
		"model", &dr->model,
		"vendor", &dr->vendor,
		"firmware revision", &dr->revision,
		"serial number", &dr->serial_number,
		"enable", &dr->enable,
		"slot", &vslot
	);

	sochgr_drive_register(dr, drive, "tape_drive");

	sochgr_standalone_changer.status = so_changer_status_idle;
	db_connection->ops->sync_changer(db_connection, &sochgr_standalone_changer, so_database_sync_id_only);

	return 0;
}

static int sochgr_standalone_changer_load(struct sochgr_peer * peer __attribute__((unused)), struct so_slot * from __attribute__((unused)), struct so_drive * to __attribute__((unused)), struct so_database_connection * db_connection __attribute__((unused))) {
	return 1;
}

static int sochgr_standalone_changer_remain_online(struct so_database_connection * db_connect) {
	sochgr_standalone_changer.status = so_changer_status_idle;
	sochgr_standalone_changer.next_action = so_changer_action_none;
	db_connect->ops->sync_changer(db_connect, &sochgr_standalone_changer, so_database_sync_default);

	return 0;
}

static int sochgr_standalone_changer_shut_down(struct so_database_connection * db_connection __attribute__((unused))) {
	return 0;
}

static int sochgr_standalone_changer_unload(struct sochgr_peer * peer __attribute__((unused)), struct so_drive * from __attribute__((unused)), struct so_database_connection * db_connection __attribute__((unused))) {
	return 1;
}
