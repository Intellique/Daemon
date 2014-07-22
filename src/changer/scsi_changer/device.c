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
// glob, globfree
#include <glob.h>
// pthread_mutex_t
#include <pthread.h>
// printf, sscanf, snprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// memset, strcat, strchr, strcpy, strdup, strncpy, strstr
#include <string.h>
// stat
#include <sys/stat.h>
// stat
#include <sys/types.h>
// readlink, sleep, stat
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/drive.h>
#include <libstone/file.h>
#include <libstone/log.h>
#include <libstone/media.h>
#include <libstone/slot.h>
#include <libstone/thread_pool.h>
#include <libstone/value.h>
#include <libstone-changer/changer.h>
#include <libstone-changer/drive.h>

#include "device.h"
#include "scsi.h"

static int scsi_changer_init(struct st_value * config, struct st_database_connection * db_connection);
static void scsi_changer_init_worker(void * arg);
static int scsi_changer_load(struct st_slot * from, struct st_drive * to, struct st_database_connection * db_connection);
static int scsi_changer_put_offline(struct st_database_connection * db_connection);
static int scsi_changer_put_online(struct st_database_connection * db_connection);
static int scsi_changer_shut_down(struct st_database_connection * db_connection);
static int scsi_changer_unload(struct st_drive * from, struct st_database_connection * db_connection);
static void scsi_changer_wait(void);

static char * scsi_changer_device = NULL;
static int scsi_changer_transport_address = -1;
static pthread_mutex_t scsi_changer_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile unsigned int scsi_changer_nb_worker = 0;

struct st_changer_ops scsi_changer_ops = {
	.init        = scsi_changer_init,
	.load        = scsi_changer_load,
	.put_offline = scsi_changer_put_offline,
	.put_online  = scsi_changer_put_online,
	.shut_down   = scsi_changer_shut_down,
	.unload      = scsi_changer_unload,
};

static struct st_changer scsi_changer = {
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

	.ops = &scsi_changer_ops,

	.data = NULL,
	.db_data = NULL,
};


struct st_changer * scsi_changer_get_device() {
	return &scsi_changer;
}

static int scsi_changer_init(struct st_value * config, struct st_database_connection * db_connection) {
	st_value_unpack(config, "{sssssssbsbsbss}", "model", &scsi_changer.model, "vendor", &scsi_changer.vendor, "serial number", &scsi_changer.serial_number, "barcode", &scsi_changer.barcode, "enable", &scsi_changer.enable, "is online", &scsi_changer.is_online, "wwn", &scsi_changer.wwn);

	glob_t gl;
	glob("/sys/class/scsi_changer/*/device", 0, NULL, &gl);

	unsigned int i;
	bool found = false, has_wwn = scsi_changer.wwn != NULL;
	for (i = 0; i < gl.gl_pathc && !found; i++) {
		char link[256];
		ssize_t length = readlink(gl.gl_pathv[i], link, 256);
		link[length] = '\0';

		char * ptr = strrchr(link, '/') + 1;
		int host = 0, target = 0, channel = 0, bus = 0;
		sscanf(ptr, "%d:%d:%d:%d", &host, &target, &channel, &bus);

		char * path;
		asprintf(&path, "%s/generic", gl.gl_pathv[i]);
		length = readlink(path, link, 256);
		link[length] = '\0';
		free(path);

		char * device;
		ptr = strrchr(link, '/');
		asprintf(&device, "/dev%s", ptr);

		asprintf(&path, "/sys/class/sas_host/host%d", host);
		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			realpath(gl.gl_pathv[i], link);

			ptr = strstr(link, "end_device-");
			if (ptr != NULL) {
				char * cp = strchr(ptr, '/');
				if (cp != NULL)
					*cp = '\0';

				free(path);
				asprintf(&path, "/sys/class/sas_device/%s/sas_address", ptr);

				char * data = st_file_read_all_from(path);
				cp = strchr(data, '\n');
				if (cp != NULL)
					*cp = '\0';

				char * wwn;
				asprintf(&wwn, "sas:%s", data);

				if (has_wwn && !strcmp(wwn, scsi_changer.wwn))
					found = true;
				else if (!has_wwn) {
					free(scsi_changer.wwn);
					scsi_changer.wwn = wwn;
				}

				if (has_wwn)
					free(wwn);
				free(data);
			}
		}
		free(path);

		if (found)
			scsi_changer_scsi_check_changer(&scsi_changer, device);
		else
			found = scsi_changer_scsi_check_changer(&scsi_changer, device);

		if (found) {
			scsi_changer_device = device;
			st_log_write(st_log_level_info, "Found scsi changer %s %s, serial: %s", scsi_changer.vendor, scsi_changer.model, scsi_changer.serial_number);
		} else {
			free(device);
		}
	}
	globfree(&gl);

	struct st_value * available_drives = st_value_new_hashtable2();
	struct st_value * drives = NULL;
	st_value_unpack(config, "{so}", "drives", &drives);

	if (drives != NULL) {
		struct st_value_iterator * iter = st_value_list_get_iterator(drives);
		while (st_value_iterator_has_next(iter)) {
			struct st_value * dr = st_value_iterator_get_value(iter, true);

			char * vendor = NULL, * model = NULL, * serial_number = NULL;
			st_value_unpack(dr, "{ssssss}", "vendor", &vendor, "model", &model, "serial number", &serial_number);

			char dev[35];
			memset(dev, ' ', 34);
			dev[34] = '\0';

			strncpy(dev, vendor, strlen(vendor));
			dev[7] = ' ';
			strncpy(dev + 8, model, strlen(model));
			dev[23] = ' ';
			strcpy(dev + 24, serial_number);

			st_value_hashtable_put2(available_drives, dev, dr, false);

			free(vendor);
			free(model);
			free(serial_number);
		}
		st_value_iterator_free(iter);
	}

	if (found && scsi_changer.enable && scsi_changer.is_online)
		scsi_changer_scsi_new_status(&scsi_changer, scsi_changer_device, available_drives, &scsi_changer_transport_address);

	st_value_free(available_drives);

	if (found) {
		scsi_changer.status = st_changer_status_idle;

		db_connection->ops->sync_changer(db_connection, &scsi_changer, st_database_sync_id_only);

		unsigned int need_init = 0, nb_drive_enabled = 0;
		for (i = 0; i < scsi_changer.nb_drives; i++) {
			struct st_slot * sl = scsi_changer.slots + i;
			if (sl->drive != NULL && sl->drive->enable)
				nb_drive_enabled++;
		}

		for (i = scsi_changer.nb_drives; i < scsi_changer.nb_slots; i++) {
			struct st_slot * sl = scsi_changer.slots + i;

			if (!sl->enable || !sl->full)
				continue;

			sl->media = db_connection->ops->get_media(db_connection, NULL, sl->volume_name);
			if (sl->media == NULL)
				need_init++;
		}

		if (need_init > 0)
			st_log_write(st_log_level_notice, "Found %u unknown media(s), #%u drive enabled", need_init, nb_drive_enabled);

		if (need_init > 1 && nb_drive_enabled > 1) {
			for (i = 0; i < scsi_changer.nb_drives; i++) {
				if (scsi_changer.drives[i].enable)
					st_thread_pool_run("changer init", scsi_changer_init_worker, scsi_changer.drives + i);
			}

			sleep(5);

			pthread_mutex_lock(&scsi_changer_lock);
			while (scsi_changer_nb_worker > 0) {
				pthread_mutex_unlock(&scsi_changer_lock);
				sleep(1);
				pthread_mutex_lock(&scsi_changer_lock);
			}
			pthread_mutex_unlock(&scsi_changer_lock);
		} else if (need_init > 0) {
			struct st_drive * dr = NULL;
			for (i = 0; i < scsi_changer.nb_drives && dr == NULL; i++)
				if (scsi_changer.drives[i].enable)
					dr = scsi_changer.drives + i;

			if (dr != NULL) {
				scsi_changer_init_worker(dr);
			} else {
				st_log_write(st_log_level_critical, "This changer (%s %s %s) seems to be misconfigured, will exited now", scsi_changer.vendor, scsi_changer.model, scsi_changer.serial_number);
				return 1;
			}
		}
	}

	return found ? 0 : 1;
}

static void scsi_changer_init_worker(void * arg) {
	struct st_drive * dr = arg;

	pthread_mutex_lock(&scsi_changer_lock);
	scsi_changer_nb_worker++;
	pthread_mutex_unlock(&scsi_changer_lock);

	// check if drive has media
	int failed = dr->ops->update_status(dr);
	if (failed != 0) {
		st_log_write(st_log_level_critical, "[%s | %s]: failed to get status from drive #%td %s %s", scsi_changer.vendor, scsi_changer.model, dr - scsi_changer.drives, dr->vendor, dr->model);

		pthread_mutex_lock(&scsi_changer_lock);
		scsi_changer_nb_worker--;
		pthread_mutex_unlock(&scsi_changer_lock);
		return;
	}

	pthread_mutex_lock(&scsi_changer_lock);
	if (dr->slot->media != NULL)
		failed = scsi_changer_unload(dr, NULL);

	unsigned int i;
	for (i = scsi_changer.nb_drives; i < scsi_changer.nb_slots; i++) {
		struct st_slot * sl = scsi_changer.slots + i;

		if (!sl->enable || !sl->full || sl->media != NULL)
			continue;

		failed = scsi_changer_load(sl, dr, NULL);

		pthread_mutex_unlock(&scsi_changer_lock);

		// get drive status
		failed = dr->ops->update_status(dr);
		if (failed != 0) {
			st_log_write(st_log_level_critical, "[%s | %s]: failed to get status from drive #%td %s %s", scsi_changer.vendor, scsi_changer.model, dr - scsi_changer.drives, dr->vendor, dr->model);

			pthread_mutex_lock(&scsi_changer_lock);
			scsi_changer_nb_worker--;
			pthread_mutex_unlock(&scsi_changer_lock);
			return;
		}

		pthread_mutex_lock(&scsi_changer_lock);

		failed = scsi_changer_unload(dr, NULL);
	}

	scsi_changer_nb_worker--;
	pthread_mutex_unlock(&scsi_changer_lock);
}

static int scsi_changer_load(struct st_slot * from, struct st_drive * to, struct st_database_connection * db_connection) {
	scsi_changer_wait();

	scsi_changer.status = st_changer_status_loading;
	db_connection->ops->sync_changer(db_connection, &scsi_changer, st_database_sync_default);

	st_log_write(st_log_level_notice, "[%s | %s]: loading media '%s' from slot #%u to drive #%td", scsi_changer.vendor, scsi_changer.model, from->volume_name, from->index, to - scsi_changer.drives);

	int failed = scsi_changer_scsi_move(scsi_changer_device, scsi_changer_transport_address, from, to->slot);

	if (failed) {
		scsi_changer_wait();

		struct st_slot tmp_from = *from;
		scsi_changer_scsi_loader_check_slot(&scsi_changer, scsi_changer_device, &tmp_from);

		struct st_slot tmp_to = *to->slot;
		scsi_changer_scsi_loader_check_slot(&scsi_changer, scsi_changer_device, &tmp_to);

		failed = tmp_from.full || !tmp_to.full;
	}

	if (failed == 0) {
		st_log_write(st_log_level_notice, "[%s | %s]: loading media '%s' from slot #%u to drive #%td finished with code = OK", scsi_changer.vendor, scsi_changer.model, from->volume_name, from->index, to - scsi_changer.drives);

		struct st_media * media = to->slot->media = from->media;
		from->media = NULL;

		to->slot->volume_name = from->volume_name;
		from->volume_name = NULL;

		from->full = false;
		to->slot->full = true;

		if (media != NULL)
			media->load_count++;

		struct scsi_changer_slot * sfrom = from->data;
		struct scsi_changer_slot * sto = to->slot->data;
		sto->src_address = sfrom->address;
		sto->src_slot = from;
	} else {
		st_log_write(st_log_level_notice, "[%s | %s]: loading media '%s' from slot #%u to drive #%td finished with code = OK", scsi_changer.vendor, scsi_changer.model, from->volume_name, from->index, to - scsi_changer.drives);
	}

	to->ops->reset(to);

	scsi_changer.status = st_changer_status_idle;
	db_connection->ops->sync_changer(db_connection, &scsi_changer, st_database_sync_default);

	return failed;
}

static int scsi_changer_put_offline(struct st_database_connection * db_connection) {
	// TODO
}

static int scsi_changer_put_online(struct st_database_connection * db_connection) {
	// TODO
}

static int scsi_changer_shut_down(struct st_database_connection * db_connection __attribute__((unused))) {
	return 0;
}

static int scsi_changer_unload(struct st_drive * from, struct st_database_connection * db_connection) {
	int failed = from->ops->update_status(from);
	if (failed != 0) {
		st_log_write(st_log_level_critical, "[%s | %s]: failed to get status from drive #%td %s %s", scsi_changer.vendor, scsi_changer.model, from - scsi_changer.drives, from->vendor, from->model);

		pthread_mutex_lock(&scsi_changer_lock);
		scsi_changer_nb_worker--;
		pthread_mutex_unlock(&scsi_changer_lock);
		return 1;
	}

	struct scsi_changer_slot * sfrom = from->slot->data;
	struct st_slot * to = sfrom->src_slot;

	if (to != NULL && (!to->enable || to->full)) {
		// TODO: oops, can't reuse original slot
		to = NULL;
	}

	unsigned int i;
	for (i = scsi_changer.nb_drives; i < scsi_changer.nb_slots && to == NULL; i++) {
		to = scsi_changer.slots + i;

		if (!to->enable || to->media != NULL) {
			to = NULL;
			continue;
		}
	}

	if (to == NULL) {
		// TODO: no slot found
		return 1;
	}

	scsi_changer_wait();

	st_log_write(st_log_level_notice, "[%s | %s]: unloading media '%s' from drive #%td to slot #%u", scsi_changer.vendor, scsi_changer.model, from->slot->volume_name, from - scsi_changer.drives, to->index);

	failed = scsi_changer_scsi_move(scsi_changer_device, scsi_changer_transport_address, from->slot, to);

	if (failed != 0) {
		scsi_changer_wait();

		struct st_slot tmp_from = *from->slot;
		scsi_changer_scsi_loader_check_slot(&scsi_changer, scsi_changer_device, &tmp_from);

		struct st_slot tmp_to = *to;
		scsi_changer_scsi_loader_check_slot(&scsi_changer, scsi_changer_device, &tmp_to);

		failed = tmp_from.full || !tmp_to.full;
	}

	if (failed == 0) {
		st_log_write(st_log_level_notice, "[%s | %s]: unloading media '%s' from drive #%u to slot #%td finished with code = OK", scsi_changer.vendor, scsi_changer.model, from->slot->volume_name, to->index, from - scsi_changer.drives);

		to->media = from->slot->media;
		from->slot->media = NULL;

		to->volume_name = from->slot->volume_name;
		from->slot->volume_name = NULL;

		to->full = true;
		from->slot->full = false;

		struct scsi_changer_slot * sfrom = from->slot->data;
		sfrom->src_address = 0;
	} else
		st_log_write(st_log_level_error, "[%s | %s]: unloading media '%s' from drive #%u to slot #%td finished with error", scsi_changer.vendor, scsi_changer.model, from->slot->volume_name, to->index, from - scsi_changer.drives);

	from->ops->reset(from);

	return failed;
}

static void scsi_changer_wait() {
	bool is_ready = true;
	while (scsi_changer_scsi_loader_ready(scsi_changer_device)) {
		if (is_ready) {
			st_log_write(st_log_level_warning, "[%s | %s]: waiting for device ready", scsi_changer.vendor, scsi_changer.model);
			is_ready = false;
		}

		sleep(5);
	}
}

