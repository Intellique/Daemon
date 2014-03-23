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
#include <libstone/poll.h>
#include <libstone/socket.h>
#include <libstone/value.h>

#include <libstone-logger/log.h>

#include "listen.h"

static unsigned int lgr_nb_clients = 0;

static void lgr_socket_accept(int fd_server, int fd_client, struct st_value * client);
static void lgr_socket_message(int fd, short event, void * data);

void lgr_listen_configure(struct st_value * config) {
	st_socket_server(config, lgr_socket_accept);
}

static void lgr_socket_accept(int fd_server __attribute__((unused)), int fd_client, struct st_value * client __attribute__((unused))) {
	st_poll_register(fd_client, POLLIN | POLLHUP, lgr_socket_message, NULL, NULL);
	lgr_nb_clients++;

	lgr_log_write2(st_log_level_debug, st_log_type_logger, "New connection...");
}

static void lgr_socket_message(int fd, short event, void * data __attribute__((unused))) {
	if (event & POLLHUP) {
		lgr_nb_clients--;
		lgr_log_write2(st_log_level_debug, st_log_type_logger, "Connection closed");
		return;
	}

	struct st_value * message = st_json_parse_fd(fd, -1);
	lgr_log_write(message);
	st_value_free(message);

	struct st_value * response = st_value_new_boolean(true);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

unsigned int lgr_listen_nb_clients() {
	return lgr_nb_clients;
}

