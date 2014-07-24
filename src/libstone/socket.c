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

#include <libstone/value.h>

#include "socket.h"
#include "socket/tcp.h"
#include "socket/unix.h"

__asm__(".symver st_socket_v1, st_socket@@LIBSTONE_1.2");
int st_socket_v1(struct st_value * config) {
	char * domain = NULL;
	if (st_value_unpack(config, "{ss}", "domain", &domain) < 1)
		return -1;

	int fd = -1;
	if (!strcmp(domain, "inet"))
		fd = st_socket_tcp_v1(config);
	else if (!strcmp(domain, "inet6"))
		fd = st_socket_tcp6_v1(config);
	else if (!strcmp(domain, "unix"))
		fd = st_socket_unix_v1(config);

	free(domain);

	return fd;
}

__asm__(".symver st_socket_accept_and_close_v1, st_socket_accept_and_close@@LIBSTONE_1.2");
int st_socket_accept_and_close_v1(int fd, struct st_value * config) {
	char * domain = NULL;
	if (st_value_unpack(config, "{ss}", "domain", &domain) < 1)
		return -1;

	int new_fd = -1;
	if (!strcmp(domain, "inet"))
		new_fd = st_socket_tcp_accept_and_close_v1(fd, config);
	else if (!strcmp(domain, "inet6"))
		new_fd = st_socket_tcp6_accept_and_close_v1(fd, config);
	else if (!strcmp(domain, "unix"))
		new_fd = st_socket_unix_accept_and_close_v1(fd, config);

	free(domain);

	return new_fd;
}

__asm__(".symver st_socket_close_v1, st_socket_close@@LIBSTONE_1.2");
int st_socket_close_v1(int fd, struct st_value * config) {
	char * domain = NULL;
	if (st_value_unpack(config, "{ss}", "domain", &domain) < 1)
		return -1;

	int failed = 0;
	if (!strcmp(domain, "inet"))
		failed = st_socket_tcp_close_v1(fd, config);
	else if (!strcmp(domain, "inet6"))
		failed = st_socket_tcp6_close_v1(fd, config);
	else if (!strcmp(domain, "unix"))
		failed = st_socket_unix_close_v1(fd, config);

	free(domain);

	return failed;
}

__asm__(".symver st_socket_server_v1, st_socket_server@@LIBSTONE_1.2");
bool st_socket_server_v1(struct st_value * config, st_socket_accept_f accept_callback) {
	char * domain = NULL;
	if (st_value_unpack(config, "{ss}", "domain", &domain) < 1)
		return false;

	bool ok = false;
	if (!strcmp(domain, "inet"))
		ok = st_socket_tcp_server_v1(config, accept_callback);
	else if (!strcmp(domain, "inet6"))
		ok = st_socket_tcp6_server_v1(config, accept_callback);
	else if (!strcmp(domain, "unix"))
		ok = st_socket_unix_server_v1(config, accept_callback);

	free(domain);

	return ok;
}

__asm__(".symver st_socket_server_temp_v1, st_socket_server_temp@@LIBSTONE_1.2");
int st_socket_server_temp_v1(struct st_value * config) {
	char * domain = NULL;
	if (st_value_unpack(config, "{ss}", "domain", &domain) < 1)
		return -1;

	int fd = -1;
	if (!strcmp(domain, "inet"))
		fd = st_socket_server_temp_tcp_v1(config);
	else if (!strcmp(domain, "inet6"))
		fd = st_socket_server_temp_tcp6_v1(config);
	else if (!strcmp(domain, "unix"))
		fd = st_socket_server_temp_unix_v1(config);

	free(domain);

	return fd;
}

