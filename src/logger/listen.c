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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// gettext
#include <libintl.h>
// NULL
#include <stddef.h>

#include <libstoriqone/json.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/value.h>
#include <logger/log.h>

#include "listen.h"

static unsigned int solgr_nb_clients = 0;

static void solgr_socket_accept(int fd_server, int fd_client, struct so_value * client);
static void solgr_socket_message(int fd, short event, void * data);


void solgr_listen_configure(struct so_value * config) {
	so_socket_server(config, solgr_socket_accept);
}

static void solgr_socket_accept(int fd_server __attribute__((unused)), int fd_client, struct so_value * client __attribute__((unused))) {
	so_poll_register(fd_client, POLLIN | POLLHUP, solgr_socket_message, NULL, NULL);
	solgr_nb_clients++;

	solgr_log_write2(so_log_level_debug, so_log_type_logger, gettext("New connection..."));
}

static void solgr_socket_message(int fd, short event, void * data __attribute__((unused))) {
	if (event & POLLHUP) {
		solgr_nb_clients--;
		solgr_log_write2(so_log_level_debug, so_log_type_logger, gettext("Connection closed"));
		return;
	}

	struct so_value * message = so_json_parse_fd(fd, 10000);
	if (message != NULL) {
		solgr_log_write(message);
		so_value_free(message);
	} else
		solgr_log_write2(so_log_level_debug, so_log_type_logger, gettext("Received incorrect message"));

	struct so_value * response = so_value_new_boolean(message != NULL);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

unsigned int solgr_listen_nb_clients() {
	return solgr_nb_clients;
}

