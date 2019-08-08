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
// lstat
#include <sys/stat.h>
// accept, bind, connect, listen, lstat, socket
#include <sys/types.h>
// struct sockaddr_un
#include <sys/un.h>
// close, lstat, unlink
#include <unistd.h>

#include <libstoriqone/checksum.h>
#include <libstoriqone/file.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/value.h>

#include "unix.h"

struct so_unix_socket_server {
	int fd;
	so_socket_accept_f callback;
};

static void so_socket_unix_server_callback(int fd, short event, void * data);


int so_socket_unix(struct so_value * config) {
	int type = SOCK_STREAM;

	struct so_value * vtype = so_value_hashtable_get2(config, "type", false, false);
	if (vtype->type == so_value_string && !strcmp(so_value_string_get(vtype), "datagram"))
		type = SOCK_DGRAM;

	struct so_value * vpath = so_value_hashtable_get2(config, "path", false, false);
	if (vpath->type != so_value_string)
		return -1;

	int fd = socket(AF_UNIX, type, 0);
	if (fd < 0)
		return -1;

	// close this socket on exec
	so_file_close_fd_on_exec(fd, true);

	struct sockaddr_un addr;
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", so_value_string_get(vpath));

	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
		close(fd);
		return -1;
	}

	return fd;
}

int so_socket_unix_accept_and_close(int fd, struct so_value * config) {
	struct sockaddr_un new_addr;
	bzero(&new_addr, sizeof(new_addr));
	socklen_t length = 0;
	int new_fd = accept(fd, (struct sockaddr *) &new_addr, &length);

	so_socket_unix_close(fd, config);

	return new_fd;
}

int so_socket_unix_close(int fd, struct so_value * config) {
	const char * path = NULL;
	so_value_unpack(config, "{sS}", "path", &path);

	int failed = close(fd);

	if (path != NULL)
		unlink(path);

	return failed;
}

bool so_socket_unix_server(struct so_value * config, so_socket_accept_f accept_callback) {
	int type = SOCK_STREAM;

	struct so_value * vtype = so_value_hashtable_get2(config, "type", false, false);
	if (vtype->type == so_value_string && !strcmp(so_value_string_get(vtype), "datagram"))
		type = SOCK_DGRAM;

	struct so_value * vpath = so_value_hashtable_get2(config, "path", false, false);
	if (vpath->type != so_value_string)
		return false;

	const char * path = so_value_string_get(vpath);
	if (access(path, F_OK | R_OK | W_OK | X_OK) == 0) {
		struct stat info;
		int failed = lstat(path, &info);
		if (failed != 0 || !S_ISSOCK(info.st_mode))
			return false;

		failed = unlink(path);
		if (failed != 0)
			return false;
	}

	int fd = socket(AF_UNIX, type, 0);
	if (fd < 0)
		return false;

	// close this socket on exec
	so_file_close_fd_on_exec(fd, true);

	struct sockaddr_un addr;
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

	int failed = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
	if (failed != 0) {
		close(fd);
		unlink(so_value_string_get(vpath));
		return false;
	}

	listen(fd, 16);

	struct so_unix_socket_server * self = malloc(sizeof(struct so_unix_socket_server));
	self->fd = fd;
	self->callback = accept_callback;

	so_poll_register(fd, POLLIN | POLLPRI, so_socket_unix_server_callback, self, free);

	return true;
}

bool so_socket_unix_from_template(struct so_value * socket_template, so_socket_accept_f accept_callback) {
	int type = SOCK_STREAM;

	struct so_value * vtype = so_value_hashtable_get2(socket_template, "type", false, false);
	if (vtype->type == so_value_string && !strcmp(so_value_string_get(vtype), "datagram"))
		type = SOCK_DGRAM;

	struct so_value * vpath = so_value_hashtable_get2(socket_template, "path", false, false);
	if (vpath->type != so_value_string)
		return false;

	int fd = -1;
	while (fd < 0) {
		fd = socket(AF_UNIX, type, 0);
		if (fd < 0)
			return false;

		// close this socket on exec
		so_file_close_fd_on_exec(fd, true);

		struct sockaddr_un addr;
		bzero(&addr, sizeof(addr));
		addr.sun_family = AF_UNIX;
		char * new_path = so_file_rename(so_value_string_get(vpath));
		snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", new_path);

		int failed = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
		if (failed != 0) {
			if (errno != EADDRINUSE) {
				close(fd);

				free(new_path);
				return false;
			}

			close(fd);
			fd = -1;
		} else
			so_value_hashtable_put2(socket_template, "path", so_value_new_string(new_path), true);

		free(new_path);
	}

	listen(fd, 16);

	struct so_unix_socket_server * self = malloc(sizeof(struct so_unix_socket_server));
	self->fd = fd;
	self->callback = accept_callback;

	so_poll_register(fd, POLLIN | POLLPRI, so_socket_unix_server_callback, self, free);

	return true;
}

static void so_socket_unix_server_callback(int fd, short event __attribute__((unused)), void * data) {
	struct so_unix_socket_server * self = data;

	struct sockaddr_un new_addr;
	bzero(&new_addr, sizeof(new_addr));
	socklen_t length = 0;
	int new_fd = accept(fd, (struct sockaddr *) &new_addr, &length);

	struct so_value * client_info = so_value_pack("{ssss}", "type", "unix", "path", new_addr.sun_path);

	self->callback(fd, new_fd, client_info);

	so_value_free(client_info);
}

int so_socket_server_temp_unix(struct so_value * config) {
	int type = SOCK_STREAM;

	struct so_value * vtype = so_value_hashtable_get2(config, "type", false, false);
	if (vtype->type == so_value_string && !strcmp(so_value_string_get(vtype), "datagram"))
		type = SOCK_DGRAM;

	char * path = NULL;
	so_value_unpack(config, "{ss}", "path", &path);
	if (path == NULL)
		return -1;

	char * dir = dirname(path);
	char * salt = NULL;

	int fd = -1;
	while (fd < 0) {
		free(salt);

		salt = so_checksum_gen_salt(NULL, 16);
		char * new_path = NULL;
		int size = asprintf(&new_path, "%s/%s.socket", dir, salt);
		free(salt);

		if (size < 0) {
			free(dir);
			free(path);
			return -1;
		}

		fd = socket(AF_UNIX, type, 0);
		if (fd < 0) {
			free(new_path);
			return -1;
		}

		struct sockaddr_un addr;
		bzero(&addr, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, new_path, sizeof(addr.sun_path));

		so_value_hashtable_put2(config, "path", so_value_new_string(new_path), true);

		int failed = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
		if (failed != 0) {
			close(fd);
			unlink(new_path);
			free(new_path);

			if (errno == EADDRINUSE)
				continue;
			else {
				close(fd);
				fd = -1;
				break;
			}
		} else
			free(new_path);

		listen(fd, 16);
	}

	free(path);
	path = dir = NULL;

	return fd;
}

