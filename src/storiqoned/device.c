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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// gettext
#include <libintl.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// close, sleep
#include <unistd.h>

#include <libstoriqone/changer.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/database.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/process.h>
#include <libstoriqone/value.h>

#include "device.h"

#include "config.h"

struct sod_device {
	char * process_name;
	struct so_process process;
	int fd_in;
	int fd_out;
	struct so_value * config;
};

static struct so_value * devices = NULL;
static struct so_value * devices_json = NULL;

static void sod_device_free(void * d);
static void sod_device_exited(int fd, short event, void * data);


void sod_device_configure(struct so_value * logger, struct so_value * db_config, struct so_database_connection * connection) {
	if (connection == NULL)
		return;

	if (devices == NULL) {
		devices = so_value_new_linked_list();
		devices_json = so_value_new_linked_list();
	}

	static int i_changer = 0;

	// get scsi changers
	struct so_value * real_changers = connection->ops->get_changers(connection);
	struct so_value_iterator * iter = so_value_list_get_iterator(real_changers);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * changer = so_value_iterator_get_value(iter, true);

		struct sod_device * dev = malloc(sizeof(struct sod_device));
		dev->process_name = "scsi_changer";
		so_process_new(&dev->process, dev->process_name, NULL, 0);
		dev->fd_in = so_process_pipe_to(&dev->process);
		dev->fd_out = so_process_pipe_from(&dev->process, so_process_stdout);
		so_process_set_null(&dev->process, so_process_stderr);

		so_poll_register(dev->fd_in, POLLHUP, sod_device_exited, dev, NULL);

		char * path;
		asprintf(&path, DAEMON_SOCKET_DIR "/changer_%d.socket", i_changer);

		so_value_hashtable_put2(changer, "socket", so_value_pack("{ssss}", "domain", "unix", "path", path), true);
		dev->config = so_value_pack("{sOsOsO}", "changer", changer, "logger", logger, "database", db_config);

		free(path);

		so_value_list_push(devices, so_value_new_custom(dev, sod_device_free), true);
		so_value_list_push(devices_json, changer, false);

		so_process_start(&dev->process, 1);
		so_json_encode_to_fd(dev->config, dev->fd_in, true);

		i_changer++;
	}
	so_value_iterator_free(iter);
	so_value_free(real_changers);

	// get standalone drives

	// get virtual tape libraries
	struct so_value * vtl_changers = connection->ops->get_vtls(connection);
	iter = so_value_list_get_iterator(vtl_changers);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * changer = so_value_iterator_get_value(iter, true);

		struct sod_device * dev = malloc(sizeof(struct sod_device));
		dev->process_name = "vtl_changer";
		so_process_new(&dev->process, dev->process_name, NULL, 0);
		dev->fd_in = so_process_pipe_to(&dev->process);
		dev->fd_out = so_process_pipe_from(&dev->process, so_process_stdout);
		so_process_set_null(&dev->process, so_process_stderr);

		so_poll_register(dev->fd_in, POLLHUP, sod_device_exited, dev, NULL);

		char * path;
		asprintf(&path, DAEMON_SOCKET_DIR "/changer_%d.socket", i_changer);

		so_value_hashtable_put2(changer, "socket", so_value_pack("{ssss}", "domain", "unix", "path", path), true);
		dev->config = so_value_pack("{sOsOsO}", "changer", changer, "logger", logger, "database", db_config);

		free(path);

		so_value_list_push(devices, so_value_new_custom(dev, sod_device_free), true);
		so_value_list_push(devices_json, changer, false);

		so_process_start(&dev->process, 1);
		so_json_encode_to_fd(dev->config, dev->fd_in, true);

		i_changer++;
	}
	so_value_iterator_free(iter);
	so_value_free(vtl_changers);

	sleep(5);

	iter = so_value_list_get_iterator(devices);
	struct so_value * cmd_ping = so_value_pack("{ss}", "command", "ping");
	while (so_value_iterator_has_next(iter)) {
		struct so_value * elt = so_value_iterator_get_value(iter, false);
		struct sod_device * dev = so_value_custom_get(elt);

		so_json_encode_to_fd(cmd_ping, dev->fd_in, true);

		struct so_value * response = so_json_parse_fd(dev->fd_out, -1);
		// TODO: check response
		so_value_free(response);
	}
	so_value_free(cmd_ping);
	so_value_iterator_free(iter);
}

static void sod_device_exited(int fd __attribute__((unused)), short event, void * data) {
	struct sod_device * dev = data;

	if (event & POLLERR) {
		so_poll_unregister(dev->fd_in, POLLHUP);
		close(dev->fd_in);
	}

	dev->fd_in = -1;
	so_process_wait(&dev->process, 1);
	so_process_free(&dev->process, 1);

	so_log_write2(so_log_level_critical, so_log_type_daemon, gettext("Restart changer: %s"), dev->process_name);

	so_process_new(&dev->process, dev->process_name, NULL, 0);
	dev->fd_in = so_process_pipe_to(&dev->process);
	so_process_close(&dev->process, so_process_stdout);
	so_process_close(&dev->process, so_process_stderr);

	so_poll_register(dev->fd_in, POLLHUP, sod_device_exited, dev, NULL);

	so_process_start(&dev->process, 1);
	so_json_encode_to_fd(dev->config, dev->fd_in, true);
}

static void sod_device_free(void * d) {
	struct sod_device * dev = d;
	so_process_free(&dev->process, 1);
	close(dev->fd_in);
	so_value_free(dev->config);
	free(dev);
}

struct so_value * sod_device_get(bool shared) {
	if (shared)
		return so_value_share(devices_json);
	return devices_json;
}

void sod_device_stop() {
	struct so_value * stop = so_value_pack("{ss}", "command", "stop");

	struct so_value_iterator * iter = so_value_list_get_iterator(devices);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * elt = so_value_iterator_get_value(iter, false);
		struct sod_device * dev = so_value_custom_get(elt);

		so_json_encode_to_fd(stop, dev->fd_in, true);
		so_poll_unregister(dev->fd_in, POLLHUP);
	}
	so_value_iterator_free(iter);

	so_value_free(stop);
	so_value_free(devices);
}

