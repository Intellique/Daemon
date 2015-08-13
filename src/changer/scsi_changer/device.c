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
// dngettext
#include <libintl.h>
// glob, globfree
#include <glob.h>
// pthread_mutex_t
#include <pthread.h>
// printf, sscanf, snprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// memset, strcat, strdup, strchr, strcpy, strdup, strncpy, strstr
#include <string.h>
// stat
#include <sys/stat.h>
// stat
#include <sys/types.h>
// readlink, sleep, stat
#include <unistd.h>
// clock_gettime
#include <time.h>

#include <libstoriqone/database.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/file.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/thread_pool.h>
#include <libstoriqone/value.h>
#include <libstoriqone-changer/changer.h>
#include <libstoriqone-changer/drive.h>
#include <libstoriqone-changer/media.h>

#include "device.h"
#include "scsi.h"

static int sochgr_scsi_changer_check(unsigned int nb_clients, struct so_database_connection * db_connection);
static int sochgr_scsi_changer_init(struct so_value * config, struct so_database_connection * db_connection);
static void sochgr_scsi_changer_init_worker(void * arg);
static int sochgr_scsi_changer_load(struct so_slot * from, struct so_drive * to, struct so_database_connection * db_connection);
static int sochgr_scsi_changer_load_inner(struct so_slot * from, struct so_drive * to, bool reset_drive, struct so_database_connection * db_connection);
static int sochgr_scsi_changer_parse_media(struct so_database_connection * db_connection);
static int sochgr_scsi_changer_put_offline(struct so_database_connection * db_connection);
static int sochgr_scsi_changer_put_online(struct so_database_connection * db_connection);
static int sochgr_scsi_changer_shut_down(struct so_database_connection * db_connection);
static int sochgr_scsi_changer_unload(struct so_drive * from, struct so_database_connection * db_connection);
static void sochgr_scsi_changer_wait(void);

static char * sochgr_scsi_changer_device = NULL;
static int sochgr_scsi_changer_transport_address = -1;
static pthread_mutex_t sochgr_scsi_changer_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile unsigned int sochgr_scsi_changer_nb_worker = 0;

struct so_changer_ops sochgr_scsi_changer_ops = {
	.check       = sochgr_scsi_changer_check,
	.init        = sochgr_scsi_changer_init,
	.load        = sochgr_scsi_changer_load,
	.put_offline = sochgr_scsi_changer_put_offline,
	.put_online  = sochgr_scsi_changer_put_online,
	.shut_down   = sochgr_scsi_changer_shut_down,
	.unload      = sochgr_scsi_changer_unload,
};

static struct so_changer sochgr_scsi_changer = {
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
	.nb_drives = 0,

	.slots    = NULL,
	.nb_slots = 0,

	.ops = &sochgr_scsi_changer_ops,

	.data = NULL,
	.db_data = NULL,
};


static int sochgr_scsi_changer_check(unsigned int nb_clients, struct so_database_connection * db_connection) {
	if (nb_clients > 0)
		return 0;

	static struct timespec last_call = { 0, 0 };
	static struct timespec last_check = { 0, 0 };

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC_COARSE, &now);

	if (last_check.tv_sec == 0 || last_call.tv_sec + 120 < now.tv_sec)
		last_check = now;
	else if (last_check.tv_sec + 1800 < now.tv_sec) {
		unsigned int i;
		for (i = 0; i < sochgr_scsi_changer.nb_drives; i++) {
			struct so_drive * dr = sochgr_scsi_changer.drives + i;

			if (!dr->enable)
				continue;

			dr->ops->update_status(dr);

			if (!dr->is_empty) {
				char * volume_name = strdup(dr->slot->media->name);

				long long diff = (now.tv_sec - last_check.tv_sec) / 60;

				so_log_write(so_log_level_notice,
					dngettext("storiqone-changer-scsi",
						"[%s | %s]: unloading media '%s' from drive #%d because media hasn't been used for %lld minute",
						"[%s | %s]: unloading media '%s' from drive #%d because media hasn't been used for %lld minutes",
						diff),
					sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, volume_name, dr->index, diff);

				int failed = sochgr_scsi_changer_unload(dr, db_connection);
				if (failed == 0)
					so_log_write(so_log_level_notice,
						dgettext("storiqone-changer-scsi", "[%s | %s]: unloading media '%s' from drive #%d completed with code = OK"),
						sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, volume_name, dr->index);
				else
					so_log_write(so_log_level_error,
						dgettext("storiqone-changer-scsi", "[%s | %s]: unloading media '%s' from drive #%d completed with code = %d"),
						sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, volume_name, dr->index, failed);

				free(volume_name);
			}
		}

		last_check = now;
	}

	last_call = now;

	return 0;
}

struct so_changer * sochgr_scsi_changer_get_device() {
	return &sochgr_scsi_changer;
}

static int sochgr_scsi_changer_init(struct so_value * config, struct so_database_connection * db_connection) {
	so_value_unpack(config, "{sssssssbsbsbss}",
		"model", &sochgr_scsi_changer.model,
		"vendor", &sochgr_scsi_changer.vendor,
		"serial number", &sochgr_scsi_changer.serial_number,
		"barcode", &sochgr_scsi_changer.barcode,
		"enable", &sochgr_scsi_changer.enable,
		"is online", &sochgr_scsi_changer.is_online,
		"wwn", &sochgr_scsi_changer.wwn
	);

	glob_t gl;
	glob("/sys/class/scsi_changer/*/device", 0, NULL, &gl);

	unsigned int i;
	bool found = false, has_wwn = sochgr_scsi_changer.wwn != NULL;
	for (i = 0; i < gl.gl_pathc && !found; i++) {
		char link[256];
		ssize_t length = readlink(gl.gl_pathv[i], link, 256);
		if (length < 0) {
			so_log_write(so_log_level_error,
				dgettext("storiqone-changer-scsi", "Failed while reading link of file '%s' because %m"),
				gl.gl_pathv[i]);
			continue;
		}

		link[length] = '\0';

		char * ptr = strrchr(link, '/') + 1;
		int host = 0, target = 0, channel = 0, bus = 0;
		sscanf(ptr, "%d:%d:%d:%d", &host, &target, &channel, &bus);

		char * path = NULL;
		int size = asprintf(&path, "%s/generic", gl.gl_pathv[i]);
		if (size < 0)
			continue;

		length = readlink(path, link, 256);
		if (length < 0) {
			so_log_write(so_log_level_error,
				dgettext("storiqone-changer-scsi", "Failed while reading link of file '%s' because %m"),
				path);
			continue;
		}

		link[length] = '\0';
		free(path);

		char * device;
		ptr = strrchr(link, '/');
		size = asprintf(&device, "/dev%s", ptr);
		if (size < 0)
			continue;

		size = asprintf(&path, "/sys/class/sas_host/host%d", host);
		if (size < 0) {
			free(device);
			continue;
		}

		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			char * resolved_path = realpath(gl.gl_pathv[i], link);
			if (resolved_path == NULL) {
				so_log_write(so_log_level_error,
					dgettext("storiqone-changer-scsi", "Failed while resolving path of file '%s' because %m"),
					gl.gl_pathv[i]);
				continue;
			}

			ptr = strstr(link, "end_device-");
			if (ptr != NULL) {
				char * cp = strchr(ptr, '/');
				if (cp != NULL)
					*cp = '\0';

				free(path);
				size = asprintf(&path, "/sys/class/sas_device/%s/sas_address", ptr);
				if (size < 0)
					continue;

				char * data = so_file_read_all_from(path);
				if (data == NULL)
					continue;

				cp = strchr(data, '\n');
				if (cp != NULL)
					*cp = '\0';

				char * wwn = NULL;
				size = asprintf(&wwn, "sas:%s", data);
				if (size < 0) {
					free(data);
					continue;
				}

				if (has_wwn && !strcmp(wwn, sochgr_scsi_changer.wwn))
					found = true;
				else if (!has_wwn) {
					free(sochgr_scsi_changer.wwn);
					sochgr_scsi_changer.wwn = wwn;
				}

				if (has_wwn)
					free(wwn);
				free(data);
			}
		}
		free(path);

		if (found)
			sochgr_scsi_changer_scsi_check_changer(&sochgr_scsi_changer, device);
		else
			found = sochgr_scsi_changer_scsi_check_changer(&sochgr_scsi_changer, device);

		if (found) {
			sochgr_scsi_changer_device = device;
			so_log_write(so_log_level_info,
				dgettext("storiqone-changer-scsi", "Found scsi changer %s %s, serial: %s"),
				sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, sochgr_scsi_changer.serial_number);
		} else
			free(device);
	}
	globfree(&gl);

	struct so_value * available_drives = so_value_new_hashtable2();
	struct so_value * drives = NULL;
	so_value_unpack(config, "{so}", "drives", &drives);

	if (drives != NULL) {
		struct so_value_iterator * iter = so_value_list_get_iterator(drives);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * dr = so_value_iterator_get_value(iter, true);

			char * vendor = NULL, * model = NULL, * serial_number = NULL;
			so_value_unpack(dr, "{ssssss}",
				"vendor", &vendor,
				"model", &model,
				"serial number", &serial_number
			);

			char dev[35];
			memset(dev, ' ', 34);
			dev[34] = '\0';

			strncpy(dev, vendor, strlen(vendor));
			dev[7] = ' ';
			strncpy(dev + 8, model, strlen(model));
			dev[23] = ' ';
			strcpy(dev + 24, serial_number);

			so_value_hashtable_put2(available_drives, dev, dr, false);

			free(vendor);
			free(model);
			free(serial_number);
		}
		so_value_iterator_free(iter);
	}

	if (found && sochgr_scsi_changer.enable && sochgr_scsi_changer.is_online) {
		sochgr_scsi_changer_scsi_medium_removal(sochgr_scsi_changer_device, false);
		sochgr_scsi_changer_scsi_new_status(&sochgr_scsi_changer, sochgr_scsi_changer_device, available_drives, &sochgr_scsi_changer_transport_address);
	}

	so_value_free(available_drives);

	if (found) {
		sochgr_scsi_changer.status = so_changer_status_idle;

		db_connection->ops->sync_changer(db_connection, &sochgr_scsi_changer, so_database_sync_id_only);

		int failed = sochgr_scsi_changer_parse_media(db_connection);
		if (failed != 0)
			return failed;
	}

	return found ? 0 : 1;
}

static void sochgr_scsi_changer_init_worker(void * arg) {
	struct so_drive * dr = arg;

	so_log_write(so_log_level_info,
		dgettext("storiqone-changer-scsi", "[%s | %s]: starting check media with drive #%u"),
		sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, dr->index);

	pthread_mutex_lock(&sochgr_scsi_changer_lock);
	sochgr_scsi_changer_nb_worker++;
	pthread_mutex_unlock(&sochgr_scsi_changer_lock);

	// check if drive has media
	int failed = dr->ops->update_status(dr);
	if (failed != 0) {
		so_log_write(so_log_level_critical,
			dgettext("storiqone-changer-scsi", "[%s | %s]: failed to get status from drive #%u %s %s"),
			sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, dr->index, dr->vendor, dr->model);

		pthread_mutex_lock(&sochgr_scsi_changer_lock);
		sochgr_scsi_changer_nb_worker--;
		pthread_mutex_unlock(&sochgr_scsi_changer_lock);

		so_log_write(so_log_level_warning,
			dgettext("storiqone-changer-scsi", "[%s | %s]: checking media with drive #%u finished"),
			sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, dr->index);

		return;
	}

	pthread_mutex_lock(&sochgr_scsi_changer_lock);
	if (dr->slot->media != NULL)
		failed = sochgr_scsi_changer_unload(dr, NULL);

	unsigned int i;
	for (i = sochgr_scsi_changer.nb_drives; i < sochgr_scsi_changer.nb_slots; i++) {
		struct so_slot * sl = sochgr_scsi_changer.slots + i;

		if (!sl->enable || !sl->full || sl->media != NULL)
			continue;

		char * volume_name = strdup(sl->volume_name);
		so_log_write(so_log_level_notice,
			dgettext("storiqone-changer-scsi", "[%s | %s]: loading media '%s' from slot #%u to drive #%u"),
			sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, volume_name, sl->index, dr->index);

		failed = sochgr_scsi_changer_load_inner(sl, dr, false, NULL);

		pthread_mutex_unlock(&sochgr_scsi_changer_lock);

		if (failed == 0)
			so_log_write(so_log_level_notice,
				dgettext("storiqone-changer-scsi", "[%s | %s]: loading media '%s' from slot #%u to drive #%u completed with code = OK"),
				sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, volume_name, sl->index, dr->index);
		else
			so_log_write(so_log_level_error,
				dgettext("storiqone-changer-scsi", "[%s | %s]: loading media '%s' from slot #%u to drive #%u completed with code = %d"),
				sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, volume_name, sl->index, failed, dr->index);

		// reset drive
		failed = dr->ops->reset(dr);
		if (failed != 0) {
			so_log_write(so_log_level_critical,
				dgettext("storiqone-changer-scsi", "[%s | %s]: failed to reset drive #%u %s %s"),
				sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, dr->index, dr->vendor, dr->model);

			pthread_mutex_lock(&sochgr_scsi_changer_lock);
			sochgr_scsi_changer_nb_worker--;
			pthread_mutex_unlock(&sochgr_scsi_changer_lock);

			so_log_write(so_log_level_warning,
				dgettext("storiqone-changer-scsi", "[%s | %s]: checking media with drive #%u finished"),
				sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, dr->index);

			free(volume_name);
			return;
		}

		// get drive status
		failed = dr->ops->update_status(dr);
		if (failed != 0) {
			so_log_write(so_log_level_critical,
				dgettext("storiqone-changer-scsi", "[%s | %s]: failed to get status from drive #%u %s %s"),
				sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, dr->index, dr->vendor, dr->model);

			pthread_mutex_lock(&sochgr_scsi_changer_lock);
			sochgr_scsi_changer_nb_worker--;
			pthread_mutex_unlock(&sochgr_scsi_changer_lock);

			so_log_write(so_log_level_warning,
				dgettext("storiqone-changer-scsi", "[%s | %s]: checking media with drive #%u finished"),
				sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, dr->index);

			free(volume_name);
			return;
		}

		so_log_write(so_log_level_notice,
			dgettext("storiqone-changer-scsi", "[%s | %s]: unloading media '%s' from drive #%u to slot #%u"),
			sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, volume_name, dr->index, sl->index);

		pthread_mutex_lock(&sochgr_scsi_changer_lock);

		failed = sochgr_scsi_changer_unload(dr, NULL);
		if (failed == 0)
			so_log_write(so_log_level_notice,
				dgettext("storiqone-changer-scsi", "[%s | %s]: unloading media '%s' from drive #%u to slot #%u completed with code = OK"),
				sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, volume_name, dr->index, sl->index);
		else
			so_log_write(so_log_level_error,
				dgettext("storiqone-changer-scsi", "[%s | %s]: unloading media '%s' from drive #%u to slot #%u completed with code = %d"),
				sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, volume_name, dr->index, sl->index, failed);

		free(volume_name);
	}

	sochgr_scsi_changer_nb_worker--;
	pthread_mutex_unlock(&sochgr_scsi_changer_lock);

	so_log_write(so_log_level_info,
		dgettext("storiqone-changer-scsi", "[%s | %s]: checking media with drive #%u completed"),
		sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, dr->index);
}

static int sochgr_scsi_changer_load(struct so_slot * from, struct so_drive * to, struct so_database_connection * db_connection) {
	return sochgr_scsi_changer_load_inner(from, to, true, db_connection);
}

static int sochgr_scsi_changer_load_inner(struct so_slot * from, struct so_drive * to, bool reset_drive, struct so_database_connection * db_connection) {
	sochgr_scsi_changer_wait();

	sochgr_scsi_changer.status = so_changer_status_loading;
	if (db_connection != NULL)
		db_connection->ops->sync_changer(db_connection, &sochgr_scsi_changer, so_database_sync_default);

	int failed = sochgr_scsi_changer_scsi_move(sochgr_scsi_changer_device, sochgr_scsi_changer_transport_address, from, to->slot);
	if (failed != 0) {
		sochgr_scsi_changer_wait();

		struct so_slot tmp_from = *from;
		sochgr_scsi_changer_scsi_loader_check_slot(&sochgr_scsi_changer, sochgr_scsi_changer_device, &tmp_from);

		struct so_slot tmp_to = *to->slot;
		sochgr_scsi_changer_scsi_loader_check_slot(&sochgr_scsi_changer, sochgr_scsi_changer_device, &tmp_to);

		failed = tmp_from.full || !tmp_to.full;
	}

	if (failed == 0) {
		struct so_media * media = to->slot->media = from->media;
		from->media = NULL;

		to->slot->volume_name = from->volume_name;
		from->volume_name = NULL;

		from->full = false;
		to->slot->full = true;

		if (media != NULL)
			media->load_count++;

		struct sochgr_scsi_changer_slot * sfrom = from->data;
		struct sochgr_scsi_changer_slot * sto = to->slot->data;
		sto->src_address = sfrom->address;
		sto->src_slot = from;
	}

	if (reset_drive) {
		failed = to->ops->reset(to);
		if (failed != 0)
			so_log_write(so_log_level_critical,
				dgettext("storiqone-changer-scsi", "[%s | %s]: failed to reset drive #%u %s %s"),
				sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, to->index, to->vendor, to->model);
	}

	sochgr_scsi_changer.status = so_changer_status_idle;
	if (db_connection != NULL)
		db_connection->ops->sync_changer(db_connection, &sochgr_scsi_changer, so_database_sync_default);

	return failed;
}

static int sochgr_scsi_changer_parse_media(struct so_database_connection * db_connection) {
	unsigned int need_init = 0, nb_drive_enabled = 0, i;
	for (i = 0; i < sochgr_scsi_changer.nb_drives; i++) {
		struct so_slot * sl = sochgr_scsi_changer.slots + i;
		if (sl->drive != NULL && sl->drive->enable)
			nb_drive_enabled++;
	}

	for (i = sochgr_scsi_changer.nb_drives; i < sochgr_scsi_changer.nb_slots; i++) {
		struct so_slot * sl = sochgr_scsi_changer.slots + i;

		if (!sl->enable || !sl->full)
			continue;

		sl->media = db_connection->ops->get_media(db_connection, NULL, sl->volume_name, NULL);
		if (sl->media == NULL)
			need_init++;
	}

	if (need_init > 0)
		so_log_write(so_log_level_notice,
			dngettext("storiqone-changer-scsi", "Found %u unknown media", "Found %u unknown medias", need_init),
			need_init);
		so_log_write(so_log_level_notice,
			dngettext("storiqone-changer-scsi", "Found #%u enabled drive", "Found #%u enabled drives", nb_drive_enabled),
			nb_drive_enabled);

	if (need_init > 1 && nb_drive_enabled > 1) {
		for (i = 0; i < sochgr_scsi_changer.nb_drives; i++)
			if (sochgr_scsi_changer.drives[i].enable)
				so_thread_pool_run("changer init", sochgr_scsi_changer_init_worker, sochgr_scsi_changer.drives + i);

		sleep(5);

		pthread_mutex_lock(&sochgr_scsi_changer_lock);
		while (sochgr_scsi_changer_nb_worker > 0) {
			pthread_mutex_unlock(&sochgr_scsi_changer_lock);
			sleep(1);
			pthread_mutex_lock(&sochgr_scsi_changer_lock);
		}
		pthread_mutex_unlock(&sochgr_scsi_changer_lock);
	} else if (need_init > 0) {
		struct so_drive * dr = NULL;
		for (i = 0; i < sochgr_scsi_changer.nb_drives && dr == NULL; i++)
			if (sochgr_scsi_changer.drives[i].enable)
				dr = sochgr_scsi_changer.drives + i;

		if (dr != NULL)
			sochgr_scsi_changer_init_worker(dr);
		else {
			so_log_write(so_log_level_critical,
				dgettext("storiqone-changer-scsi", "This changer (%s %s %s) seems to be misconfigured, exiting now"),
				sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, sochgr_scsi_changer.serial_number);
			return 1;
		}
	}

	return 0;
}

static int sochgr_scsi_changer_put_offline(struct so_database_connection * db_connect) {
	unsigned int i;

	sochgr_scsi_changer.status = so_changer_status_go_offline;
	db_connect->ops->sync_changer(db_connect, &sochgr_scsi_changer, so_database_sync_default);

retry:
	for (i = 0; i < sochgr_scsi_changer.nb_drives; i++) {
		struct so_drive * dr = sochgr_scsi_changer.drives + i;

		if (!dr->enable)
			continue;

		if (!dr->ops->is_free(dr)) {
			so_poll(-1);
			db_connect->ops->sync_changer(db_connect, &sochgr_scsi_changer, so_database_sync_default);
			goto retry;
		}
	}

	sochgr_scsi_changer.status = so_changer_status_go_offline;
	db_connect->ops->sync_changer(db_connect, &sochgr_scsi_changer, so_database_sync_default);

	for (i = 0; i < sochgr_scsi_changer.nb_drives; i++) {
		struct so_drive * dr = sochgr_scsi_changer.drives + i;

		if (!dr->enable)
			continue;

		dr->ops->update_status(dr);

		if (!dr->is_empty)
			sochgr_scsi_changer_unload(dr, db_connect);
	}

	sochgr_media_release(&sochgr_scsi_changer);

	for (i = sochgr_scsi_changer.nb_drives; i < sochgr_scsi_changer.nb_slots; i++) {
		struct so_slot * sl = sochgr_scsi_changer.slots + i;

		if (!sl->full)
			continue;

		so_media_free(sl->media);
		sl->media = NULL;
		free(sl->volume_name);
		sl->volume_name = NULL;
		sl->full = false;
	}

	sochgr_scsi_changer_scsi_medium_removal(sochgr_scsi_changer_device, true);

	sochgr_scsi_changer.status = so_changer_status_offline;
	sochgr_scsi_changer.is_online = false;
	sochgr_scsi_changer.next_action = so_changer_action_none;
	db_connect->ops->sync_changer(db_connect, &sochgr_scsi_changer, so_database_sync_default);

	return 0;
}

static int sochgr_scsi_changer_put_online(struct so_database_connection * db_connection) {
	sochgr_scsi_changer.status = so_changer_status_go_online;
	db_connection->ops->sync_changer(db_connection, &sochgr_scsi_changer, so_database_sync_default);

	sochgr_scsi_changer_scsi_medium_removal(sochgr_scsi_changer_device, false);

	sochgr_scsi_changer_scsi_loader_ready(sochgr_scsi_changer_device);

	sochgr_scsi_changer_scsi_update_status(&sochgr_scsi_changer, sochgr_scsi_changer_device);

	int failed = sochgr_scsi_changer_parse_media(db_connection);

	sochgr_scsi_changer.status = failed == 0 ? so_changer_status_idle : so_changer_status_error;
	sochgr_scsi_changer.is_online = failed == 0;
	sochgr_scsi_changer.next_action = so_changer_action_none;
	db_connection->ops->sync_changer(db_connection, &sochgr_scsi_changer, so_database_sync_default);

	return failed;
}

static int sochgr_scsi_changer_shut_down(struct so_database_connection * db_connection __attribute__((unused))) {
	sochgr_scsi_changer_scsi_medium_removal(sochgr_scsi_changer_device, true);
	return 0;
}

static int sochgr_scsi_changer_unload(struct so_drive * from, struct so_database_connection * db_connection) {
	int failed = from->ops->update_status(from);
	if (failed != 0) {
		so_log_write(so_log_level_critical,
			dgettext("storiqone-changer-scsi", "[%s | %s]: failed to get status from drive #%u %s %s"),
			sochgr_scsi_changer.vendor, sochgr_scsi_changer.model, from->index, from->vendor, from->model);

		pthread_mutex_lock(&sochgr_scsi_changer_lock);
		sochgr_scsi_changer_nb_worker--;
		pthread_mutex_unlock(&sochgr_scsi_changer_lock);
		return 1;
	}

	struct sochgr_scsi_changer_slot * sfrom = from->slot->data;
	struct so_slot * to = sfrom->src_slot;

	if (to != NULL && (!to->enable || to->full)) {
		// TODO: oops, can't reuse original slot
		to = NULL;
	}

	unsigned int i;
	for (i = sochgr_scsi_changer.nb_drives; i < sochgr_scsi_changer.nb_slots && to == NULL; i++) {
		to = sochgr_scsi_changer.slots + i;

		if (!to->enable || to->media != NULL) {
			to = NULL;
			continue;
		}
	}

	if (to == NULL) {
		// TODO: no slot found
		return 1;
	}

	sochgr_scsi_changer_wait();

	sochgr_scsi_changer.status = so_changer_status_unloading;
	if (db_connection != NULL)
		db_connection->ops->sync_changer(db_connection, &sochgr_scsi_changer, so_database_sync_default);

	failed = sochgr_scsi_changer_scsi_move(sochgr_scsi_changer_device, sochgr_scsi_changer_transport_address, from->slot, to);

	if (failed != 0) {
		sochgr_scsi_changer_wait();

		struct so_slot tmp_from = *from->slot;
		sochgr_scsi_changer_scsi_loader_check_slot(&sochgr_scsi_changer, sochgr_scsi_changer_device, &tmp_from);

		struct so_slot tmp_to = *to;
		sochgr_scsi_changer_scsi_loader_check_slot(&sochgr_scsi_changer, sochgr_scsi_changer_device, &tmp_to);

		failed = tmp_from.full || !tmp_to.full;
	}

	if (failed == 0) {
		to->media = from->slot->media;
		from->slot->media = NULL;

		to->volume_name = from->slot->volume_name;
		from->slot->volume_name = NULL;

		to->full = true;
		from->slot->full = false;

		struct sochgr_scsi_changer_slot * sfrom = from->slot->data;
		sfrom->src_address = 0;
	}

	from->ops->reset(from);

	sochgr_scsi_changer.status = so_changer_status_idle;
	if (db_connection != NULL)
		db_connection->ops->sync_changer(db_connection, &sochgr_scsi_changer, so_database_sync_default);

	return failed;
}

static void sochgr_scsi_changer_wait() {
	bool is_ready = true;
	while (sochgr_scsi_changer_scsi_loader_ready(sochgr_scsi_changer_device)) {
		if (is_ready) {
			so_log_write(so_log_level_warning,
				dgettext("storiqone-changer-scsi", "[%s | %s]: waiting for device to be ready"),
				sochgr_scsi_changer.vendor, sochgr_scsi_changer.model);
			is_ready = false;
		}

		sleep(5);
	}
}

