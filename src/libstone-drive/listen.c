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

// free
#include <stdlib.h>
// strcmp
#include <string.h>

#include <libstone/checksum.h>
#include <libstone/json.h>
#include <libstone/socket.h>
#include <libstone/poll.h>
#include <libstone/string.h>
#include <libstone/value.h>

#include "listen.h"
#include "peer.h"

static struct st_value * stdr_config = NULL;
static unsigned int stdr_nb_clients = 0;
static struct stdr_peer * stdr_current_peer = NULL;

static void stdr_socket_accept(int fd_server, int fd_client, struct st_value * client);
static void stdr_socket_message(int fd, short event, void * data);

static void stdr_socket_command_get_raw_reader(struct stdr_peer * peer, struct st_value * request, int fd);
static void stdr_socket_command_lock(struct stdr_peer * peer, struct st_value * request, int fd);
static void stdr_socket_command_release(struct stdr_peer * peer, struct st_value * request, int fd);

struct stdr_socket_command {
	unsigned long hash;
	char * name;
	void (*function)(struct stdr_peer * peer, struct st_value * request, int fd);
} commands[] = {
	{ 0, "get raw reader", stdr_socket_command_get_raw_reader },
	{ 0, "lock",           stdr_socket_command_lock },
	{ 0, "release",        stdr_socket_command_release },

	{ 0, NULL, NULL }
};


void stdr_listen_configure(struct st_value * config) {
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		commands[i].hash = st_string_compute_hash2(commands[i].name);

	if (stdr_config != NULL)
		st_value_free(stdr_config);
	stdr_config = st_value_share(config);

	st_socket_server(config, stdr_socket_accept);
}

unsigned int stdr_listen_nb_clients() {
	return stdr_nb_clients;
}

static void stdr_socket_accept(int fd_server __attribute__((unused)), int fd_client, struct st_value * client __attribute__((unused))) {
	struct stdr_peer * peer = stdr_peer_new(fd_client);

	st_poll_register(fd_client, POLLIN | POLLHUP, stdr_socket_message, peer, NULL);
	stdr_nb_clients++;
}

static void stdr_socket_message(int fd, short event, void * data) {
	struct stdr_peer * peer = data;

	if (event & POLLHUP) {
		stdr_nb_clients--;
		stdr_peer_free(peer);
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


static void stdr_socket_command_get_raw_reader(struct stdr_peer * peer, struct st_value * request, int fd) {
	long int position = -1;
	char * cookie = NULL;
	st_value_unpack(request, "{siss}", "file position", &position, "cookie", &cookie);

	if (cookie == NULL || peer->cookie == NULL || strcmp(cookie, peer->cookie) != 0) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	struct st_value * tmp_config = st_value_copy(stdr_config, true);
	int tmp_socket = st_socket_server_temp(tmp_config);

	struct st_value * response = st_value_pack("{sbsO}", "status", true, "socket", tmp_config);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);

	peer->data_socket = st_socket_accept_and_close(tmp_socket, tmp_config);
	st_value_free(tmp_config);
}

static void stdr_socket_command_lock(struct stdr_peer * peer, struct st_value * request __attribute__((unused)), int fd) {
	if (stdr_current_peer == NULL) {
		stdr_current_peer = peer;

		free(peer->cookie);
		peer->cookie = st_checksum_gen_salt(NULL, 32);

		struct st_value * response = st_value_pack("{sbss}", "status", true, "cookie", peer->cookie);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
	} else {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
	}
}

static void stdr_socket_command_release(struct stdr_peer * peer, struct st_value * request __attribute__((unused)), int fd) {
	bool ok = stdr_current_peer == peer;

	if (ok)
		stdr_current_peer = NULL;

	free(peer->cookie);
	peer->cookie = NULL;

	struct st_value * response = st_value_pack("{sb}", "status", ok);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

