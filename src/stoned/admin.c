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

#include <libstone/socket.h>
#include <libstone/value.h>

#include "admin.h"

static struct st_value * std_admin_conf = NULL;

struct st_admin_client {
	bool logged;
};

static void std_admin_new_connection(int fd_server, int fd_client, struct st_value * client);


void std_admin_config(struct st_value * config) {
	std_admin_conf = st_value_share(config);

	struct st_value * socket = st_value_hashtable_get2(config, "socket", false);
	st_socket_server(socket, std_admin_new_connection);
}

static void std_admin_new_connection(int fd_server, int fd_client, struct st_value * client) {
}

