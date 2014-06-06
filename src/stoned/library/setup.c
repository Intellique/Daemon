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
*  Last modified: Fri, 06 Jun 2014 18:51:36 +0200                            *
\****************************************************************************/

#define _GNU_SOURCE
// open
#include <fcntl.h>
// glob, globfree
#include <glob.h>
// realpath
#include <limits.h>
// sscanf, snprintf
#include <stdio.h>
// free, calloc, malloc, realloc
#include <stdlib.h>
// strcat, strchr, strcpy, strstr
#include <string.h>
// open, stat
#include <sys/stat.h>
// open, stat
#include <sys/types.h>
// time
#include <time.h>
// readlink, sleep, stat
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/library/ressource.h>
#include <libstone/library/vtl.h>
#include <libstone/log.h>
#include <libstone/util/file.h>
#include <stoned/library/changer.h>

#include "common.h"
#include "scsi.h"

static struct st_changer * st_changers = NULL;
static unsigned int st_nb_fake_changers = 0;
static unsigned int st_nb_real_changers = 0;
static struct st_vtl_list * st_vtls = NULL, * st_last_vtl = NULL;
static unsigned int st_nb_vtls = 0;

static struct st_slot * st_changer_find_free_media_by_format2(struct st_changer * changer, struct st_media_format * format);
static struct st_slot * st_changer_find_media_by_pool2(struct st_changer * changer, struct st_pool * pool, struct st_media ** previous_medias, unsigned int nb_medias);
static struct st_slot * st_changer_find_slot_by_media2(struct st_changer * changer, struct st_media * media);
static ssize_t st_changer_get_online_size2(struct st_changer * changer, struct st_pool * pool);


void st_changer_add(struct st_changer * vtl) {
	struct st_vtl_list * node = malloc(sizeof(struct st_vtl_list));
	node->changer = vtl;
	node->next = NULL;

	if (st_vtls == NULL)
		st_vtls = st_last_vtl = node;
	else
		st_last_vtl = st_last_vtl->next = node;

	st_nb_vtls++;
}

struct st_slot * st_changer_find_free_media_by_format(struct st_media_format * format) {
	struct st_slot * slot = NULL;
	unsigned int i, nb_changers = st_nb_real_changers + st_nb_fake_changers;
	for (i = 0; slot == NULL && i < nb_changers; i++)
		slot = st_changer_find_free_media_by_format2(st_changers + i, format);

	struct st_vtl_list * vtl;
	for (vtl = st_vtls; slot == NULL && vtl != NULL; vtl = vtl->next)
		slot = st_changer_find_free_media_by_format2(vtl->changer, format);

	return slot;
}

static struct st_slot * st_changer_find_free_media_by_format2(struct st_changer * changer, struct st_media_format * format) {
	unsigned int j;
	for (j = 0; j < changer->nb_slots; j++) {
		struct st_slot * slot = changer->slots + j;
		struct st_drive * drive = slot->drive;

		if (drive != NULL && drive->lock->ops->try_lock(drive->lock))
			continue;
		else if (drive == NULL && slot->lock->ops->try_lock(slot->lock))
			continue;

		struct st_media * media = slot->media;
		if (media == NULL || media->format != format) {
			if (drive != NULL)
				drive->lock->ops->unlock(drive->lock);
			else
				slot->lock->ops->unlock(slot->lock);
			continue;
		}

		if (media != NULL && media->status == st_media_status_new && media->pool == NULL)
			return slot;

		if (drive != NULL)
			drive->lock->ops->unlock(drive->lock);
		else
			slot->lock->ops->unlock(slot->lock);
	}

	return NULL;
}

struct st_slot * st_changer_find_media_by_pool(struct st_pool * pool, struct st_media ** previous_medias, unsigned int nb_medias) {
	struct st_slot * slot = NULL;
	unsigned int i, nb_changers = st_nb_real_changers + st_nb_fake_changers;
	for (i = 0; slot == NULL && i < nb_changers; i++)
		slot = st_changer_find_media_by_pool2(st_changers + i, pool, previous_medias, nb_medias);

	struct st_vtl_list * vtl;
	for (vtl = st_vtls; slot == NULL && vtl != NULL; vtl = vtl->next)
		slot = st_changer_find_media_by_pool2(vtl->changer, pool, previous_medias, nb_medias);

	return slot;
}

static struct st_slot * st_changer_find_media_by_pool2(struct st_changer * changer, struct st_pool * pool, struct st_media ** previous_medias, unsigned int nb_medias) {
	unsigned int j;
	for (j = 0; j < changer->nb_slots; j++) {
		struct st_slot * slot = changer->slots + j;
		struct st_drive * drive = slot->drive;

		if (drive != NULL && drive->lock->ops->try_lock(drive->lock))
			continue;
		else if (drive == NULL && slot->lock->ops->try_lock(slot->lock))
			continue;

		struct st_media * media = slot->media;
		if (media == NULL || media->pool != pool) {
			if (drive != NULL)
				drive->lock->ops->unlock(drive->lock);
			else
				slot->lock->ops->unlock(slot->lock);
			continue;
		}

		unsigned int k;
		for (k = 0; media != NULL && k < nb_medias; k++)
			if (media == previous_medias[k])
				media = NULL;

		if (media != NULL)
			return slot;

		if (drive != NULL)
			drive->lock->ops->unlock(drive->lock);
		else
			slot->lock->ops->unlock(slot->lock);
	}

	return NULL;
}

struct st_slot * st_changer_find_slot_by_media(struct st_media * media) {
	struct st_slot * slot = NULL;
	unsigned int i, nb_changers = st_nb_real_changers + st_nb_fake_changers;
	for (i = 0; slot == NULL && i < nb_changers; i++)
		slot = st_changer_find_slot_by_media2(st_changers + i, media);

	struct st_vtl_list * vtl;
	for (vtl = st_vtls; slot == NULL && vtl != NULL; vtl = vtl->next)
		slot = st_changer_find_slot_by_media2(vtl->changer, media);

	return slot;
}

static struct st_slot * st_changer_find_slot_by_media2(struct st_changer * changer, struct st_media * media) {
	unsigned int j;
	for (j = 0; j < changer->nb_slots; j++) {
		struct st_slot * slot = changer->slots + j;

		if (media == slot->media)
			return slot;
	}

	return NULL;
}

ssize_t st_changer_get_online_size(struct st_pool * pool) {
	ssize_t total = 0;
	unsigned int i, nb_changers = st_nb_real_changers + st_nb_fake_changers;
	for (i = 0; i < nb_changers; i++)
		total += st_changer_get_online_size2(st_changers + i, pool);

	struct st_vtl_list * vtl;
	for (vtl = st_vtls; vtl != NULL; vtl = vtl->next)
		total += st_changer_get_online_size2(vtl->changer, pool);

	return total;
}

static ssize_t st_changer_get_online_size2(struct st_changer * changer, struct st_pool * pool) {
	ssize_t total = 0;
	unsigned int j;
	for (j = 0; j < changer->nb_slots; j++) {
		struct st_slot * slot = changer->slots + j;

		struct st_media * media = slot->media;
		if (media == NULL || media->pool != pool || media->status != st_media_status_in_use)
			continue;

		if (media != NULL)
			total += media->free_block * media->block_size;
	}

	return total;
}

void st_changer_remove(struct st_changer * vtl) {
	if (st_vtls == NULL)
		return;

	struct st_vtl_list * v = NULL;

	if (st_vtls->changer == vtl) {
		v = st_vtls;
		st_vtls = st_vtls->next;
		if (st_vtls == NULL)
			st_last_vtl = NULL;
	} else {
		struct st_vtl_list * prev;
		for (prev = st_vtls; prev->next != NULL; prev = prev->next) {
			v = prev->next;
			if (v->changer == vtl) {
				prev->next = v->next;
				if (v->next == NULL)
					st_last_vtl = prev;
				break;
			}
		}
	}

	if (v != NULL) {
		// stop vtl
		struct st_changer * ch = v->changer;
		free(v);
		v = NULL;

		ch->ops->free(ch);

		st_nb_vtls--;
	}
}

int st_changer_setup(void) {
	glob_t gl;
	glob("/sys/class/scsi_device/*/device/scsi_tape", 0, NULL, &gl);

	if (gl.gl_pathc == 0 && st_nb_vtls == 0) {
		globfree(&gl);
		st_log_write_all(st_log_level_warning, st_log_type_user_message, "There is no drive found !!!");
		return 0;
	}

	unsigned int nb_drives = gl.gl_pathc + st_nb_vtls;
	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %u drive%s", nb_drives, nb_drives != 1 ? "s" : "");

	if (gl.gl_pathc == 0) {
		globfree(&gl);
		return 0;
	}

	struct st_drive * drives = calloc(gl.gl_pathc, sizeof(struct st_drive));
	nb_drives = gl.gl_pathc;

	unsigned int i;
	for (i = 0; i < gl.gl_pathc; i++) {
		char * ptr = strrchr(gl.gl_pathv[i], '/');
		*ptr = '\0';

		char link[256];
		ssize_t length = readlink(gl.gl_pathv[i], link, 256);
		link[length] = '\0';

		ptr = strrchr(link, '/') + 1;

		char path[256];
		strcpy(path, gl.gl_pathv[i]);
		strcat(path, "/generic");
		length = readlink(path, link, 256);
		link[length] = '\0';

		char scsi_device[12];
		ptr = strrchr(link, '/');
		strcpy(scsi_device, "/dev");
		strcat(scsi_device, ptr);

		strcpy(path, gl.gl_pathv[i]);
		strcat(path, "/tape");
		length = readlink(path, link, 256);
		link[length] = '\0';

		char device[12];
		ptr = strrchr(link, '/') + 1;
		strcpy(device, "/dev/n");
		strcat(device, ptr);

		drives[i].device = strdup(device);
		drives[i].scsi_device = strdup(scsi_device);
		drives[i].status = st_drive_unknown;
		drives[i].enabled = true;

		drives[i].model = NULL;
		drives[i].vendor = NULL;
		drives[i].revision = NULL;
		drives[i].serial_number = NULL;

		drives[i].changer = NULL;
		drives[i].slot = NULL;

		drives[i].data = NULL;
		drives[i].db_data = NULL;
	}
	globfree(&gl);

	glob("/sys/class/scsi_changer/*/device", 0, NULL, &gl);

	/**
	 * In the worst case, we have nb_drives changers,
	 * so we alloc enough memory for this worst case.
	 */
	st_changers = calloc(nb_drives + st_nb_vtls, sizeof(struct st_changer));
	st_nb_real_changers = gl.gl_pathc;

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %zd changer%s", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");
	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %u virtual changer%s", st_nb_vtls, st_nb_vtls != 1 ? "s" : "");

	for (i = 0; i < gl.gl_pathc; i++) {
		char link[256];
		ssize_t length = readlink(gl.gl_pathv[i], link, 256);
		link[length] = '\0';

		char * ptr = strrchr(link, '/') + 1;
		int host = 0, target = 0, channel = 0, bus = 0;
		sscanf(ptr, "%d:%d:%d:%d", &host, &target, &channel, &bus);

		char path[256];
		strcpy(path, gl.gl_pathv[i]);
		strcat(path, "/generic");
		length = readlink(path, link, 256);
		link[length] = '\0';

		char device[12];
		ptr = strrchr(link, '/');
		strcpy(device, "/dev");
		strcat(device, ptr);

		st_changers[i].device = strdup(device);
		st_changers[i].status = st_changer_unknown;
		st_changers[i].enabled = true;

		st_changers[i].model = NULL;
		st_changers[i].vendor = NULL;
		st_changers[i].revision = NULL;
		st_changers[i].serial_number = NULL;
		st_changers[i].wwn = NULL;
		st_changers[i].barcode = false;

		snprintf(path, sizeof(path), "/sys/class/sas_host/host%d", host);
		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			realpath(gl.gl_pathv[i], link);

			ptr = strstr(link, "end_device-");
			if (ptr != NULL) {
				char * cp = strchr(ptr, '/');
				if (cp != NULL)
					*cp = '\0';

				snprintf(path, sizeof(path), "/sys/class/sas_device/%s/sas_address", ptr);

				char * data = st_util_file_read_all_from(path);
				cp = strchr(data, '\n');
				if (cp != NULL)
					*cp = '\0';

				asprintf(&st_changers[i].wwn, "sas:%s", data);
				free(data);
			}
		}

		st_changers[i].drives = NULL;
		st_changers[i].nb_drives = 0;
		st_changers[i].slots = NULL;
		st_changers[i].nb_slots = 0;

		st_changers[i].data = NULL;
		st_changers[i].db_data = NULL;
	}
	globfree(&gl);


	// do loaderinfo
	for (i = 0; i < st_nb_real_changers; i++) {
		int fd = open(st_changers[i].device, O_RDWR);
		st_scsi_loader_info(fd, st_changers + i);
		close(fd);
	}

	// do tapeinfo
	for (i = 0; i < nb_drives; i++) {
		int fd = open(drives[i].scsi_device, O_RDWR);
		st_scsi_tape_info(fd, drives + i);
		close(fd);
	}

	// link drive to real changer
	unsigned int nb_changer_without_drive = 0;
	for (i = 0; i < st_nb_real_changers; i++) {
		unsigned j;
		for (j = 0; j < nb_drives; j++) {
			if (drives[j].changer == NULL && st_scsi_loader_has_drive(st_changers + i, drives + j)) {
				drives[j].changer = st_changers + i;

				st_changers[i].drives = realloc(st_changers[i].drives, (st_changers[i].nb_drives + 1) * sizeof(struct st_drive));
				st_changers[i].drives[st_changers[i].nb_drives] = drives[j];
				st_changers[i].nb_drives++;
			}
		}

		if (st_changers[i].nb_drives == 0)
			nb_changer_without_drive++;
	}

	// try to link drive to real changer with database
	if (nb_changer_without_drive > 0) {
		struct st_database * db = NULL;
		struct st_database_config * config = NULL;
		struct st_database_connection * connect = NULL;

		db = st_database_get_default_driver();
		if (db != NULL)
			config = db->ops->get_default_config();
		if (config != NULL)
			connect = config->ops->connect(config);

		for (i = 0; connect != NULL && i < st_nb_real_changers; i++) {
			if (st_changers[i].nb_drives > 0)
				continue;

			unsigned int j, linked = 0;
			for (j = 0; j < nb_drives; j++) {
				if (drives[j].changer)
					continue;

				connect->ops->sync_changer(connect, st_changers + i);
				connect->ops->sync_drive(connect, drives + i);

				if (connect->ops->is_changer_contain_drive(connect, st_changers + i, drives + j)) {
					drives[j].changer = st_changers + i;

					st_changers[i].drives = realloc(st_changers[i].drives, (st_changers[i].nb_drives + 1) * sizeof(struct st_drive));
					st_changers[i].drives[st_changers[i].nb_drives] = drives[j];
					st_changers[i].nb_drives++;

					linked++;
				}
			}

			if (linked)
				nb_changer_without_drive--;
		}

		if (connect != NULL) {
			connect->ops->close(connect);
			connect->ops->free(connect);
		}
	}

	// infor user that there is real changer without drive
	// and wait until database is up to date
	if (nb_changer_without_drive > 0) {
		st_log_write_all(st_log_level_warning, st_log_type_user_message, "Library: There is %u changer%s without drives", nb_changer_without_drive, nb_changer_without_drive != 1 ? "s" : "");

		struct st_database * db = NULL;
		struct st_database_config * config = NULL;
		struct st_database_connection * connect = NULL;

		db = st_database_get_default_driver();
		if (db != NULL)
			config = db->ops->get_default_config();
		if (config != NULL)
			connect = config->ops->connect(config);

		while (nb_changer_without_drive > 0) {
			for (i = 0; i < st_nb_real_changers; i++) {
				if (st_changers[i].nb_drives > 0)
					continue;

				unsigned int j, linked = 0;
				for (j = 0; j < nb_drives; j++) {
					if (drives[j].changer != NULL)
						continue;

					if (connect->ops->is_changer_contain_drive(connect, st_changers + i, drives + j)) {
						drives[j].changer = st_changers + i;

						st_changers[i].drives = realloc(st_changers[i].drives, (st_changers[i].nb_drives + 1) * sizeof(struct st_drive));
						st_changers[i].drives[st_changers[i].nb_drives] = drives[j];
						st_changers[i].nb_drives++;

						linked++;
					}
				}

				if (linked)
					nb_changer_without_drive--;
			}

			if (nb_changer_without_drive > 0)
				sleep(60);
		}

		if (connect) {
			connect->ops->close(connect);
			connect->ops->free(connect);
		}
	}

	// link stand-alone drive to fake changer
	for (i = st_nb_real_changers; i < nb_drives; i++) {
		unsigned j;
		for (j = 0; j < nb_drives; j++) {
			if (drives[j].changer == NULL) {
				drives[j].changer = st_changers + i;

				st_changers[i].device = strdup("");
				st_changers[i].status = st_changer_unknown;
				st_changers[i].model = strdup(drives[j].model);
				st_changers[i].vendor = strdup(drives[j].vendor);
				st_changers[i].revision = strdup(drives[j].revision);
				st_changers[i].serial_number = strdup(drives[j].serial_number);
				st_changers[i].barcode = false;

				st_changers[i].drives = malloc(sizeof(struct st_drive));
				*st_changers[i].drives = drives[j];
				st_changers[i].nb_drives = 1;

				st_nb_fake_changers++;
			}
		}
	}

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %u stand-alone drive%s", st_nb_fake_changers, st_nb_fake_changers != 1 ? "s" : "");

	free(drives);

	// check enabled devices
	struct st_database * db = st_database_get_default_driver();
	struct st_database_config * config = NULL;
	struct st_database_connection * connect = NULL;

	if (db != NULL)
		config = db->ops->get_default_config();
	if (config != NULL)
		connect = config->ops->connect(config);

	for (i = 0; i < st_nb_real_changers + st_nb_fake_changers + st_nb_vtls && connect != NULL; i++) {
		struct st_changer * ch = st_changers + i;

		if (!connect->ops->changer_is_enabled(connect, ch))
			continue;

		unsigned int j;
		for (j = 0; j < ch->nb_drives; j++)
			connect->ops->drive_is_enabled(connect, ch->drives + j);
	}

	// start setup of enabled devices
	for (i = 0; i < st_nb_real_changers; i++) {
		struct st_changer * ch = st_changers + i;

		if (ch->enabled)
			st_scsi_changer_setup(st_changers + i, connect);
	}

	if (connect != NULL)
		connect->ops->free(connect);

	for (i = st_nb_real_changers; i < st_nb_fake_changers + st_nb_real_changers; i++) {
		struct st_changer * ch = st_changers + i;

		if (ch->enabled)
			st_standalone_drive_setup(st_changers + i);
	}

	return 0;
}

void st_changer_stop() {
	unsigned int i, nb_changers = st_nb_real_changers + st_nb_fake_changers;
	for (i = 0; i < nb_changers; i++) {
		struct st_changer * ch = st_changers + i;

		if (ch->enabled) {
			ch->ops->shut_down(ch);
			ch->ops->free(ch);
		}
	}

	struct st_vtl_list * vtl, * next = NULL;
	for (vtl = st_vtls; vtl != NULL; vtl = next) {
		struct st_changer * ch = vtl->changer;

		if (ch->enabled) {
			ch->ops->shut_down(ch);
			ch->ops->free(ch);
		}

		next = vtl->next;
		free(vtl);
	}

	free(st_changers);
	st_changers = NULL;
	st_nb_fake_changers = st_nb_real_changers = st_nb_vtls = 0;
}

void st_changer_sync(struct st_database_connection * connection) {
	unsigned int i, nb_changers = st_nb_real_changers + st_nb_fake_changers;
	for (i = 0; i < nb_changers; i++) {
		struct st_changer * ch = st_changers + i;
		if (!ch->enabled)
			continue;

		ch->ops->update_status(ch);

		connection->ops->sync_changer(connection, ch);
	}

	struct st_vtl_list * vtl;
	for (vtl = st_vtls; vtl != NULL; vtl = vtl->next) {
		struct st_changer * ch = vtl->changer;
		ch->ops->update_status(ch);

		connection->ops->sync_changer(connection, ch);
	}
}

struct st_slot_iterator * st_slot_iterator_by_new_media(struct st_media_format * format) {
	return st_slot_iterator_by_new_media2(format, st_changers, st_nb_fake_changers + st_nb_real_changers, st_vtls);
}

struct st_slot_iterator * st_slot_iterator_by_pool(struct st_pool * pool) {
	return st_slot_iterator_by_pool2(pool, st_changers, st_nb_fake_changers + st_nb_real_changers, st_vtls);
}

