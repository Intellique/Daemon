/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sat, 13 Oct 2012 10:03:50 +0200                         *
\*************************************************************************/

// open
#include <fcntl.h>
// glob
#include <glob.h>
// calloc, realloc
#include <stdlib.h>
// sscanf
#include <stdio.h>
// strcat, strchr, strcpy
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// time
#include <time.h>
// readlink
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/library/ressource.h>
#include <libstone/log.h>
#include <stoned/library/changer.h>

#include "common.h"
#include "scsi.h"

static struct st_changer * st_changers = NULL;
static unsigned int st_nb_fake_changers = 0;
static unsigned int st_nb_real_changers = 0;

struct st_slot * st_changer_find_media_by_pool(struct st_pool * pool, struct st_media ** previous_medias, unsigned int nb_medias) {
	unsigned int i;
	for (i = 0; i < st_nb_real_changers + st_nb_fake_changers; i++) {
		struct st_changer * changer = st_changers + i;

		unsigned int j;
		for (j = 0; j < changer->nb_slots; j++) {
			struct st_slot * slot = changer->slots + j;

			if (slot->lock->ops->try_lock(slot->lock))
				continue;

			struct st_media * media = slot->media;
			if (!media || media->pool != pool) {
				slot->lock->ops->unlock(slot->lock);
				continue;
			}

			unsigned int k;
			for (k = 0; media && k < nb_medias; k++)
				if (media == previous_medias[k])
					media = NULL;

			if (media)
				return slot;

			slot->lock->ops->unlock(slot->lock);
		}
	}

	return NULL;
}

struct st_slot * st_changer_find_slot_by_media(struct st_media * media) {
	unsigned int i;
	for (i = 0; i < st_nb_real_changers + st_nb_fake_changers; i++) {
		struct st_changer * changer = st_changers + i;

		unsigned int j;
		for (j = 0; j < changer->nb_slots; j++) {
			struct st_slot * slot = changer->slots + j;

			if (media == slot->media)
				return slot;
		}
	}

	return NULL;
}

int st_changer_setup(void) {
	glob_t gl;
	gl.gl_offs = 0;
	glob("/sys/class/scsi_device/*/device/scsi_tape", GLOB_DOOFFS, NULL, &gl);

	if (gl.gl_pathc == 0) {
		st_log_write_all(st_log_level_error, st_log_type_user_message, "Panic: There is drive found, exit now !!!");
		return 1;
	}

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %zd drive%s", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");

	struct st_drive * drives = calloc(gl.gl_pathc, sizeof(struct st_drive));
	unsigned int nb_drives = gl.gl_pathc;

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
		drives[i].enabled = 1;

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

	gl.gl_offs = 0;
	glob("/sys/class/scsi_changer/*/device", GLOB_DOOFFS, NULL, &gl);

	/**
	 * In the worst case, we have nb_drives changers,
	 * so we alloc enough memory for this worst case.
	 */
	st_changers = calloc(nb_drives, sizeof(struct st_changer));
	st_nb_real_changers = gl.gl_pathc;

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %zd changer%s", gl.gl_pathc, gl.gl_pathc != 1 ? "s" : "");

	for (i = 0; i < gl.gl_pathc; i++) {
		char link[256];
		ssize_t length = readlink(gl.gl_pathv[i], link, 256);
		link[length] = '\0';

		char * ptr = strrchr(link, '/') + 1;

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
		st_changers[i].enabled = 1;

		st_changers[i].model = NULL;
		st_changers[i].vendor = NULL;
		st_changers[i].revision = NULL;
		st_changers[i].serial_number = NULL;
		st_changers[i].barcode = 0;

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
			if (st_scsi_loader_has_drive(st_changers + i, drives + j)) {
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
			if (drives[j].changer != NULL) {
				drives[j].changer = st_changers + i;

				st_changers[i].drives = malloc(sizeof(struct st_drive));
				*st_changers[i].drives = drives[j];
				st_changers[i].nb_drives = 1;

				st_nb_fake_changers++;
			}
		}
	}

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Library: Found %u stand-alone drive%s", st_nb_fake_changers, st_nb_fake_changers != 1 ? "s" : "");

	if (drives)
		free(drives);

	for (i = 0; i < st_nb_real_changers; i++)
		st_scsi_changer_setup(st_changers + i);

	for (i = st_nb_real_changers; i < st_nb_fake_changers + st_nb_real_changers; i++)
		st_standalone_drive_setup(st_changers + i);

	return 0;
}

void st_changer_sync(struct st_database_connection * connection) {
	unsigned int i;
	for (i = 0; i < st_nb_real_changers + st_nb_fake_changers; i++)
		connection->ops->sync_changer(connection, st_changers + i);
}

