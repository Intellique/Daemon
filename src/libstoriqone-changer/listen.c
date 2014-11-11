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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// dgettext
#include <libintl.h>
// free, malloc
#include <stdlib.h>
// bzero
#include <strings.h>

#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

#include "changer.h"
#include "drive.h"
#include "listen.h"
#include "media.h"
#include "peer.h"

static struct so_database_connection * sochgr_db = NULL;
static unsigned int sochgr_nb_clients = 0;

static void sochgr_socket_accept(int fd_server, int fd_client, struct so_value * client);
static void sochgr_socket_message(int fd, short event, void * data);

static void sochgr_socket_command_find_free_drive(struct sochgr_peer * peer, struct so_value * request, int fd);
static void sochgr_socket_command_load(struct sochgr_peer * peer, struct so_value * request, int fd);
static void sochgr_socket_command_release_all_media(struct sochgr_peer * peer, struct so_value * request, int fd);
static void sochgr_socket_command_release_media(struct sochgr_peer * peer, struct so_value * request, int fd);
static void sochgr_socket_command_reserve_media(struct sochgr_peer * peer, struct so_value * request, int fd);
static void sochgr_socket_command_sync(struct sochgr_peer * peer, struct so_value * request, int fd);
static void sochgr_socket_command_unload(struct sochgr_peer * peer, struct so_value * request, int fd);

struct sochgr_socket_command {
	unsigned long hash;
	char * name;
	void (*function)(struct sochgr_peer * peer, struct so_value * request, int fd);
} commands[] = {
	{ 0, "find free drive",   sochgr_socket_command_find_free_drive },
	{ 0, "load",              sochgr_socket_command_load },
	{ 0, "release all media", sochgr_socket_command_release_all_media },
	{ 0, "release media",     sochgr_socket_command_release_media },
	{ 0, "reserve media",     sochgr_socket_command_reserve_media },
	{ 0, "sync",              sochgr_socket_command_sync },
	{ 0, "unload",            sochgr_socket_command_unload },

	{ 0, NULL, NULL }
};


void sochgr_listen_configure(struct so_value * config) {
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		commands[i].hash = so_string_compute_hash2(commands[i].name);

	so_socket_server(config, sochgr_socket_accept);
}

unsigned int sochgr_listen_nb_clients() {
	return sochgr_nb_clients;
}

static void sochgr_socket_accept(int fd_server __attribute__((unused)), int fd_client, struct so_value * client __attribute__((unused))) {
	struct sochgr_peer * peer = sochgr_peer_new(fd_client);

	so_poll_register(fd_client, POLLIN | POLLHUP, sochgr_socket_message, peer, NULL);
	sochgr_nb_clients++;
}

static void sochgr_socket_message(int fd, short event, void * data) {
	struct sochgr_peer * peer = data;

	if (event & POLLHUP) {
		sochgr_nb_clients--;

		sochgr_socket_command_release_all_media(peer, NULL, -1);

		sochgr_peer_free(peer);
		return;
	}

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
			commands[i].function(peer, request, fd);
			break;
		}
	so_value_free(request);

	if (commands[i].name == NULL) {
		struct so_value * response = so_value_new_boolean(true);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	}
}

void sochgr_listen_set_db_connection(struct so_database_connection * db) {
	sochgr_db = db;
}


static void sochgr_socket_command_find_free_drive(struct sochgr_peer * peer __attribute__((unused)), struct so_value * request, int fd) {
	struct so_value * media_format = NULL;
	bool for_writing = false;
	so_value_unpack(request, "{s{sosb}}", "params", "media format", &media_format, "for writing", &for_writing);

	struct so_media_format * format = malloc(sizeof(struct so_media_format));
	bzero(format, sizeof(struct so_media_format));
	so_media_format_sync(format, media_format);

	struct so_changer_driver * driver = sochgr_changer_get();
	struct so_changer * changer = driver->device;

	unsigned int i, index = changer->nb_drives;
	bool found = false;
	for (i = 0; i < changer->nb_drives && !found; i++) {
		struct so_drive * dr = changer->drives + i;
		if (!dr->enable)
			continue;

		found = dr->ops->check_support(dr, format, for_writing);
		if (!found)
			continue;

		found = dr->ops->is_free(dr);
		if (found)
			index = i;
	}

	so_media_format_free(format);

	struct so_value * response = so_value_pack("{sbsi}", "found", index < changer->nb_drives, "index", (long int) index);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

static void sochgr_socket_command_load(struct sochgr_peer * peer, struct so_value * request, int fd) {
	long int slot_from = -1, slot_to = -1;
	so_value_unpack(request, "{s{sisi}}", "params", "from", &slot_from, "to", &slot_to);

	if (slot_to < 0) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	if (slot_from == slot_to) {
		struct so_value * response = so_value_pack("{sb}", "status", true);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_changer_driver * driver = sochgr_changer_get();
	struct so_changer * changer = driver->device;

	struct so_slot * from = changer->slots + slot_from;
	struct so_slot * to = changer->slots + slot_to;

	if (from->media == NULL || to->drive == NULL) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct sochgr_media * mp = from->media->changer_data;
	if (mp->peer != peer) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	so_log_write(so_log_level_notice, dgettext("libstoriqone-changer", "[%s | %s]: loading media '%s' from slot #%u to drive #%d"), changer->vendor, changer->model, from->volume_name, from->index, to->index);

	int failed = changer->ops->load(from, to->drive, sochgr_db);
	if (failed != 0)
		so_log_write(so_log_level_error, dgettext("libstoriqone-changer", "[%s | %s]: loading media '%s' from slot #%u to drive #%d finished with code = %d"), changer->vendor, changer->model, from->volume_name, from->index, to->index, failed);
	else
		so_log_write(so_log_level_notice, dgettext("libstoriqone-changer", "[%s | %s]: loading media '%s' from slot #%u to drive #%d finished with code = OK"), changer->vendor, changer->model, from->volume_name, from->index, to->index);

	struct so_value * response = so_value_pack("{sb}", "status", failed != 0 ? false : true);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

static void sochgr_socket_command_release_all_media(struct sochgr_peer * peer, struct so_value * request __attribute__((unused)), int fd) {
	struct so_changer_driver * driver = sochgr_changer_get();
	struct so_changer * changer = driver->device;

	unsigned int i;
	for (i = 0; i < changer->nb_slots; i++) {
		struct so_slot * sl = changer->slots + i;
		if (sl->media == NULL)
			continue;

		struct sochgr_media * mp = sl->media->changer_data;
		if (peer == mp->peer)
			mp->peer = NULL;
	}

	if (fd > -1) {
		struct so_value * response = so_value_pack("{sb}", "status", true);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	}
}

static void sochgr_socket_command_release_media(struct sochgr_peer * peer, struct so_value * request, int fd) {
	long int slot = -1;
	so_value_unpack(request, "{s{si}}", "params", "slot", &slot);

	if (slot < 0) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_changer_driver * driver = sochgr_changer_get();
	struct so_changer * changer = driver->device;
	struct so_slot * sl = changer->slots + slot;

	if (sl->media == NULL) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct sochgr_media * mp = sl->media->changer_data;
	bool ok = mp->peer == peer;
	if (ok)
		mp->peer = NULL;

	struct so_value * response = so_value_pack("{sb}", "status", ok);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

static void sochgr_socket_command_reserve_media(struct sochgr_peer * peer, struct so_value * request, int fd) {
	long int slot = -1;
	so_value_unpack(request, "{s{si}}", "params", "slot", &slot);

	if (slot < 0) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_changer_driver * driver = sochgr_changer_get();
	struct so_changer * changer = driver->device;
	struct so_slot * sl = changer->slots + slot;

	if (sl->media == NULL) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct sochgr_media * mp = sl->media->changer_data;
	bool ok = mp->peer == NULL;
	if (ok)
		mp->peer = peer;

	struct so_value * response = so_value_pack("{sb}", "status", ok);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

static void sochgr_socket_command_sync(struct sochgr_peer * peer __attribute__((unused)), struct so_value * request __attribute__((unused)), int fd) {
	struct so_changer_driver * driver = sochgr_changer_get();
	struct so_changer * changer = driver->device;

	struct so_value * response = so_value_pack("{so}", "changer", so_changer_convert(changer));
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

static void sochgr_socket_command_unload(struct sochgr_peer * peer, struct so_value * request, int fd) {
	long int slot_from = -1;
	so_value_unpack(request, "{s{si}}", "params", "from", &slot_from);

	if (slot_from < 0) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_changer_driver * driver = sochgr_changer_get();
	struct so_changer * changer = driver->device;

	struct so_slot * from = changer->slots + slot_from;

	if (from->media == NULL) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct sochgr_media * mp = from->media->changer_data;
	if (mp->peer != peer) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	so_log_write(so_log_level_notice, dgettext("libstoriqone-changer", "[%s | %s]: unloading media '%s' from drive #%d"), changer->vendor, changer->model, from->volume_name, from->drive->index);

	int failed = changer->ops->unload(from->drive, sochgr_db);
	if (failed != 0)
		so_log_write(so_log_level_error, dgettext("libstoriqone-changer", "[%s | %s]: unloading media '%s' from drive #%d finished with code = %d"), changer->vendor, changer->model, from->volume_name, from->drive->index, failed);
	else
		so_log_write(so_log_level_notice, dgettext("libstoriqone-changer", "[%s | %s]: unloading media '%s' from drive #%d finished with code = OK"), changer->vendor, changer->model, from->volume_name, from->drive->index);

	struct so_value * response = so_value_pack("{sb}", "status", failed != 0 ? false : true);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

