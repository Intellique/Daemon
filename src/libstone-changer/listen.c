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

// free, malloc
#include <stdlib.h>
// bzero
#include <strings.h>

#include <libstone/json.h>
#include <libstone/socket.h>
#include <libstone/poll.h>
#include <libstone/slot.h>
#include <libstone/string.h>
#include <libstone/value.h>

#include "changer.h"
#include "drive.h"
#include "listen.h"
#include "media.h"
#include "peer.h"

static unsigned int stchgr_nb_clients = 0;

static void stchgr_socket_accept(int fd_server, int fd_client, struct st_value * client);
static void stchgr_socket_message(int fd, short event, void * data);

static void stchgr_socket_command_find_free_drive(struct stchgr_peer * peer, struct st_value * request, int fd);
static void stchgr_socket_command_load(struct stchgr_peer * peer, struct st_value * request, int fd);
static void stchgr_socket_command_release_all_media(struct stchgr_peer * peer, struct st_value * request, int fd);
static void stchgr_socket_command_release_media(struct stchgr_peer * peer, struct st_value * request, int fd);
static void stchgr_socket_command_reserve_media(struct stchgr_peer * peer, struct st_value * request, int fd);
static void stchgr_socket_command_sync(struct stchgr_peer * peer, struct st_value * request, int fd);
static void stchgr_socket_command_unload(struct stchgr_peer * peer, struct st_value * request, int fd);

struct stchgr_socket_command {
	unsigned long hash;
	char * name;
	void (*function)(struct stchgr_peer * peer, struct st_value * request, int fd);
} commands[] = {
	{ 0, "find free drive",   stchgr_socket_command_find_free_drive },
	{ 0, "load",              stchgr_socket_command_load },
	{ 0, "release all media", stchgr_socket_command_release_all_media },
	{ 0, "release media",     stchgr_socket_command_release_media },
	{ 0, "reserve media",     stchgr_socket_command_reserve_media },
	{ 0, "sync",              stchgr_socket_command_sync },
	{ 0, "unload",            stchgr_socket_command_unload },

	{ 0, NULL, NULL }
};


void stchgr_listen_configure(struct st_value * config) {
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		commands[i].hash = st_string_compute_hash2(commands[i].name);

	st_socket_server(config, stchgr_socket_accept);
}

unsigned int stchgr_listen_nb_clients() {
	return stchgr_nb_clients;
}

static void stchgr_socket_accept(int fd_server __attribute__((unused)), int fd_client, struct st_value * client __attribute__((unused))) {
	struct stchgr_peer * peer = stchgr_peer_new(fd_client);

	st_poll_register(fd_client, POLLIN | POLLHUP, stchgr_socket_message, peer, NULL);
	stchgr_nb_clients++;
}

static void stchgr_socket_message(int fd, short event, void * data) {
	struct stchgr_peer * peer = data;

	if (event & POLLHUP) {
		stchgr_nb_clients--;

		stchgr_socket_command_release_all_media(peer, NULL, -1);

		stchgr_peer_free(peer);
		return;
	}

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
			commands[i].function(peer, request, fd);
			break;
		}
	st_value_free(request);

	if (commands[i].name == NULL) {
		struct st_value * response = st_value_new_boolean(true);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
	}
}


static void stchgr_socket_command_find_free_drive(struct stchgr_peer * peer __attribute__((unused)), struct st_value * request, int fd) {
	struct st_value * media_format = NULL;
	bool for_reading = false, for_writing = false;
	st_value_unpack(request, "{s{sosbsb}}", "params", "media format", &media_format, "for reading", &for_reading, "for writing", &for_writing);

	struct st_media_format * format = malloc(sizeof(struct st_media_format));
	bzero(format, sizeof(struct st_media_format));
	st_media_format_sync(format, media_format);

	struct st_changer_driver * driver = stchgr_changer_get();
	struct st_changer * changer = driver->device;

	unsigned int i, index = changer->nb_drives;
	for (i = 0; i < changer->nb_drives; i++) {
		struct st_drive * dr = changer->drives + i;
		bool ok = dr->ops->check_support(dr, format, for_reading, for_writing);
		if (!ok)
			continue;

		ok = dr->ops->is_free(dr);
		if (ok)
			index = i;
	}

	free(format);

	struct st_value * response = st_value_pack("{sbsi}", "found", index < changer->nb_drives, "index", (long int) index);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

static void stchgr_socket_command_load(struct stchgr_peer * peer, struct st_value * request, int fd) {
	long int slot_from = -1, slot_to = -1;
	st_value_unpack(request, "{s{sisi}}", "params", "from", &slot_from, "to", &slot_to);

	if (slot_to < 0) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	if (slot_from == slot_to) {
		struct st_value * response = st_value_pack("{sb}", "status", true);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	struct st_changer_driver * driver = stchgr_changer_get();
	struct st_changer * changer = driver->device;

	struct st_slot * from = changer->slots + slot_from;
	struct st_slot * to = changer->slots + slot_to;

	if (from->media == NULL || to->drive == NULL) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	struct stchgr_media * mp = from->media->changer_data;
	if (mp->peer != peer) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	int failed = changer->ops->load(from, to->drive, NULL);

	struct st_value * response = st_value_pack("{sb}", "status", failed != 0 ? false : true);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

static void stchgr_socket_command_release_all_media(struct stchgr_peer * peer, struct st_value * request __attribute__((unused)), int fd) {
	struct st_changer_driver * driver = stchgr_changer_get();
	struct st_changer * changer = driver->device;

	unsigned int i;
	for (i = 0; i < changer->nb_slots; i++) {
		struct st_slot * sl = changer->slots + i;
		if (sl->media == NULL)
			continue;

		struct stchgr_media * mp = sl->media->changer_data;
		if (peer == mp->peer)
			mp->peer = NULL;
	}

	if (fd > -1) {
		struct st_value * response = st_value_pack("{sb}", "status", true);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
	}
}

static void stchgr_socket_command_release_media(struct stchgr_peer * peer, struct st_value * request, int fd) {
	long int slot = -1;
	st_value_unpack(request, "{s{si}}", "params", "slot", &slot);

	if (slot < 0) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	struct st_changer_driver * driver = stchgr_changer_get();
	struct st_changer * changer = driver->device;
	struct st_slot * sl = changer->slots + slot;

	if (sl->media == NULL) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	struct stchgr_media * mp = sl->media->changer_data;
	bool ok = mp->peer == peer;
	if (ok)
		mp->peer = NULL;

	struct st_value * response = st_value_pack("{sb}", "status", ok);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

static void stchgr_socket_command_reserve_media(struct stchgr_peer * peer, struct st_value * request, int fd) {
	long int slot = -1;
	st_value_unpack(request, "{s{si}}", "params", "slot", &slot);

	if (slot < 0) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	struct st_changer_driver * driver = stchgr_changer_get();
	struct st_changer * changer = driver->device;
	struct st_slot * sl = changer->slots + slot;

	if (sl->media == NULL) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	struct stchgr_media * mp = sl->media->changer_data;
	bool ok = mp->peer == NULL;
	if (ok)
		mp->peer = peer;

	struct st_value * response = st_value_pack("{sb}", "status", ok);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

static void stchgr_socket_command_sync(struct stchgr_peer * peer __attribute__((unused)), struct st_value * request __attribute__((unused)), int fd) {
	struct st_changer_driver * driver = stchgr_changer_get();
	struct st_changer * changer = driver->device;

	struct st_value * response = st_value_pack("{so}", "changer", st_changer_convert(changer));
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

static void stchgr_socket_command_unload(struct stchgr_peer * peer, struct st_value * request, int fd) {
	long int slot_from = -1;
	st_value_unpack(request, "{s{si}}", "params", "from", &slot_from);

	if (slot_from < 0) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	struct st_changer_driver * driver = stchgr_changer_get();
	struct st_changer * changer = driver->device;

	struct st_slot * from = changer->slots + slot_from;

	if (from->media == NULL) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	struct stchgr_media * mp = from->media->changer_data;
	if (mp->peer != peer) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	int failed = changer->ops->unload(from->drive, NULL);

	struct st_value * response = st_value_pack("{sb}", "status", failed != 0 ? false : true);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

