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

// gettext
#include <libintl.h>
// free, malloc
#include <stdlib.h>
// close
#include <unistd.h>

#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/value.h>

#include "admin.h"
#include "admin/common.h"

static struct so_value * sod_admin_conf = NULL;
static struct so_value * sod_admin_callbacks = NULL;

static void sod_admin_client_do(int fd, short event, void * data);
static void sod_admin_client_free(void * data);
static void sod_admin_client_timeout(int fd, void * data);
static void sod_admin_exit(void) __attribute__((destructor));
static void sod_admin_new_connection(int fd_server, int fd_client, struct so_value * client);


void sod_admin_config(struct so_value * config) {
	sod_admin_conf = so_value_share(config);

	if (sod_admin_callbacks == NULL) {
		sod_admin_callbacks = so_value_new_hashtable2();
		so_value_hashtable_put2(sod_admin_callbacks, "login", so_value_new_custom(sod_admin_login, NULL), true);
		so_value_hashtable_put2(sod_admin_callbacks, "shutdown", so_value_new_custom(sod_admin_server_shutdown, NULL), true);
	}

	struct so_value * socket = so_value_hashtable_get2(config, "socket", false, false);
	so_socket_server(socket, sod_admin_new_connection);
}

static void sod_admin_client_do(int fd, short event, void * data) {
	if ((event & POLLERR) || (event & POLLHUP)) {
		sod_admin_client_timeout(fd, data);
		return;
	}

	struct so_value * request = so_json_parse_fd(fd, 1000);
	if (request == NULL || !so_value_hashtable_has_key2(request, "method")) {
		if (request != NULL)
			so_value_free(request);

		so_poll_unregister(fd, POLLIN | POLLERR | POLLHUP);
		close(fd);
		return;
	}

	struct so_value * method = so_value_hashtable_get2(request, "method", false, false);
	if (!so_value_hashtable_has_key(sod_admin_callbacks, method)) {
		so_value_free(request);

		so_poll_unregister(fd, POLLIN | POLLERR | POLLHUP);
		close(fd);
		return;
	}

	struct so_value * vmethod = so_value_hashtable_get(sod_admin_callbacks, method, false, false);
	sod_admin_f command = so_value_custom_get(vmethod);

	struct so_value * response = command(data, request, sod_admin_conf);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
	so_value_free(request);
}

static void sod_admin_client_free(void * data) {
	struct sod_admin_client * self = data;
	free(self->salt);
	free(self);
}

static void sod_admin_client_timeout(int fd, void * data __attribute__((unused))) {
	so_poll_unregister(fd, POLLIN | POLLERR | POLLHUP);
	close(fd);
}

static void sod_admin_exit() {
	if (sod_admin_conf != NULL)
		so_value_free(sod_admin_conf);
	sod_admin_conf = NULL;

	if (sod_admin_callbacks != NULL)
		so_value_free(sod_admin_callbacks);
	sod_admin_callbacks = NULL;
}

static void sod_admin_new_connection(int fd_server __attribute__((unused)), int fd_client, struct so_value * client __attribute__((unused))) {
	so_log_write2(so_log_level_notice, so_log_type_daemon, gettext("New connection 'admin'"));

	struct sod_admin_client * new_client = malloc(sizeof(struct sod_admin_client));
	new_client->logged = false;
	new_client->salt = NULL;

	so_poll_register(fd_client, POLLIN | POLLERR | POLLHUP, sod_admin_client_do, new_client, sod_admin_client_free);
	so_poll_set_timeout(fd_client, 600000, sod_admin_client_timeout);
}

