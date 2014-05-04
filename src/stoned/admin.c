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
// close
#include <unistd.h>

#include <libstone/json.h>
#include <libstone/log.h>
#include <libstone/poll.h>
#include <libstone/socket.h>
#include <libstone/value.h>

#include "admin.h"
#include "admin/common.h"

static struct st_value * std_admin_conf = NULL;
static struct st_value * std_admin_callbacks = NULL;

static void std_admin_client_do(int fd, short event, void * data);
static void std_admin_client_free(void * data);
static void std_admin_client_timeout(int fd, void * data);
static void std_admin_exit(void) __attribute__((destructor));
static void std_admin_new_connection(int fd_server, int fd_client, struct st_value * client);


void std_admin_config(struct st_value * config) {
	std_admin_conf = st_value_share(config);

	if (std_admin_callbacks == NULL) {
		std_admin_callbacks = st_value_new_hashtable2();
		st_value_hashtable_put2(std_admin_callbacks, "login", st_value_new_custom(std_admin_login, NULL), true);
		st_value_hashtable_put2(std_admin_callbacks, "shutdown", st_value_new_custom(std_admin_server_shutdown, NULL), true);
	}

	struct st_value * socket = st_value_hashtable_get2(config, "socket", false);
	st_socket_server(socket, std_admin_new_connection);
}

static void std_admin_client_do(int fd, short event, void * data) {
	if ((event & POLLERR) || (event & POLLHUP)) {
		std_admin_client_timeout(fd, data);
		return;
	}

	struct st_value * request = st_json_parse_fd(fd, 1000);
	if (request == NULL || !st_value_hashtable_has_key2(request, "method")) {
		if (request != NULL)
			st_value_free(request);

		st_poll_unregister(fd);
		close(fd);
		return;
	}

	struct st_value * method = st_value_hashtable_get2(request, "method", false);
	if (!st_value_hashtable_has_key(std_admin_callbacks, method)) {
		st_value_free(request);

		st_poll_unregister(fd);
		close(fd);
		return;
	}

	struct st_value * vmethod = st_value_hashtable_get(std_admin_callbacks, method, false, false);
	std_admin_f command = st_value_custom_get(vmethod);

	struct st_value * response = command(data, request, std_admin_conf);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
	st_value_free(request);
}

static void std_admin_client_free(void * data) {
	struct std_admin_client * self = data;
	free(self->salt);
	free(self);
}

static void std_admin_client_timeout(int fd, void * data __attribute__((unused))) {
	st_poll_unregister(fd);
	close(fd);
}

static void std_admin_exit() {
	if (std_admin_conf != NULL)
		st_value_free(std_admin_conf);
	std_admin_conf = NULL;

	if (std_admin_callbacks != NULL)
		st_value_free(std_admin_callbacks);
	std_admin_callbacks = NULL;
}

static void std_admin_new_connection(int fd_server __attribute__((unused)), int fd_client, struct st_value * client __attribute__((unused))) {
	st_log_write2(st_log_level_notice, st_log_type_daemon, "New connection 'admin'");

	struct std_admin_client * new_client = malloc(sizeof(struct std_admin_client));
	new_client->logged = false;
	new_client->salt = NULL;

	st_poll_register(fd_client, POLLIN | POLLERR | POLLHUP, std_admin_client_do, new_client, std_admin_client_free);
	st_poll_set_timeout(fd_client, 600000, std_admin_client_timeout);
}

