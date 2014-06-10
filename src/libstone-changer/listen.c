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
#include <libstone/value.h>

#include "listen.h"

static unsigned int stchr_nb_clients = 0;

static void stchgr_socket_accept(int fd_server, int fd_client, struct st_value * client);
static void stchgr_socket_message(int fd, short event, void * data);


void stchgr_listen_configure(struct st_value * config) {
	st_socket_server(config, stchgr_socket_accept);
}

unsigned int stchgr_listen_nb_clients(void) {
	return stchr_nb_clients;
}

static void stchgr_socket_accept(int fd_server __attribute__((unused)), int fd_client, struct st_value * client __attribute__((unused))) {
	st_poll_register(fd_client, POLLIN | POLLHUP, stchgr_socket_message, NULL, NULL);
	stchr_nb_clients++;
}

static void stchgr_socket_message(int fd, short event, void * data __attribute__((unused))) {
	if (event & POLLHUP) {
		stchr_nb_clients--;
		return;
	}

	struct st_value * message = st_json_parse_fd(fd, -1);
	st_value_free(message);

	struct st_value * response = st_value_new_boolean(true);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

