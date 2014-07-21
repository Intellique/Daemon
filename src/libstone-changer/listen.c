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

// NULL
#include <stddef.h>

#include <libstone/json.h>
#include <libstone/socket.h>
#include <libstone/poll.h>
#include <libstone/string.h>
#include <libstone/value.h>

#include "changer.h"
#include "listen.h"
#include "peer.h"

static unsigned int stchr_nb_clients = 0;

static void stchgr_socket_accept(int fd_server, int fd_client, struct st_value * client);
static void stchgr_socket_message(int fd, short event, void * data);

static void stchgr_socket_command_sync(struct st_value * request, int fd);

struct stchger_socket_command {
	unsigned long hash;
	char * name;
	void (*function)(struct st_value * request, int fd);
} commands[] = {
	{ 0, "sync", stchgr_socket_command_sync },

	{ 0, NULL, NULL }
};


void stchgr_listen_configure(struct st_value * config) {
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		commands[i].hash = st_string_compute_hash2(commands[i].name);

	st_socket_server(config, stchgr_socket_accept);
}

unsigned int stchgr_listen_nb_clients(void) {
	return stchr_nb_clients;
}

static void stchgr_socket_accept(int fd_server __attribute__((unused)), int fd_client, struct st_value * client __attribute__((unused))) {
	struct stchgr_peer * peer = stchgr_peer_new(fd_client);

	st_poll_register(fd_client, POLLIN | POLLHUP, stchgr_socket_message, peer, NULL);
	stchr_nb_clients++;
}

static void stchgr_socket_message(int fd, short event, void * data __attribute__((unused))) {
	if (event & POLLHUP) {
		stchr_nb_clients--;
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
			commands[i].function(request, fd);
			break;
		}
	st_value_free(request);

	if (commands[i].name == NULL) {
		struct st_value * response = st_value_new_boolean(true);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
	}
}


static void stchgr_socket_command_sync(struct st_value * request __attribute__((unused)), int fd) {
	struct st_changer_driver * driver = stchgr_changer_get();
	struct st_changer * changer = driver->device;

	struct st_value * response = st_value_pack("{so}", "changer", st_changer_convert(changer));
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

