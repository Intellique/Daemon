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

// inet_ntop, htons, ntohs
#include <arpa/inet.h>
// free, malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>
// accept, bind, connect, getaddrinfo, socket
#include <sys/socket.h>
// accept, bind, connect, getaddrinfo, socket
#include <sys/types.h>
// getaddrinfo
#include <netdb.h>
// close
#include <unistd.h>

#include <libstone/poll.h>

#include "tcp.h"
#include "../value.h"

struct st_tcp_socket_server_v1 {
	int fd;
	st_socket_accept_f_v1 callback;
	int af;
};

static void st_socket_tcp_server_callback(int fd, short event, void * data);


int st_socket_tcp_v1(struct st_value_v1 * config) {
	int stype = SOCK_STREAM;

	char * type = NULL;
	char * saddr;
	long long int port;

	if (st_value_unpack_v1(config, "{sssi}", "address", &saddr, "port", &port) < 2)
		return -1;

	if (st_value_unpack_v1(config, "{ss}", "type", &type) > 0 && !strcmp(type, "datagram"))
		stype = SOCK_DGRAM;

	free(type);

	struct addrinfo * addr = NULL;
	struct addrinfo hint = {
		.ai_family    = AF_INET,
		.ai_socktype  = stype,
		.ai_protocol  = 0,
		.ai_addrlen   = 0,
		.ai_addr      = NULL,
		.ai_canonname = NULL,
		.ai_next      = NULL,
	};
	int failed = getaddrinfo(saddr, NULL, &hint, &addr);
	free(saddr);
	if (failed != 0)
		return -1;

	int fd = -1;
	struct addrinfo * ptr = addr;
	while (fd < 0 && ptr != NULL) {
		struct sockaddr_in * addr_v4  = (struct sockaddr_in *) ptr->ai_addr;

		fd = socket(AF_INET, stype, 0);
		if (fd < 0) {
			ptr = ptr->ai_next;
			continue;
		}

		addr_v4->sin_port = htons(port);

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

int st_socket_tcp6_v1(struct st_value_v1 * config) {
	int stype = SOCK_STREAM;

	char * type = NULL;
	char * saddr;
	long long int port;

	if (st_value_unpack_v1(config, "{sssi}", "address", &saddr, "port", &port) < 2)
		return -1;

	if (st_value_unpack_v1(config, "{ss}", "type", &type) > 0 && !strcmp(type, "datagram"))
		stype = SOCK_DGRAM;

	free(type);

	struct addrinfo * addr = NULL;
	struct addrinfo hint = {
		.ai_family    = AF_INET6,
		.ai_socktype  = stype,
		.ai_protocol  = 0,
		.ai_addrlen   = 0,
		.ai_addr      = NULL,
		.ai_canonname = NULL,
		.ai_next      = NULL,
	};
	int failed = getaddrinfo(saddr, NULL, &hint, &addr);
	free(saddr);
	if (failed != 0)
		return -1;

	int fd = -1;
	struct addrinfo * ptr = addr;
	while (fd < 0 && ptr != NULL) {
		struct sockaddr_in6 * addr_v6  = (struct sockaddr_in6 *) ptr->ai_addr;

		fd = socket(AF_INET6, stype, 0);
		if (fd < 0) {
			ptr = ptr->ai_next;
			continue;
		}

		addr_v6->sin6_port = htons(port);

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

bool st_socket_tcp_server_v1(struct st_value_v1 * config, st_socket_accept_f_v1 accept_callback) {
	int type = SOCK_STREAM;

	struct st_value_v1 * vtype = st_value_hashtable_get2_v1(config, "type", false);
	if (vtype->type == st_value_string && !strcmp(vtype->value.string, "datagram"))
		type = SOCK_DGRAM;

	struct st_value_v1 * vaddr = st_value_hashtable_get2_v1(config, "address", false);
	struct st_value_v1 * vport = st_value_hashtable_get2_v1(config, "port", false);
	if (vport->type != st_value_integer)
		return -1;

	int fd = -1, failed;
	if (vaddr->type == st_value_string) {
		struct addrinfo * addr = NULL;
		struct addrinfo hint = {
			.ai_family    = AF_INET,
			.ai_socktype  = type,
			.ai_protocol  = 0,
			.ai_addrlen   = 0,
			.ai_addr      = NULL,
			.ai_canonname = NULL,
			.ai_next      = NULL,
		};
		failed = getaddrinfo(vaddr->value.string, NULL, &hint, &addr);
		if (failed != 0)
			return -1;

		struct addrinfo * ptr = addr;
		while (fd < 0 && ptr != NULL) {
			fd = socket(AF_INET, type, 0);
			if (fd < 0) {
				ptr = ptr->ai_next;
				continue;
			}

			struct sockaddr_in * addr_v4 = (struct sockaddr_in *) ptr->ai_addr;
			addr_v4->sin_port = htons(vport->value.integer);

			int failed = bind(fd, ptr->ai_addr, ptr->ai_addrlen);
			if (failed != 0) {
				close(fd);
				fd = -1;
				ptr = ptr->ai_next;
			}
		}

		freeaddrinfo(addr);
	} else {
		fd = socket(AF_INET, type, 0);
		if (fd < 0)
			return false;

		struct sockaddr_in addr_v4;
		bzero(&addr_v4, sizeof(addr_v4));
		addr_v4.sin_port = htons(vport->value.integer);

		int failed = bind(fd, (struct sockaddr *) &addr_v4, sizeof(addr_v4));
		if (failed != 0) {
			close(fd);
			fd = -1;
		}
	}

	if (fd < 0)
		return false;

	failed = listen(fd, 16);

	struct st_tcp_socket_server_v1 * self = malloc(sizeof(struct st_tcp_socket_server_v1));
	self->fd = fd;
	self->callback = accept_callback;
	self->af = AF_INET;

	st_poll_register(fd, POLLIN | POLLPRI, st_socket_tcp_server_callback, self, free);

	return true;
}

bool st_socket_tcp6_server_v1(struct st_value_v1 * config, st_socket_accept_f_v1 accept_callback) {
	int type = SOCK_STREAM;

	struct st_value_v1 * vtype = st_value_hashtable_get2_v1(config, "type", false);
	if (vtype->type == st_value_string && !strcmp(vtype->value.string, "datagram"))
		type = SOCK_DGRAM;

	struct st_value_v1 * vaddr = st_value_hashtable_get2_v1(config, "address", false);
	struct st_value_v1 * vport = st_value_hashtable_get2_v1(config, "port", false);
	if (vport->type != st_value_integer)
		return -1;

	int fd = -1, failed;
	if (vaddr->type == st_value_string) {
		struct addrinfo * addr = NULL;
		struct addrinfo hint = {
			.ai_family    = AF_INET6,
			.ai_socktype  = type,
			.ai_protocol  = 0,
			.ai_addrlen   = 0,
			.ai_addr      = NULL,
			.ai_canonname = NULL,
			.ai_next      = NULL,
		};
		failed = getaddrinfo(vaddr->value.string, NULL, &hint, &addr);
		if (failed != 0)
			return -1;

		struct addrinfo * ptr = addr;
		while (fd < 0 && ptr != NULL) {
			fd = socket(AF_INET6, type, 0);
			if (fd < 0) {
				ptr = ptr->ai_next;
				continue;
			}

			struct sockaddr_in6 * addr_v6 = (struct sockaddr_in6 *) ptr->ai_addr;
			addr_v6->sin6_port = htons(vport->value.integer);

			int failed = bind(fd, ptr->ai_addr, ptr->ai_addrlen);
			if (failed != 0) {
				close(fd);
				fd = -1;
				ptr = ptr->ai_next;
			}
		}

		freeaddrinfo(addr);
	} else {
		fd = socket(AF_INET6, type, 0);
		if (fd < 0)
			return false;

		struct sockaddr_in6 addr_v6;
		bzero(&addr_v6, sizeof(addr_v6));
		addr_v6.sin6_port = htons(vport->value.integer);

		int failed = bind(fd, (struct sockaddr *) &addr_v6, sizeof(addr_v6));
		if (failed != 0) {
			close(fd);
			fd = -1;
		}
	}

	if (fd < 0)
		return false;

	failed = listen(fd, 16);

	struct st_tcp_socket_server_v1 * self = malloc(sizeof(struct st_tcp_socket_server_v1));
	self->fd = fd;
	self->callback = accept_callback;
	self->af = AF_INET6;

	st_poll_register(fd, POLLIN | POLLPRI, st_socket_tcp_server_callback, self, free);

	return true;
}

static void st_socket_tcp_server_callback(int fd, short event __attribute__((unused)), void * data) {
	struct st_tcp_socket_server_v1 * self = data;

	int new_fd = -1;
	socklen_t length = 0;
	struct st_value_v1 * client_info;

	if (self->af == AF_INET) {
		struct sockaddr_in addr_v4;
		bzero(&addr_v4, sizeof(addr_v4));

		new_fd = accept(fd, (struct sockaddr *) &addr_v4, &length);

		char str_addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr_v4, str_addr, INET_ADDRSTRLEN);
		long long int port = ntohs(addr_v4.sin_port);

		client_info = st_value_pack_v1("{sssssi}", "type", "inet", "addr", str_addr, "port", port);
	} else {
		struct sockaddr_in6 addr_v6;
		bzero(&addr_v6, sizeof(addr_v6));

		new_fd = accept(fd, (struct sockaddr *) &addr_v6, &length);

		char str_addr[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &addr_v6, str_addr, INET6_ADDRSTRLEN);
		long long int port = ntohs(addr_v6.sin6_port);

		client_info = st_value_pack_v1("{sssssi}", "type", "inet6", "addr", str_addr, "port", port);
	}

	self->callback(fd, new_fd, client_info);

	st_value_free_v1(client_info);
}

