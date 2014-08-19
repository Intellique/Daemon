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

// malloc
#include <stdlib.h>
// bzero
#include <strings.h>

#include <libstone/json.h>
#include <libstone/poll.h>
#include <libstone/slot.h>
#include <libstone/string.h>
#include <libstone/value.h>

#include "changer.h"
#include "drive.h"
#include "listen.h"

static bool stop = false;

static void stdr_changer_process(int fd, short event, void * data);

static void stdr_changer_process_check_support(struct st_value * request, struct st_database_connection * db);
static void stdr_changer_process_is_free(struct st_value * request, struct st_database_connection * db);
static void stdr_changer_process_reset(struct st_value * request, struct st_database_connection * db);
static void stdr_changer_process_stop(struct st_value * request, struct st_database_connection * db);
static void stdr_changer_process_update_status(struct st_value * request, struct st_database_connection * db);


static struct stdr_socket_command {
	unsigned long hash;
	char * name;
	void (*function)(struct st_value * request, struct st_database_connection * db);
} commands[] = {
	{ 0, "check support", stdr_changer_process_check_support },
	{ 0, "is free",       stdr_changer_process_is_free },
	{ 0, "reset",         stdr_changer_process_reset },
	{ 0, "stop",          stdr_changer_process_stop },
	{ 0, "update status", stdr_changer_process_update_status },

	{ 0, NULL, NULL }
};


bool stdr_changer_is_stopped() {
	return stop;
}

static void stdr_changer_process(int fd, short event __attribute__((unused)), void * data) {
	struct st_value * request = st_json_parse_fd(fd, -1);
	char * command = NULL;
	if (request == NULL || st_value_unpack(request, "{ss}", "command", &command) < 0) {
		if (request != NULL)
			st_value_free(request);
		return;
	}

	const unsigned long hash = st_string_compute_hash2(command);
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		if (hash == commands[i].hash) {
			commands[i].function(request, data);
			break;
		}
	st_value_free(request);

	if (commands[i].name == NULL) {
		struct st_value * response = st_value_new_boolean(true);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
	}
}

void stdr_changer_setup(struct st_database_connection * db_connect) {
	st_poll_register(0, POLLIN, stdr_changer_process, db_connect, NULL);

	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		commands[i].hash = st_string_compute_hash2(commands[i].name);
}

void stdr_changer_stop() {
	stop = true;
	st_poll_unregister(0, POLLIN);
}


static void stdr_changer_process_check_support(struct st_value * request, struct st_database_connection * db) {
	struct st_drive_driver * driver = stdr_drive_get();
	struct st_drive * drive = driver->device;

	struct st_value * media_format = NULL;
	bool for_writing = false;

	st_value_unpack(request, "{s{sosb}}", "params", "format", &media_format, "for writing", &for_writing);

	struct st_media_format * format = malloc(sizeof(struct st_media_format));
	bzero(format, sizeof(struct st_media_format));
	st_media_format_sync(format, media_format);

	bool ok = drive->ops->check_support(format, for_writing, db);

	st_media_format_free(format);

	struct st_value * returned = st_value_pack("{sb}", "status", ok);
	st_json_encode_to_fd(returned, 1, true);
	st_value_free(returned);
}

static void stdr_changer_process_is_free(struct st_value * request __attribute__((unused)), struct st_database_connection * db __attribute__((unused))) {
	bool locked = stdr_listen_is_locked();;

	struct st_value * returned = st_value_pack("{sb}", "ok", !locked);
	st_json_encode_to_fd(returned, 1, true);
	st_value_free(returned);
}

static void stdr_changer_process_reset(struct st_value * request, struct st_database_connection * db) {
	struct st_drive_driver * driver = stdr_drive_get();
	struct st_drive * drive = driver->device;

	if (st_value_hashtable_has_key2(request, "slot")) {
		struct st_value * slot = st_value_hashtable_get2(request, "slot", false, false);
		st_slot_sync(drive->slot, slot);
	}

	int failed = drive->ops->reset(db);

	struct st_value * returned = st_value_pack("{sb}", "status", failed != 0 ? false : true);
	st_json_encode_to_fd(returned, 1, true);
	st_value_free(returned);
}

static void stdr_changer_process_stop(struct st_value * request __attribute__((unused)), struct st_database_connection * db __attribute__((unused))) {
	stop = true;
}

static void stdr_changer_process_update_status(struct st_value * request __attribute__((unused)), struct st_database_connection * db) {
	struct st_drive_driver * driver = stdr_drive_get();
	struct st_drive * drive = driver->device;

	int failed = drive->ops->update_status(db);

	struct st_value * returned = st_value_pack("{siso}", "status", (long long int) failed, "drive", st_drive_convert(drive, true));
	st_json_encode_to_fd(returned, 1, true);
	st_value_free(returned);
}

