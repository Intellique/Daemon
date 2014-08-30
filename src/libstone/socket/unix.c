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

#define _GNU_SOURCE
// errno
#include <errno.h>
// dirname
#include <libgen.h>
// free, malloc
#include <stdlib.h>
// asprintf, snprintf
#include <stdio.h>
// free
#include <stdlib.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>
// accept, bind, connect, listen, socket
#include <sys/socket.h>
// accept, bind, connect, listen, socket
#include <sys/types.h>
// struct sockaddr_un
#include <sys/un.h>
// close, unlink
#include <unistd.h>

#include <libstone/poll.h>
#include <libstone/value.h>

#include "../checksum.h"
#include "unix.h"

struct st_unix_socket_server_v1 {
	int fd;
	st_socket_accept_f callback;
};

static void st_socket_unix_server_callback_v1(int fd, short event, void * data);


int st_socket_unix_v1(struct st_value * config) {
	int type = SOCK_STREAM;

	struct st_value * vtype = st_value_hashtable_get2(config, "type", false, false);
	if (vtype->type == st_value_string && !strcmp(st_value_string_get(vtype), "datagram"))
		type = SOCK_DGRAM;

	struct st_value * vpath = st_value_hashtable_get2(config, "path", false, false);
	if (vpath->type != st_value_string)
		return -1;

	int fd = socket(AF_UNIX, type, 0);
	if (fd < 0)
		return -1;

	struct sockaddr_un addr;
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), st_value_string_get(vpath));

	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
		close(fd);
		return -1;
	}

	return fd;
}

int st_socket_unix_accept_and_close_v1(int fd, struct st_value * config) {
	struct sockaddr_un new_addr;
	bzero(&new_addr, sizeof(new_addr));
	socklen_t length;
	int new_fd = accept(fd, (struct sockaddr *) &new_addr, &length);

	st_socket_unix_close_v1(fd, config);

	return new_fd;
}

int st_socket_unix_close_v1(int fd, struct st_value * config) {
	char * path = NULL;
	st_value_unpack(config, "{ss}", "path", &path);

	int failed = close(fd);

	if (path != NULL) {
		unlink(path);
		free(path);
	}

	return failed;
}

bool st_socket_unix_server_v1(struct st_value * config, st_socket_accept_f accept_callback) {
	int type = SOCK_STREAM;

	struct st_value * vtype = st_value_hashtable_get2(config, "type", false, false);
	if (vtype->type == st_value_string && !strcmp(st_value_string_get(vtype), "datagram"))
		type = SOCK_DGRAM;

	struct st_value * vpath = st_value_hashtable_get2(config, "path", false, false);
	if (vpath->type != st_value_string)
		return false;

	int fd = socket(AF_UNIX, type, 0);
	if (fd < 0)
		return false;

	struct sockaddr_un addr;
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), st_value_string_get(vpath));

	int failed = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
	if (failed != 0) {
		close(fd);
		unlink(st_value_string_get(vpath));
		return false;
	}

	listen(fd, 16);

	struct st_unix_socket_server_v1 * self = malloc(sizeof(struct st_unix_socket_server_v1));
	self->fd = fd;
	self->callback = accept_callback;

	st_poll_register(fd, POLLIN | POLLPRI, st_socket_unix_server_callback_v1, self, free);

	return true;
}

static void st_socket_unix_server_callback_v1(int fd, short event __attribute__((unused)), void * data) {
	struct st_unix_socket_server_v1 * self = data;

	struct sockaddr_un new_addr;
	bzero(&new_addr, sizeof(new_addr));
	socklen_t length;
	int new_fd = accept(fd, (struct sockaddr *) &new_addr, &length);

	struct st_value * client_info = st_value_pack("{ssss}", "type", "unix", "path", new_addr.sun_path);

	self->callback(fd, new_fd, client_info);

	st_value_free(client_info);
}

int st_socket_server_temp_unix_v1(struct st_value * config) {
	int type = SOCK_STREAM;

	struct st_value * vtype = st_value_hashtable_get2(config, "type", false, false);
	if (vtype->type == st_value_string && !strcmp(st_value_string_get(vtype), "datagram"))
		type = SOCK_DGRAM;

	char * path = NULL;
	st_value_unpack(config, "{ss}", "path", &path);
	if (path == NULL)
		return -1;

	char * dir = dirname(path);
	char * salt = NULL;

	int fd = -1;
	while (fd < 0) {
		free(salt);

		salt = st_checksum_gen_salt(NULL, 16);
		char * new_path;
		asprintf(&new_path, "%s/%s.socket", dir, salt);

		fd = socket(AF_UNIX, type, 0);
		if (fd < 0) {
			free(new_path);
			return -1;
		}

		struct sockaddr_un addr;
		bzero(&addr, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, new_path, sizeof(addr.sun_path));

		st_value_hashtable_put2(config, "path", st_value_new_string(new_path), true);
		free(new_path);

		int failed = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
		if (failed != 0) {
			close(fd);
			unlink(new_path);

			if (errno == EADDRINUSE)
				continue;
			else {
				close(fd);
				fd = -1;
				break;
			}
		}

		listen(fd, 16);
	}

	free(path);
	path = dir = NULL;

	return fd;
}

