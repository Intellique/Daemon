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
// dgettext, dngettext
#include <libintl.h>
// asprintf, rename
#include <stdio.h>
// calloc, free, malloc, realloc
#include <stdlib.h>
// memmove, strdup
#include <string.h>
// bzero
#include <strings.h>
// access, _exit, sleep, symlink
#include <unistd.h>
// time
#include <time.h>

#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-changer/changer.h>
#include <libstoriqone-changer/drive.h>
#include <libstoriqone-changer/media.h>

#include "device.h"
#include "util.h"

static struct sochgr_vtl_drive * sochgr_vtl_drives = NULL;
static struct so_media_format * sochgr_vtl_format = NULL;
static char * sochgr_vtl_prefix = NULL;
static char * sochgr_vtl_root_dir = NULL;

static int sochgr_vtl_changer_check(unsigned int nb_clients, struct so_database_connection * db_connection);
static int sochgr_vtl_changer_init(struct so_value * config, struct so_database_connection * db_connection);
static int sochgr_vtl_changer_load(struct so_slot * from, struct so_drive * to, struct so_database_connection * db_connection);
static int sochgr_vtl_changer_put_offline(struct so_database_connection * db_connection);
static int sochgr_vtl_changer_put_online(struct so_database_connection * db_connection);
static int sochgr_vtl_changer_shut_down(struct so_database_connection * db_connection);
static int sochgr_vtl_changer_unload(struct so_drive * from, struct so_database_connection * db_connection);
static int sochgr_vtl_changer_unload_all_drives(struct so_database_connection * db_connection);

struct so_changer_ops sochgr_vtl_changer_ops = {
	.check       = sochgr_vtl_changer_check,
	.init        = sochgr_vtl_changer_init,
	.load        = sochgr_vtl_changer_load,
	.put_offline = sochgr_vtl_changer_put_offline,
	.put_online  = sochgr_vtl_changer_put_online,
	.shut_down   = sochgr_vtl_changer_shut_down,
	.unload      = sochgr_vtl_changer_unload,
};

static struct so_changer sochgr_vtl_changer = {
	.model         = "Storiq One vtl changer",
	.vendor        = "Intellique",
	.revision      = "B01",
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


static int sochgr_vtl_changer_check(unsigned int nb_clients, struct so_database_connection * db_connection) {
	if (nb_clients > 0)
		return 0;

	struct so_value * vtl_update = db_connection->ops->update_vtl(db_connection, &sochgr_vtl_changer);
	if (vtl_update == NULL)
		return 1;

	unsigned int nb_drives = 0, nb_slots = 0;
	bool deleted = false;

	int nb_parsed = so_value_unpack(vtl_update, "{sususb}",
		"nb slots", &nb_slots,
		"nb drives", &nb_drives,
		"deleted", &deleted
	);
	so_value_free(vtl_update);

	if (nb_parsed < 3)
		return 2;

	if (nb_drives > nb_slots || nb_drives < 1 || nb_slots < nb_drives)
		return 3;

	if (nb_drives != sochgr_vtl_changer.nb_drives || nb_slots != sochgr_vtl_changer.nb_slots - sochgr_vtl_changer.nb_drives || deleted) {
		so_log_write(so_log_level_warning,
			dgettext("storiqone-changer-vtl", "[%s | %s]: will update VTL"),
			sochgr_vtl_changer.vendor, sochgr_vtl_changer.model);

		sochgr_vtl_changer_unload_all_drives(db_connection);

		unsigned int i;
		if (nb_drives < sochgr_vtl_changer.nb_drives) {
			so_log_write(so_log_level_warning,
				dngettext("storiqone-changer-vtl", "[%s | %s]: will remove %u drive", "[%s | %s]: will remove %u drives", sochgr_vtl_changer.nb_drives - nb_drives),
				sochgr_vtl_changer.vendor, sochgr_vtl_changer.model, sochgr_vtl_changer.nb_drives - nb_drives);

			for (i = nb_drives; i < sochgr_vtl_changer.nb_drives; i++) {
				struct so_drive * dr = sochgr_vtl_changer.drives + i;

				dr->ops->stop(dr);

				db_connection->ops->delete_drive(db_connection, dr);

				sochgr_vtl_slot_delete(dr->slot);
				sochgr_vtl_drive_delete(dr);
			}

			void * addr = realloc(sochgr_vtl_changer.drives, nb_drives * sizeof(struct so_drive));
			if (addr != NULL) {
				sochgr_vtl_changer.drives = addr;

				for (i = 0; i < nb_drives; i++) {
					struct so_drive * dr = sochgr_vtl_changer.drives + i;
					dr->slot->drive = dr;
				}
			}

			memmove(sochgr_vtl_changer.slots + nb_drives, sochgr_vtl_changer.slots + sochgr_vtl_changer.nb_drives, (sochgr_vtl_changer.nb_slots - sochgr_vtl_changer.nb_drives) * sizeof(struct so_slot));


			addr = realloc(sochgr_vtl_changer.slots, (nb_drives + nb_slots) * sizeof(struct so_slot));
			if (addr != NULL) {
				sochgr_vtl_changer.slots = addr;

				for (i = 0; i < nb_drives; i++) {
					struct so_drive * dr = sochgr_vtl_changer.drives + i;
					dr->slot = sochgr_vtl_changer.slots + i;
				}

				for (i = nb_drives; i < nb_drives + nb_slots; i++) {
					struct so_slot * sl = sochgr_vtl_changer.slots + i;
					sl->index = i;
				}
			}

			sochgr_vtl_changer.nb_drives = nb_drives;
			sochgr_vtl_changer.nb_slots = nb_slots + nb_drives;
		}

		if (nb_slots < sochgr_vtl_changer.nb_slots - sochgr_vtl_changer.nb_drives) {
			so_log_write(so_log_level_warning,
				dngettext("storiqone-changer-vtl", "[%s | %s]: will remove %u slot", "[%s | %s]: will remove %u slots", sochgr_vtl_changer.nb_slots - sochgr_vtl_changer.nb_drives - nb_slots),
				sochgr_vtl_changer.vendor, sochgr_vtl_changer.model, sochgr_vtl_changer.nb_slots - sochgr_vtl_changer.nb_drives - nb_slots);

			for (i = nb_drives + nb_slots; i < sochgr_vtl_changer.nb_slots; i++) {
				struct so_slot * sl = sochgr_vtl_changer.slots + i;

				so_media_free(sl->media);
				sochgr_vtl_slot_delete(sl);
			}

			void * addr = realloc(sochgr_vtl_changer.slots, (nb_drives + nb_slots) * sizeof(struct so_slot));
			if (addr != NULL) {
				sochgr_vtl_changer.slots = addr;

				for (i = 0; i < nb_drives; i++) {
					struct so_drive * dr = sochgr_vtl_changer.drives + i;
					dr->slot = sochgr_vtl_changer.slots + i;
				}
			}

			sochgr_vtl_changer.nb_slots = sochgr_vtl_changer.nb_drives + nb_slots;
		}

		if (nb_slots > sochgr_vtl_changer.nb_slots - sochgr_vtl_changer.nb_drives) {
			so_log_write(so_log_level_warning,
				dngettext("storiqone-changer-vtl", "[%s | %s]: will add %u slot", "[%s | %s]: will add %u slots", nb_slots - sochgr_vtl_changer.nb_slots + sochgr_vtl_changer.nb_drives),
				sochgr_vtl_changer.vendor, sochgr_vtl_changer.model, nb_slots - sochgr_vtl_changer.nb_slots + sochgr_vtl_changer.nb_drives);

			void * addr = realloc(sochgr_vtl_changer.slots, (sochgr_vtl_changer.nb_drives + nb_slots) * sizeof(struct so_slot));
			if (addr != NULL) {
				sochgr_vtl_changer.slots = addr;

				for (i = 0; i < sochgr_vtl_changer.nb_drives; i++) {
					struct so_drive * dr = sochgr_vtl_changer.drives + i;
					dr->slot = sochgr_vtl_changer.slots + i;
				}

				for (i = sochgr_vtl_changer.nb_slots; i < nb_slots + sochgr_vtl_changer.nb_drives; i++) {
					struct so_slot * sl = sochgr_vtl_changer.slots + i;
					bzero(sl, sizeof(struct so_slot));

					if (!sochgr_vtl_slot_create(sl, sochgr_vtl_root_dir, sochgr_vtl_prefix, i - sochgr_vtl_changer.nb_drives))
						return 1;

					sl->changer = &sochgr_vtl_changer;
					sl->index = i;

					sl->media = sochgr_vtl_media_create(sochgr_vtl_root_dir, sochgr_vtl_prefix, i - sochgr_vtl_changer.nb_drives, sochgr_vtl_format, db_connection);
					if (sl->media == NULL)
						return 1;
				}
			}

			sochgr_vtl_changer.nb_slots = sochgr_vtl_changer.nb_drives + nb_slots;
		}

		if (nb_drives > sochgr_vtl_changer.nb_drives) {
			so_log_write(so_log_level_warning,
				dngettext("storiqone-changer-vtl", "[%s | %s]: will add %u drive", "[%s | %s]: will add %u drives", nb_drives - sochgr_vtl_changer.nb_drives),
				sochgr_vtl_changer.vendor, sochgr_vtl_changer.model, nb_drives - sochgr_vtl_changer.nb_drives);

			void * addr = realloc(sochgr_vtl_drives, nb_drives * sizeof(struct sochgr_vtl_drive));
			if (addr != NULL)
				sochgr_vtl_drives = addr;

			addr = realloc(sochgr_vtl_changer.slots, (nb_drives + sochgr_vtl_changer.nb_slots) * sizeof(struct so_slot));
			if (addr != NULL) {
				sochgr_vtl_changer.slots = addr;

				memmove(sochgr_vtl_changer.slots + nb_drives, sochgr_vtl_changer.slots + sochgr_vtl_changer.nb_drives, (sochgr_vtl_changer.nb_slots - sochgr_vtl_changer.nb_drives) * sizeof(struct so_slot));

				for (i = 0; i < sochgr_vtl_changer.nb_drives; i++) {
					struct so_drive * dr = sochgr_vtl_changer.drives + i;
					dr->slot = sochgr_vtl_changer.slots + i;
				}

				for (i = nb_drives; i < nb_drives + nb_slots; i++) {
					struct so_slot * sl = sochgr_vtl_changer.slots + i;
					sl->index = i;
				}
			}

			addr = realloc(sochgr_vtl_changer.drives, nb_drives * sizeof(struct so_drive));
			if (addr != NULL) {
				sochgr_vtl_changer.drives = addr;

				for (i = sochgr_vtl_changer.nb_drives; i < nb_drives; i++) {
					struct sochgr_vtl_drive * dr_p = sochgr_vtl_drives + i;
					struct so_drive * dr = sochgr_vtl_changer.drives + i;
					bzero(dr_p, sizeof(struct sochgr_vtl_drive));
					bzero(dr, sizeof(struct so_drive));
					dr_p->params = so_value_new_hashtable2();

					if (!sochgr_vtl_drive_create(dr_p, dr, sochgr_vtl_root_dir, i)) {
						sochgr_vtl_changer.status = so_changer_status_error;
						return 1;
					}

					dr->density_code = sochgr_vtl_format->density_code;
					dr->changer = &sochgr_vtl_changer;

					struct so_slot * sl = sochgr_vtl_changer.slots + i;
					bzero(sl, sizeof(struct so_slot));
					sl->changer = &sochgr_vtl_changer;

					dr->slot = sochgr_vtl_changer.slots + i;
					if (!sochgr_vtl_drive_slot_create(dr, sl, sochgr_vtl_root_dir, i))
						return 1;

					so_value_hashtable_put2(dr_p->params, "format", so_media_format_convert(sochgr_vtl_format), false);
					so_value_hashtable_put2(dr_p->params, "serial number", so_value_new_string(dr->serial_number), true);
				}
			}

			for (i = sochgr_vtl_changer.nb_drives; i < nb_drives; i++) {
				struct so_drive * drive = sochgr_vtl_changer.drives + i;
				struct sochgr_vtl_drive * dr_p = sochgr_vtl_drives + i;

				sochgr_drive_register(drive, dr_p->params, "vtl_drive", i);
			}

			sochgr_vtl_changer.nb_drives = nb_drives;
			sochgr_vtl_changer.nb_slots = nb_slots + nb_drives;
		}

		if (deleted) {
			so_log_write(so_log_level_warning,
				dgettext("storiqone-changer-vtl", "[%s | %s]: will delete VTL"),
				sochgr_vtl_changer.vendor, sochgr_vtl_changer.model);

			for (i = 0; i < sochgr_vtl_changer.nb_drives; i++) {
				struct so_drive * dr = sochgr_vtl_changer.drives + i;

				dr->ops->stop(dr);

				db_connection->ops->delete_drive(db_connection, dr);

				sochgr_vtl_slot_delete(dr->slot);
				sochgr_vtl_drive_delete(dr);
			}

			for (i = sochgr_vtl_changer.nb_drives; i < sochgr_vtl_changer.nb_slots; i++) {
				struct so_slot * sl = sochgr_vtl_changer.slots + i;

				so_media_free(sl->media);
				sochgr_vtl_slot_delete(sl);
			}

			db_connection->ops->delete_changer(db_connection, &sochgr_vtl_changer);

			so_file_rm(sochgr_vtl_root_dir);

			struct so_value * command = so_value_pack("{ss}", "command", "exit");
			so_json_encode_to_fd(command, 1, true);
			so_value_free(command);

			_exit(0);
		} else
			db_connection->ops->sync_changer(db_connection, &sochgr_vtl_changer, so_database_sync_init);
	}

	return 0;
}

struct so_changer * sochgr_vtl_changer_get_device() {
	return &sochgr_vtl_changer;
}

static int sochgr_vtl_changer_init(struct so_value * config, struct so_database_connection * db_connection) {
	unsigned int nb_drives, nb_slots;
	struct so_value * vformat = NULL;
	so_value_unpack(config, "{sssususossss}",
		"path", &sochgr_vtl_root_dir,
		"nb drives", &nb_drives,
		"nb slots", &nb_slots,
		"format", &vformat,
		"prefix", &sochgr_vtl_prefix,
		"uuid", &sochgr_vtl_changer.serial_number
	);

	sochgr_vtl_format = malloc(sizeof(struct so_media_format));
	bzero(sochgr_vtl_format, sizeof(struct so_media_format));
	so_media_format_sync(sochgr_vtl_format, vformat);

	sochgr_vtl_drives = calloc(nb_drives, sizeof(struct sochgr_vtl_drive));
	sochgr_vtl_changer.drives = calloc(nb_drives, sizeof(struct so_drive));
	sochgr_vtl_changer.nb_drives = nb_drives;

	sochgr_vtl_changer.nb_slots = nb_drives + nb_slots;
	sochgr_vtl_changer.slots = calloc(sochgr_vtl_changer.nb_slots, sizeof(struct so_slot));

	if (so_file_mkdir(sochgr_vtl_root_dir, 0700))
		goto init_error;

	unsigned int i;
	for (i = 0; i < nb_drives; i++) {
		struct sochgr_vtl_drive * dr_p = sochgr_vtl_drives + i;
		struct so_drive * dr = sochgr_vtl_changer.drives + i;
		dr_p->params = so_value_new_hashtable2();

		if (!sochgr_vtl_drive_create(dr_p, dr, sochgr_vtl_root_dir, i))
			goto init_error;

		dr->density_code = sochgr_vtl_format->density_code;
		dr->changer = &sochgr_vtl_changer;

		struct so_slot * sl = sochgr_vtl_changer.slots + i;
		sl->changer = &sochgr_vtl_changer;

		if (!sochgr_vtl_drive_slot_create(dr, sl, sochgr_vtl_root_dir, i))
			goto init_error;

		so_value_hashtable_put2(dr_p->params, "index", so_value_new_integer(i), true);
		so_value_hashtable_put2(dr_p->params, "format", vformat, false);
		so_value_hashtable_put2(dr_p->params, "serial number", so_value_new_string(dr->serial_number), true);

		char * media = NULL;
		int size = asprintf(&media, "%s/drives/%u/media", sochgr_vtl_root_dir, i);
		if (size < 0)
			goto init_error;

		so_file_rm(media);
		free(media);
	}

	for (i = 0; i < nb_slots; i++) {
		struct so_slot * sl = sochgr_vtl_changer.slots + nb_drives + i;
		if (!sochgr_vtl_slot_create(sl, sochgr_vtl_root_dir, sochgr_vtl_prefix, i))
			goto init_error;

		sl->changer = &sochgr_vtl_changer;
		sl->index = nb_drives + i;

		sl->media = sochgr_vtl_media_create(sochgr_vtl_root_dir, sochgr_vtl_prefix, i, sochgr_vtl_format, db_connection);
		if (sl->media == NULL)
			goto init_error;
	}

	db_connection->ops->sync_changer(db_connection, &sochgr_vtl_changer, so_database_sync_init);

	for (i = 0; i < sochgr_vtl_changer.nb_drives; i++) {
		struct so_drive * drive = sochgr_vtl_changer.drives + i;
		struct sochgr_vtl_drive * dr_p = sochgr_vtl_drives + i;

		sochgr_drive_register(drive, dr_p->params, "vtl_drive", i);
	}

	return 0;

init_error:
	free(sochgr_vtl_root_dir);

	return 1;
}

static int sochgr_vtl_changer_load(struct so_slot * from, struct so_drive * to, struct so_database_connection * db_connection) {
	sochgr_vtl_changer.status = so_changer_status_loading;
	db_connection->ops->sync_changer(db_connection, &sochgr_vtl_changer, so_database_sync_default);

	struct sochgr_vtl_changer_slot * vtl_from = from->data;
	struct sochgr_vtl_changer_slot * vtl_to = to->slot->data;

	char * sfrom = NULL, * sto = NULL;
	int size = asprintf(&sfrom, "%s/media", vtl_from->path);
	if (size < 0)
		goto move_error;

	size = asprintf(&sto, "%s/media", vtl_to->path);
	if (size < 0)
		goto move_error;

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

move_error:
	free(sfrom);
	free(sto);

	return 1;
}

static int sochgr_vtl_changer_put_offline(struct so_database_connection * db_connect) {
	unsigned int i;

	sochgr_vtl_changer.status = so_changer_status_go_offline;
	db_connect->ops->sync_changer(db_connect, &sochgr_vtl_changer, so_database_sync_default);

retry:
	for (i = 0; i < sochgr_vtl_changer.nb_drives; i++) {
		struct so_drive * dr = sochgr_vtl_changer.drives + i;

		if (!dr->ops->is_free(dr)) {
			so_poll(12000);
			db_connect->ops->sync_changer(db_connect, &sochgr_vtl_changer, so_database_sync_default);
			goto retry;
		}
	}

	sochgr_vtl_changer_unload_all_drives(db_connect);

	sochgr_vtl_changer.status = so_changer_status_go_offline;
	db_connect->ops->sync_changer(db_connect, &sochgr_vtl_changer, so_database_sync_default);

	for (i = sochgr_vtl_changer.nb_drives; i < sochgr_vtl_changer.nb_slots; i++) {
		struct so_slot * sl = sochgr_vtl_changer.slots + i;
		so_media_free(sl->media);
		sl->media = NULL;
		free(sl->volume_name);
		sl->volume_name = NULL;
		sl->full = false;
	}

	sochgr_vtl_changer.status = so_changer_status_offline;
	sochgr_vtl_changer.is_online = false;
	sochgr_vtl_changer.next_action = so_changer_action_none;
	db_connect->ops->sync_changer(db_connect, &sochgr_vtl_changer, so_database_sync_default);

	return 0;
}

static int sochgr_vtl_changer_put_online(struct so_database_connection * db_connection) {
	sochgr_vtl_changer.status = so_changer_status_go_online;
	db_connection->ops->sync_changer(db_connection, &sochgr_vtl_changer, so_database_sync_default);

	unsigned int i, j;
	for (i = 0, j = sochgr_vtl_changer.nb_drives; j < sochgr_vtl_changer.nb_slots; i++, j++) {
		char * serial_file;
		int size = asprintf(&serial_file, "%s/medias/%s%03u/serial_number", sochgr_vtl_root_dir, sochgr_vtl_prefix, i);

		if (size < 0) {
			sochgr_vtl_changer.status = so_changer_status_error;
			db_connection->ops->sync_changer(db_connection, &sochgr_vtl_changer, so_database_sync_default);
			return 2;
		}

		struct so_slot * sl = sochgr_vtl_changer.slots + j;
		size = asprintf(&sl->volume_name, "%s%03u", sochgr_vtl_prefix, i);
		sl->full = true;

		if (size < 0) {
			sochgr_vtl_changer.status = so_changer_status_error;
			db_connection->ops->sync_changer(db_connection, &sochgr_vtl_changer, so_database_sync_default);
			return 2;
		}

		char * serial_number = sochgr_vtl_util_get_serial(serial_file);
		sl->media = db_connection->ops->get_media(db_connection, serial_number, NULL, NULL);

		free(serial_number);
		free(serial_file);
	}

	sochgr_vtl_changer.status = so_changer_status_idle;
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

	char * sfrom = NULL, * sto = NULL;
	int size = asprintf(&sfrom, "%s/media", vtl_from->path);
	if (size < 0)
		goto move_error;

	size = asprintf(&sto, "%s/media", vtl_to->path);
	if (size < 0)
		goto move_error;

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

move_error:
	free(sfrom);
	free(sto);

	return 1;
}

static int sochgr_vtl_changer_unload_all_drives(struct so_database_connection * db_connection) {
	unsigned int i;
	for (i = 0; i < sochgr_vtl_changer.nb_drives; i++) {
		struct so_drive * dr = sochgr_vtl_changer.drives + i;

		while (!dr->ops->is_free(dr))
			sleep(5);

		if (dr->slot->full) {
			int failed = sochgr_vtl_changer_unload(dr, db_connection);
			if (failed != 0)
				return failed;
		}
	}

	return 0;
}

