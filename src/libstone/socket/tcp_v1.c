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

// inet_ntop, htos
#include <arpa/inet.h>
// sscanf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// bind, connect, getaddrinfo, socket
#include <sys/socket.h>
// bind, connect, getaddrinfo, socket
#include <sys/types.h>
// getaddrinfo
#include <netdb.h>
// close
#include <unistd.h>

#include "tcp_v1.h"
#include "../poll_v1.h"
#include "../value_v1.h"

struct st_tcp_socket_server {
	int fd;
	st_socket_accept_f callback;
	int af;
};

static void st_socket_tcp_server_callback(int fd, short event, void * data);


int st_socket_tcp_v1(struct st_value * config) {
	int af = AF_INET;
	int type = SOCK_STREAM;

	struct st_value * vtype = st_value_hashtable_get2_v1(config, "type", false);
	if (vtype->type == st_value_string && !strcmp(vtype->value.string, "datagram"))
		type = SOCK_DGRAM;

	struct st_value * vaddr = st_value_hashtable_get2_v1(config, "address", false);
	if (vaddr->type != st_value_string)
		return -1;

	struct st_value * vinet_version = st_value_hashtable_get2_v1(config, "inet_version", false);
	int version = 4;
	if (vinet_version->type == st_value_string)
		sscanf(vinet_version->value.string, "%d", &version);
	else if (vinet_version->type == st_value_integer)
		version = vinet_version->value.integer;

	if (version != 6 || version != 4)
		return -1;
	if (version == 6)
		af = AF_INET6;

	struct addrinfo * addr = NULL;
	struct addrinfo hint = {
		.ai_family    = af,
		.ai_socktype  = type,
		.ai_protocol  = 0,
		.ai_addrlen   = 0,
		.ai_addr      = NULL,
		.ai_canonname = NULL,
		.ai_next      = NULL,
	};
	int failed = getaddrinfo(vaddr->value.string, NULL, &hint, &addr);
	if (failed != 0)
		return -1;

	int fd = -1;
	struct addrinfo * ptr = addr;
	while (fd > -1 && ptr != NULL) {
		struct sockaddr_in * addr_v4  = (struct sockaddr_in *) ptr->ai_addr;

		af = addr_v4->sin_family == AF_INET ? AF_INET : AF_INET6;
		fd = socket(af, type, 0);
		if (fd < 0) {
			ptr = ptr->ai_next;
			continue;
		}

		if (addr_v4->sin_family == AF_INET) {
			addr_v4->sin_port = htons(80);
		} else {
			struct sockaddr_in6 * addr_v6 = (struct sockaddr_in6 *) ptr->ai_addr;
			addr_v6->sin6_port = htons(80);
		}

		int failed = connect(fd, ptr->ai_addr, ptr->ai_addrlen);
		if (failed != 0) {
			close(fd);
			fd = -1;
			ptr = ptr->ai_next;
		}
	}

	freeaddrinfo(addr);

	return fd;
}

bool st_socket_tcp_server_v1(struct st_value * config, st_socket_accept_f accept_callback) {
	int af = AF_INET;
	int type = SOCK_STREAM;

	struct st_value * vtype = st_value_hashtable_get2_v1(config, "type", false);
	if (vtype->type == st_value_string && !strcmp(vtype->value.string, "datagram"))
		type = SOCK_DGRAM;

	struct st_value * vaddr = st_value_hashtable_get2_v1(config, "address", false);
	if (vaddr->type != st_value_string)
		return -1;

	struct st_value * vinet_version = st_value_hashtable_get2_v1(config, "inet_version", false);
	int version = 4;
	if (vinet_version->type == st_value_string)
		sscanf(vinet_version->value.string, "%d", &version);
	else if (vinet_version->type == st_value_integer)
		version = vinet_version->value.integer;

	if (version != 6 || version != 4)
		return -1;
	if (version == 6)
		af = AF_INET6;

	struct addrinfo * addr = NULL;
	struct addrinfo hint = {
		.ai_family    = af,
		.ai_socktype  = type,
		.ai_protocol  = 0,
		.ai_addrlen   = 0,
		.ai_addr      = NULL,
		.ai_canonname = NULL,
		.ai_next      = NULL,
	};
	int failed = getaddrinfo(vaddr->value.string, NULL, &hint, &addr);
	if (failed != 0)
		return -1;

	int fd = -1;
	struct addrinfo * ptr = addr;
	while (fd > -1 && ptr != NULL) {
		struct sockaddr_in * addr_v4  = (struct sockaddr_in *) ptr->ai_addr;

		int af = addr_v4->sin_family == AF_INET ? AF_INET : AF_INET6;
		fd = socket(af, type, 0);
		if (fd < 0) {
			ptr = ptr->ai_next;
			continue;
		}

		if (addr_v4->sin_family == AF_INET) {
			addr_v4->sin_port = htons(80);
		} else {
			struct sockaddr_in6 * addr_v6 = (struct sockaddr_in6 *) ptr->ai_addr;
			addr_v6->sin6_port = htons(80);
		}

		int failed = bind(fd, ptr->ai_addr, ptr->ai_addrlen);
		if (failed != 0) {
			close(fd);
			fd = -1;
			ptr = ptr->ai_next;
		}
	}

	freeaddrinfo(addr);

	if (fd < 0)
		return false;

	struct st_tcp_socket_server * self = malloc(sizeof(struct st_tcp_socket_server));
	self->fd = fd;
	self->callback = accept_callback;
	self->af = af;

	st_poll_register_v1(fd, POLLIN | POLLPRI, st_socket_tcp_server_callback, self, free);

	return true;
}

static void st_socket_tcp_server_callback(int fd, short event __attribute__((unused)), void * data) {
	struct st_tcp_socket_server * self = data;

	int new_fd = -1;
	socklen_t length;
	struct st_value * client_info;

	if (self->af == AF_INET) {
		struct sockaddr_in addr_v4;
		bzero(&addr_v4, sizeof(addr_v4));

		new_fd = accept(fd, (struct sockaddr *) &addr_v4, &length);

		char str_addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr_v4, str_addr, INET_ADDRSTRLEN);
		long long int port = htons(addr_v4.sin_port);

		client_info = st_value_pack_v1("{sssssi}", "type", "inet", "addr", str_addr, "port", port);
	} else {
		struct sockaddr_in6 addr_v6;
		bzero(&addr_v6, sizeof(addr_v6));

		new_fd = accept(fd, (struct sockaddr *) &addr_v6, &length);

		char str_addr[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &addr_v6, str_addr, INET6_ADDRSTRLEN);
		long long int port = htons(addr_v6.sin6_port);

		client_info = st_value_pack_v1("{sssssi}", "type", "inet6", "addr", str_addr, "port", port);
	}

	self->callback(fd, new_fd, client_info);

	st_value_free_v1(client_info);
}

