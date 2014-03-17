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

#include <libstone/poll.h>
#include <libstone/socket.h>

#include "listen.h"

static void lgr_socket_accept(int fd_server, int fd_client, struct st_value * client);
static void lgr_socket_message(int fd, short event, void * data);

void lgr_listen_configure(struct st_value * config) {
	st_socket_server(config, lgr_socket_accept);
}

static void lgr_socket_accept(int fd_server __attribute__((unused)), int fd_client, struct st_value * client __attribute__((unused))) {
	st_poll_register(fd_client, POLLIN | POLLHUP, lgr_socket_message, NULL, NULL);
}

static void lgr_socket_message(int fd, short event, void * data __attribute__((unused))) {
	if (event == POLLHUP) {
		st_poll_unregister(fd);
		return;
	}
}

