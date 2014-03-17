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
// snprintf
#include <stdio.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>
// bind, listen, socket
#include <sys/socket.h>
// bind, listen, socket
#include <sys/types.h>
// struct sockaddr_un
#include <sys/un.h>
// close, unlink
#include <unistd.h>

#include "unix_v1.h"
#include "../poll_v1.h"
#include "../value_v1.h"

struct st_unix_socket_server {
	int fd;
	st_socket_accept_f callback;
};

static void st_socket_unix_server_callback(int fd, short event, void * data);


int st_socket_unix_v1(struct st_value * config) {
	int type = SOCK_STREAM;

	struct st_value * vtype = st_value_hashtable_get2_v1(config, "type", false);
	if (vtype->type == st_value_string && !strcmp(vtype->value.string, "datagram"))
		type = SOCK_DGRAM;

	struct st_value * vpath = st_value_hashtable_get2_v1(config, "path", false);
	if (vpath->type != st_value_string)
		return -1;

	int fd = socket(AF_UNIX, type, 0);
	if (fd < 0)
		return -1;

	struct sockaddr_un addr;
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), vpath->value.string);
	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
		close(fd);
		return -1;
	}

	return fd;
}

bool st_socket_unix_server_v1(struct st_value * config, st_socket_accept_f accept_callback) {
	int type = SOCK_STREAM;

	struct st_value * vtype = st_value_hashtable_get2_v1(config, "type", false);
	if (vtype->type == st_value_string && !strcmp(vtype->value.string, "datagram"))
		type = SOCK_DGRAM;

	struct st_value * vpath = st_value_hashtable_get2_v1(config, "path", false);
	if (vpath->type != st_value_string)
		return false;

	int fd = socket(AF_UNIX, type, 0);
	if (fd < 0)
		return false;

	struct sockaddr_un addr;
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), vpath->value.string);
	int failed = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
	if (failed != 0) {
		close(fd);
		unlink(vpath->value.string);
		return false;
	}

	listen(fd, 16);

	struct st_unix_socket_server * self = malloc(sizeof(struct st_unix_socket_server));
	self->fd = fd;
	self->callback = accept_callback;

	st_poll_register_v1(fd, POLLIN | POLLPRI, st_socket_unix_server_callback, self, free);

	return true;
}

static void st_socket_unix_server_callback(int fd, short event __attribute__((unused)), void * data) {
	struct st_unix_socket_server * self = data;

	struct sockaddr_un new_addr;
	bzero(&new_addr, sizeof(new_addr));
	socklen_t length;
	int new_fd = accept(fd, (struct sockaddr *) &new_addr, &length);

	struct st_value * client_info = st_value_pack_v1("{ssss}", "type", "unix", "path", new_addr.sun_path);

	self->callback(fd, new_fd, client_info);
}

