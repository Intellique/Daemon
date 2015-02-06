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
// strdup
#include <string.h>
// bzero
#include <strings.h>

#include <libstoriqone/json.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

#include "changer.h"
#include "drive.h"
#include "listen.h"

static bool stop = false;

static void sodr_changer_process(int fd, short event, void * data);

static void sodr_changer_process_check_support(struct so_value * request, struct so_database_connection * db);
static void sodr_changer_process_is_free(struct so_value * request, struct so_database_connection * db);
static void sodr_changer_process_load_media(struct so_value * request, struct so_database_connection * db);
static void sodr_changer_process_lock(struct so_value * request, struct so_database_connection * db);
static void sodr_changer_process_reset(struct so_value * request, struct so_database_connection * db);
static void sodr_changer_process_stop(struct so_value * request, struct so_database_connection * db);
static void sodr_changer_process_update_status(struct so_value * request, struct so_database_connection * db);


static struct sodr_socket_command {
	unsigned long hash;
	char * name;
	void (*function)(struct so_value * request, struct so_database_connection * db);
} commands[] = {
	{ 0, "check support", sodr_changer_process_check_support },
	{ 0, "is free",       sodr_changer_process_is_free },
	{ 0, "load media",    sodr_changer_process_load_media },
	{ 0, "lock",          sodr_changer_process_lock },
	{ 0, "reset",         sodr_changer_process_reset },
	{ 0, "stop",          sodr_changer_process_stop },
	{ 0, "update status", sodr_changer_process_update_status },

	{ 0, NULL, NULL }
};


bool sodr_changer_is_stopped() {
	return stop;
}

static void sodr_changer_process(int fd, short event __attribute__((unused)), void * data) {
	struct so_value * request = so_json_parse_fd(fd, -1);
	char * command = NULL;
	if (request == NULL || so_value_unpack(request, "{ss}", "command", &command) < 0) {
		if (request != NULL)
			so_value_free(request);
		return;
	}

	const unsigned long hash = so_string_compute_hash2(command);
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		if (hash == commands[i].hash) {
			commands[i].function(request, data);
			break;
		}
	so_value_free(request);

	if (commands[i].name == NULL) {
		struct so_value * response = so_value_new_boolean(true);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	}
}

void sodr_changer_setup(struct so_database_connection * db_connect) {
	so_poll_register(0, POLLIN, sodr_changer_process, db_connect, NULL);

	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		commands[i].hash = so_string_compute_hash2(commands[i].name);
}

void sodr_changer_stop() {
	stop = true;
	so_poll_unregister(0, POLLIN);
}


static void sodr_changer_process_check_support(struct so_value * request, struct so_database_connection * db) {
	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	struct so_value * media_format = NULL;
	bool for_writing = false;

	so_value_unpack(request, "{s{sosb}}", "params", "format", &media_format, "for writing", &for_writing);

	struct so_media_format * format = malloc(sizeof(struct so_media_format));
	bzero(format, sizeof(struct so_media_format));
	so_media_format_sync(format, media_format);

	bool ok = drive->ops->check_support(format, for_writing, db);

	so_media_format_free(format);

	struct so_value * returned = so_value_pack("{sb}", "status", ok);
	so_json_encode_to_fd(returned, 1, true);
	so_value_free(returned);
}

static void sodr_changer_process_is_free(struct so_value * request __attribute__((unused)), struct so_database_connection * db __attribute__((unused))) {
	bool locked = sodr_listen_is_locked();

	struct so_value * returned = so_value_pack("{sb}", "returned", !locked);
	so_json_encode_to_fd(returned, 1, true);
	so_value_free(returned);
}

static void sodr_changer_process_load_media(struct so_value * request, struct so_database_connection * db __attribute__((unused))) {
	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	struct so_value * media = NULL;

	so_value_unpack(request, "{s{sosb}}", "params", "media", &media);

	if (media != NULL) {
		struct so_media * new_media = so_media_new(media);
		struct so_slot * sl = drive->slot;

		if (new_media != NULL) {
			sl->media = new_media;
			sl->volume_name = strdup(new_media->label);
		} else {
			so_media_free(sl->media);
			sl->media = NULL;
			free(sl->volume_name);
			sl->volume_name = NULL;
		}
		sl->full = new_media != NULL;
	}

	struct so_value * returned = so_value_pack("{si}", "status", media != NULL);
	so_json_encode_to_fd(returned, 1, true);
	so_value_free(returned);
}

static void sodr_changer_process_lock(struct so_value * request, struct so_database_connection * db __attribute__((unused))) {
	if (sodr_listen_is_locked()) {
		struct so_value * returned = so_value_pack("{sb}", "returned", false);
		so_json_encode_to_fd(returned, 1, true);
		so_value_free(returned);
		return;
	}

	char * job_key = NULL;
	so_value_unpack(request, "{s{ss}}", "params", "job key", &job_key);
	sodr_listen_set_peer_key(job_key);
	free(job_key);

	struct so_value * returned = so_value_pack("{sb}", "returned", true);
	so_json_encode_to_fd(returned, 1, true);
	so_value_free(returned);
}

static void sodr_changer_process_reset(struct so_value * request, struct so_database_connection * db) {
	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	if (so_value_hashtable_has_key2(request, "slot")) {
		struct so_value * slot = so_value_hashtable_get2(request, "slot", false, false);
		so_slot_sync(drive->slot, slot);
	}

	int failed = drive->ops->reset(db);

	struct so_value * returned = so_value_pack("{sb}", "status", failed != 0 ? false : true);
	so_json_encode_to_fd(returned, 1, true);
	so_value_free(returned);
}

static void sodr_changer_process_stop(struct so_value * request __attribute__((unused)), struct so_database_connection * db __attribute__((unused))) {
	stop = true;
}

static void sodr_changer_process_update_status(struct so_value * request __attribute__((unused)), struct so_database_connection * db) {
	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	int failed = drive->ops->update_status(db);

	struct so_value * returned = so_value_pack("{siso}", "status", (long long int) failed, "drive", so_drive_convert(drive, true));
	so_json_encode_to_fd(returned, 1, true);
	so_value_free(returned);
}

