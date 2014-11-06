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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// inet_ntop, htons, ntohs
#include <arpa/inet.h>
// errno
#include <errno.h>
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

#include <libstoriqone/poll.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/value.h>

#include "tcp.h"

struct so_tcp_socket_server {
	int fd;
	so_socket_accept_f callback;
	int af;
};

static void so_socket_tcp_server_callback(int fd, short event, void * data);


int so_socket_tcp(struct so_value * config) {
	int stype = SOCK_STREAM;

	char * type = NULL;
	char * saddr = NULL;
	long long int port;

	if (so_value_unpack(config, "{sssi}", "address", &saddr, "port", &port) < 2) {
		free(saddr);
		return -1;
	}

	if (so_value_unpack(config, "{ss}", "type", &type) > 0 && !strcmp(type, "datagram"))
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

int so_socket_tcp6(struct so_value * config) {
	int stype = SOCK_STREAM;

	char * type = NULL;
	char * saddr = NULL;
	long long int port;

	if (so_value_unpack(config, "{sssi}", "address", &saddr, "port", &port) < 2) {
		free(saddr);
		return -1;
	}

	if (so_value_unpack(config, "{ss}", "type", &type) > 0 && !strcmp(type, "datagram"))
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

int so_socket_tcp_accept_and_close(int fd, struct so_value * config) {
	struct sockaddr_in addr_v4;
	bzero(&addr_v4, sizeof(addr_v4));
	socklen_t length;

	int new_fd = accept(fd, (struct sockaddr *) &addr_v4, &length);

	so_socket_tcp_close(fd, config);

	return new_fd;
}

int so_socket_tcp6_accept_and_close(int fd, struct so_value * config) {
	struct sockaddr_in6 addr_v6;
	bzero(&addr_v6, sizeof(addr_v6));
	socklen_t length;

	int new_fd = accept(fd, (struct sockaddr *) &addr_v6, &length);

	so_socket_tcp_close(fd, config);

	return new_fd;
}

int so_socket_tcp_close(int fd, struct so_value * config __attribute__((unused))) {
	return close(fd);
}

int so_socket_tcp6_close(int fd, struct so_value * config __attribute__((unused))) {
	return close(fd);
}

bool so_socket_tcp_server(struct so_value * config, so_socket_accept_f accept_callback) {
	int stype = SOCK_STREAM;

	char * type = NULL;
	char * saddr = NULL;
	long long int port;

	if (so_value_unpack(config, "{siss}", "port", &port, "address", &saddr) < 1)
		return -1;

	if (so_value_unpack(config, "{ss}", "type", &type) > 0 && !strcmp(type, "datagram"))
		stype = SOCK_DGRAM;
	free(type);

	int fd = -1, failed;
	if (saddr != NULL) {
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
		failed = getaddrinfo(saddr, NULL, &hint, &addr);
		if (failed != 0)
			return -1;

		struct addrinfo * ptr = addr;
		while (fd < 0 && ptr != NULL) {
			fd = socket(AF_INET, stype, 0);
			if (fd < 0) {
				ptr = ptr->ai_next;
				continue;
			}

			struct sockaddr_in * addr_v4 = (struct sockaddr_in *) ptr->ai_addr;
			addr_v4->sin_port = htons(port);

			int failed = bind(fd, ptr->ai_addr, ptr->ai_addrlen);
			if (failed != 0) {
				close(fd);
				fd = -1;
				ptr = ptr->ai_next;
			}
		}

		freeaddrinfo(addr);
		free(saddr);
	} else {
		fd = socket(AF_INET, stype, 0);
		if (fd < 0)
			return false;

		struct sockaddr_in addr_v4;
		bzero(&addr_v4, sizeof(addr_v4));
		addr_v4.sin_port = htons(port);

		int failed = bind(fd, (struct sockaddr *) &addr_v4, sizeof(addr_v4));
		if (failed != 0) {
			close(fd);
			fd = -1;
		}
	}

	if (fd < 0)
		return false;

	failed = listen(fd, 16);

	struct so_tcp_socket_server * self = malloc(sizeof(struct so_tcp_socket_server));
	self->fd = fd;
	self->callback = accept_callback;
	self->af = AF_INET;

	so_poll_register(fd, POLLIN | POLLPRI, so_socket_tcp_server_callback, self, free);

	return true;
}

bool so_socket_tcp6_server(struct so_value * config, so_socket_accept_f accept_callback) {
	int stype = SOCK_STREAM;

	char * type = NULL;
	char * saddr = NULL;
	long long int port;

	if (so_value_unpack(config, "{siss}", "port", &port, "address", &saddr) < 1)
		return -1;

	if (so_value_unpack(config, "{ss}", "type", &type) > 0 && !strcmp(type, "datagram"))
		stype = SOCK_DGRAM;
	free(type);

	int fd = -1, failed;
	if (saddr != NULL) {
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
		failed = getaddrinfo(saddr, NULL, &hint, &addr);
		if (failed != 0)
			return -1;

		struct addrinfo * ptr = addr;
		while (fd < 0 && ptr != NULL) {
			fd = socket(AF_INET6, stype, 0);
			if (fd < 0) {
				ptr = ptr->ai_next;
				continue;
			}

			struct sockaddr_in6 * addr_v6 = (struct sockaddr_in6 *) ptr->ai_addr;
			addr_v6->sin6_port = htons(port);

			int failed = bind(fd, ptr->ai_addr, ptr->ai_addrlen);
			if (failed != 0) {
				close(fd);
				fd = -1;
				ptr = ptr->ai_next;
			}
		}

		freeaddrinfo(addr);
	} else {
		fd = socket(AF_INET6, stype, 0);
		if (fd < 0)
			return false;

		struct sockaddr_in6 addr_v6;
		bzero(&addr_v6, sizeof(addr_v6));
		addr_v6.sin6_port = htons(port);

		int failed = bind(fd, (struct sockaddr *) &addr_v6, sizeof(addr_v6));
		if (failed != 0) {
			close(fd);
			fd = -1;
		}
	}

	if (fd < 0)
		return false;

	failed = listen(fd, 16);

	struct so_tcp_socket_server * self = malloc(sizeof(struct so_tcp_socket_server));
	self->fd = fd;
	self->callback = accept_callback;
	self->af = AF_INET6;

	so_poll_register(fd, POLLIN | POLLPRI, so_socket_tcp_server_callback, self, free);

	return true;
}

static void so_socket_tcp_server_callback(int fd, short event __attribute__((unused)), void * data) {
	struct so_tcp_socket_server * self = data;

	int new_fd = -1;
	socklen_t length = 0;
	struct so_value * client_info;

	if (self->af == AF_INET) {
		struct sockaddr_in addr_v4;
		bzero(&addr_v4, sizeof(addr_v4));

		new_fd = accept(fd, (struct sockaddr *) &addr_v4, &length);

		char str_addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr_v4, str_addr, INET_ADDRSTRLEN);
		long long int port = ntohs(addr_v4.sin_port);

		client_info = so_value_pack("{sssssi}", "type", "inet", "addr", str_addr, "port", port);
	} else {
		struct sockaddr_in6 addr_v6;
		bzero(&addr_v6, sizeof(addr_v6));

		new_fd = accept(fd, (struct sockaddr *) &addr_v6, &length);

		char str_addr[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &addr_v6, str_addr, INET6_ADDRSTRLEN);
		long long int port = ntohs(addr_v6.sin6_port);

		client_info = so_value_pack("{sssssi}", "type", "inet6", "addr", str_addr, "port", port);
	}

	self->callback(fd, new_fd, client_info);

	so_value_free(client_info);
}

int so_socket_server_temp_tcp(struct so_value * config) {
	int stype = SOCK_STREAM;

	char * type = NULL;
	char * saddr = NULL;
	long long int port;

	if (so_value_unpack(config, "{siss}", "port", &port, "address", &saddr) < 1)
		return -1;

	if (so_value_unpack(config, "{ss}", "type", &type) > 0 && !strcmp(type, "datagram"))
		stype = SOCK_DGRAM;
	free(type);

	int fd = -1, failed;
	if (saddr != NULL) {
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
		failed = getaddrinfo(saddr, NULL, &hint, &addr);
		if (failed != 0)
			return -1;

		struct addrinfo * ptr = addr;
		long long int first_port = port;

		while (fd < 0 && ptr != NULL) {
			port = first_port;

			while (fd < 0) {
				fd = socket(AF_INET, stype, 0);
				if (fd < 0) {
					ptr = ptr->ai_next;
					continue;
				}

				struct sockaddr_in * addr_v4 = (struct sockaddr_in *) ptr->ai_addr;
				addr_v4->sin_port = htons(port);

				int failed = bind(fd, ptr->ai_addr, ptr->ai_addrlen);
				if (failed != 0) {
					close(fd);
					fd = -1;
					ptr = ptr->ai_next;

					if (errno == EADDRINUSE) {
						port++;
					} else
						return -1;
				}
			}
		}

		so_value_hashtable_put2(config, "port", so_value_new_integer(port), true);

		freeaddrinfo(addr);
		free(saddr);
	} else {
		while (fd < 0) {
			fd = socket(AF_INET, stype, 0);
			if (fd < 0)
				return false;

			struct sockaddr_in addr_v4;
			bzero(&addr_v4, sizeof(addr_v4));
			addr_v4.sin_port = htons(port);

			int failed = bind(fd, (struct sockaddr *) &addr_v4, sizeof(addr_v4));
			if (failed != 0) {
				close(fd);
				fd = -1;

				if (errno == EADDRINUSE) {
					port++;
				} else
					return -1;
			}
		}

		so_value_hashtable_put2(config, "port", so_value_new_integer(port), true);
	}

	if (fd < 0)
		return -1;

	failed = listen(fd, 16);

	return fd;
}

int so_socket_server_temp_tcp6(struct so_value * config) {
	int stype = SOCK_STREAM;

	char * type = NULL;
	char * saddr = NULL;
	long long int port;

	if (so_value_unpack(config, "{siss}", "port", &port, "address", &saddr) < 1)
		return -1;

	if (so_value_unpack(config, "{ss}", "type", &type) > 0 && !strcmp(type, "datagram"))
		stype = SOCK_DGRAM;
	free(type);

	int fd = -1, failed;
	if (saddr != NULL) {
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
		failed = getaddrinfo(saddr, NULL, &hint, &addr);
		if (failed != 0)
			return -1;

		struct addrinfo * ptr = addr;
		long long int first_port = port;

		while (fd < 0 && ptr != NULL) {
			port = first_port;

			while (fd < 0) {
				fd = socket(AF_INET6, stype, 0);
				if (fd < 0) {
					ptr = ptr->ai_next;
					continue;
				}

				struct sockaddr_in6 * addr_v6 = (struct sockaddr_in6 *) ptr->ai_addr;
				addr_v6->sin6_port = htons(port);

				int failed = bind(fd, ptr->ai_addr, ptr->ai_addrlen);
				if (failed != 0) {
					close(fd);
					fd = -1;
					ptr = ptr->ai_next;

					if (errno == EADDRINUSE) {
						port++;
					} else
						return -1;
				}
			}
		}

		so_value_hashtable_put2(config, "port", so_value_new_integer(port), true);

		freeaddrinfo(addr);
		free(saddr);
	} else {
		while (fd < 0) {
			fd = socket(AF_INET6, stype, 0);
			if (fd < 0)
				return false;

			struct sockaddr_in6 addr_v6;
			bzero(&addr_v6, sizeof(addr_v6));
			addr_v6.sin6_port = htons(port);

			int failed = bind(fd, (struct sockaddr *) &addr_v6, sizeof(addr_v6));
			if (failed != 0) {
				close(fd);
				fd = -1;

				if (errno == EADDRINUSE) {
					port++;
				} else
					return -1;
			}
		}

		so_value_hashtable_put2(config, "port", so_value_new_integer(port), true);
	}

	if (fd < 0)
		return -1;

	failed = listen(fd, 16);

	return fd;
}

