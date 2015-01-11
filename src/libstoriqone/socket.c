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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

// free
#include <stdlib.h>
// strcmp
#include <string.h>

#include <libstoriqone/socket.h>
#include <libstoriqone/value.h>

#include "socket/tcp.h"
#include "socket/unix.h"

int so_socket(struct so_value * config) {
	char * domain = NULL;
	if (so_value_unpack(config, "{ss}", "domain", &domain) < 1)
		return -1;

	int fd = -1;
	if (!strcmp(domain, "inet"))
		fd = so_socket_tcp(config);
	else if (!strcmp(domain, "inet6"))
		fd = so_socket_tcp6(config);
	else if (!strcmp(domain, "unix"))
		fd = so_socket_unix(config);

	free(domain);

	return fd;
}

int so_socket_accept_and_close(int fd, struct so_value * config) {
	char * domain = NULL;
	if (so_value_unpack(config, "{ss}", "domain", &domain) < 1)
		return -1;

	int new_fd = -1;
	if (!strcmp(domain, "inet"))
		new_fd = so_socket_tcp_accept_and_close(fd, config);
	else if (!strcmp(domain, "inet6"))
		new_fd = so_socket_tcp6_accept_and_close(fd, config);
	else if (!strcmp(domain, "unix"))
		new_fd = so_socket_unix_accept_and_close(fd, config);

	free(domain);

	return new_fd;
}

int so_socket_close(int fd, struct so_value * config) {
	char * domain = NULL;
	if (so_value_unpack(config, "{ss}", "domain", &domain) < 1)
		return -1;

	int failed = 0;
	if (!strcmp(domain, "inet"))
		failed = so_socket_tcp_close(fd, config);
	else if (!strcmp(domain, "inet6"))
		failed = so_socket_tcp6_close(fd, config);
	else if (!strcmp(domain, "unix"))
		failed = so_socket_unix_close(fd, config);

	free(domain);

	return failed;
}

bool so_socket_server(struct so_value * config, so_socket_accept_f accept_callback) {
	char * domain = NULL;
	if (so_value_unpack(config, "{ss}", "domain", &domain) < 1)
		return false;

	bool ok = false;
	if (!strcmp(domain, "inet"))
		ok = so_socket_tcp_server(config, accept_callback);
	else if (!strcmp(domain, "inet6"))
		ok = so_socket_tcp6_server(config, accept_callback);
	else if (!strcmp(domain, "unix"))
		ok = so_socket_unix_server(config, accept_callback);

	free(domain);

	return ok;
}

int so_socket_server_temp(struct so_value * config) {
	char * domain = NULL;
	if (so_value_unpack(config, "{ss}", "domain", &domain) < 1)
		return -1;

	int fd = -1;
	if (!strcmp(domain, "inet"))
		fd = so_socket_server_temp_tcp(config);
	else if (!strcmp(domain, "inet6"))
		fd = so_socket_server_temp_tcp6(config);
	else if (!strcmp(domain, "unix"))
		fd = so_socket_server_temp_unix(config);

	free(domain);

	return fd;
}

