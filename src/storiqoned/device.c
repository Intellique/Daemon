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
*  Copyright (C) 2013-2019, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// gettext
#include <libintl.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// close, sleep
#include <unistd.h>

#include <libstoriqone/changer.h>
#include <libstoriqone/database.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/process.h>
#include <libstoriqone/value.h>

#include "device.h"

#include "config.h"

struct sod_device {
	unsigned int index;
	char * process_name;
	struct so_process process;
	int fd_in;
	int fd_out;
	struct so_value * config;
};

static struct so_value * devices = NULL;
static struct so_value * devices_json = NULL;
static unsigned int nb_total_drives = 0;

static void sod_device_free(void * d);
static void sod_device_exited(int fd, short event, void * data);
static void sod_device_pong(int fd, short event, void * data);


void sod_device_configure(struct so_value * logger, struct so_value * db_config, struct so_value * default_values, struct so_database_connection * connection, bool append) {
	if (connection == NULL)
		return;

	if (devices == NULL) {
		devices = so_value_new_linked_list();
		devices_json = so_value_new_linked_list();
	}

	int i_changer = so_value_list_get_length(devices);
	bool has_new = false;

	if (!append) {
		// get scsi changers
		struct so_value * real_changers = connection->ops->get_changers(connection);
		struct so_value_iterator * iter = so_value_list_get_iterator(real_changers);
		while (so_value_iterator_has_next(iter)) {
			has_new = true;

			struct so_value * changer = so_value_iterator_get_value(iter, true);

			/**
			 * valgrind
			 * valgrind -v --log-file=valgrind.log --num-callers=24 --leak-check=full --show-reachable=yes --track-origins=yes ./bin/stoned
			 *
			 * const char * params[] = { "-v", "--log-file=valgrind/scsi_changer.log", "--track-fds=yes", "--time-stamp=yes", "--num-callers=24", "--leak-check=full", "--show-reachable=yes", "--track-origins=yes", "--fullpath-after=/home/guillaume/prog/StoriqOne/", "scsi_changer" };
			 * so_process_new(&dev->process, "valgrind", params, 10);
			 */

			struct sod_device * dev = malloc(sizeof(struct sod_device));
			dev->index = i_changer;
			dev->process_name = "scsi_changer";
			so_process_new(&dev->process, dev->process_name, NULL, 0);
			dev->fd_in = so_process_pipe_to(&dev->process);
			dev->fd_out = so_process_pipe_from(&dev->process, so_process_stdout);
			so_process_set_null(&dev->process, so_process_stderr);

			so_poll_register(dev->fd_out, POLLIN, sod_device_pong, dev, NULL);
			so_poll_register(dev->fd_in, POLLHUP, sod_device_exited, dev, NULL);

			char * path = NULL;
			int size = asprintf(&path, DAEMON_SOCKET_DIR "/changer_%d.socket", i_changer);
			if (size < 0)
				continue;

			so_value_hashtable_put2(changer, "socket", so_value_pack("{ssss}", "domain", "unix", "path", path), true);
			dev->config = so_value_pack("{sOsOsOsO}", "changer", changer, "logger", logger, "database", db_config, "default values", default_values);

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
		struct so_value * standalone_changers = connection->ops->get_standalone_drives(connection);
		iter = so_value_list_get_iterator(standalone_changers);
		while (so_value_iterator_has_next(iter)) {
			has_new = true;

			struct so_value * changer = so_value_iterator_get_value(iter, true);

			struct sod_device * dev = malloc(sizeof(struct sod_device));
			dev->index = i_changer;
			dev->process_name = "standalone_changer";
			so_process_new(&dev->process, dev->process_name, NULL, 0);
			dev->fd_in = so_process_pipe_to(&dev->process);
			dev->fd_out = so_process_pipe_from(&dev->process, so_process_stdout);
			so_process_set_null(&dev->process, so_process_stderr);

			so_poll_register(dev->fd_out, POLLIN, sod_device_pong, dev, NULL);
			so_poll_register(dev->fd_in, POLLHUP, sod_device_exited, dev, NULL);

			char * path = NULL;
			int size = asprintf(&path, DAEMON_SOCKET_DIR "/changer_%d.socket", i_changer);
			if (size < 0)
				continue;

			so_value_hashtable_put2(changer, "socket", so_value_pack("{ssss}", "domain", "unix", "path", path), true);
			dev->config = so_value_pack("{sOsOsOsO}", "changer", changer, "logger", logger, "database", db_config, "default values", default_values);

			free(path);

			so_value_list_push(devices, so_value_new_custom(dev, sod_device_free), true);
			so_value_list_push(devices_json, changer, false);

			so_process_start(&dev->process, 1);
			so_json_encode_to_fd(dev->config, dev->fd_in, true);

			i_changer++;
		}
		so_value_iterator_free(iter);
		so_value_free(standalone_changers);
	}

	// get virtual tape libraries
	struct so_value * vtl_changers = connection->ops->get_vtls(connection, append);
	struct so_value_iterator * iter = so_value_list_get_iterator(vtl_changers);
	while (so_value_iterator_has_next(iter)) {
		has_new = true;

		struct so_value * changer = so_value_iterator_get_value(iter, true);

		/**
		 * valgrind
		 * valgrind -v --log-file=valgrind.log --num-callers=24 --leak-check=full --show-reachable=yes --track-origins=yes ./bin/stoned
		 *
		 * const char * params[] = { "-v", "--log-file=valgrind/vtl.log", "--track-fds=yes", "--time-stamp=yes", "--num-callers=24", "--leak-check=full", "--show-reachable=yes", "--track-origins=yes", "--fullpath-after=/home/guillaume/prog/StoriqOne/", "vtl_changer" };
		 * so_process_new(&dev->process, "valgrind", params, 10);
		 */

		struct sod_device * dev = malloc(sizeof(struct sod_device));
		dev->index = i_changer;
		dev->process_name = "vtl_changer";
		so_process_new(&dev->process, dev->process_name, NULL, 0);
		dev->fd_in = so_process_pipe_to(&dev->process);
		dev->fd_out = so_process_pipe_from(&dev->process, so_process_stdout);
		so_process_set_null(&dev->process, so_process_stderr);

		so_poll_register(dev->fd_out, POLLIN, sod_device_pong, dev, NULL);
		so_poll_register(dev->fd_in, POLLHUP, sod_device_exited, dev, NULL);

		char * path = NULL;
		int size = asprintf(&path, DAEMON_SOCKET_DIR "/changer_%d.socket", i_changer);
		if (size < 0)
			continue;

		so_value_hashtable_put2(changer, "socket", so_value_pack("{ssss}", "domain", "unix", "path", path), true);
		dev->config = so_value_pack("{sOsOsOsO}", "changer", changer, "logger", logger, "database", db_config, "default values", default_values);

		free(path);

		so_value_list_push(devices, so_value_new_custom(dev, sod_device_free), true);
		so_value_list_push(devices_json, changer, false);

		so_process_start(&dev->process, 1);
		so_json_encode_to_fd(dev->config, dev->fd_in, true);

		i_changer++;
	}
	so_value_iterator_free(iter);
	so_value_free(vtl_changers);

	if (!has_new)
		return;

	sleep(5);

	nb_total_drives = 0;

	iter = so_value_list_get_iterator(devices);
	struct so_value * cmd_count = so_value_pack("{ss}", "command", "get number of drives");
	while (so_value_iterator_has_next(iter)) {
		struct so_value * elt = so_value_iterator_get_value(iter, false);
		struct sod_device * dev = so_value_custom_get(elt);

		so_json_encode_to_fd(cmd_count, dev->fd_in, true);

		unsigned int nb_drives = 0;
		struct so_value * response = so_json_parse_fd(dev->fd_out, -1);
		so_value_unpack(response, "{su}", "return", &nb_drives);
		so_value_free(response);

		nb_total_drives += nb_drives;
	}
	so_value_free(cmd_count);
	so_value_iterator_free(iter);
}

static void sod_device_exited(int fd __attribute__((unused)), short event, void * data) {
	struct sod_device * dev = data;

	if (event & POLLERR) {
		so_poll_unregister(dev->fd_in, POLLHUP);
		close(dev->fd_in);
	}

	dev->fd_in = -1;
	so_process_wait(&dev->process, 1, true);
	so_process_free(&dev->process, 1);

	so_log_write2(so_log_level_critical, so_log_type_daemon, gettext("Restarting changer: %s"), dev->process_name);

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
	close(dev->fd_out);
	so_value_free(dev->config);
	free(dev);
}

struct so_value * sod_device_get(bool shared) {
	if (shared)
		return so_value_share(devices_json);
	return devices_json;
}

unsigned int sod_device_get_nb_drives() {
	return nb_total_drives;
}

static void sod_device_pong(int fd, short event __attribute__((unused)), void * data) {
	struct so_value * command = so_json_parse_fd(fd, -1);
	if (command == NULL)
		return;

	const char * com = NULL;
	so_value_unpack(command, "{sS}", "command", &com);
	so_value_free(command);

	if (com == NULL || strcmp(com, "exit") != 0)
		return;

	struct sod_device * dev = data;
	so_poll_unregister(dev->fd_out, POLLIN);
	so_poll_unregister(dev->fd_in, POLLHUP);
	so_process_wait(&dev->process, 1, true);

	unsigned int index = dev->index;
	so_value_list_remove(devices, index);
	so_value_list_remove(devices_json, index);

	struct so_value_iterator * iter = so_value_list_get_iterator(devices);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * val = so_value_iterator_get_value(iter, false);
		dev = so_value_custom_get(val);

		if (dev->index > index)
			dev->index--;
	}
	so_value_iterator_free(iter);
}

void sod_device_stop() {
	struct so_value * stop = so_value_pack("{ss}", "command", "stop");

	struct so_value_iterator * iter = so_value_list_get_iterator(devices);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * elt = so_value_iterator_get_value(iter, false);
		struct sod_device * dev = so_value_custom_get(elt);

		so_poll_unregister(dev->fd_in, POLLHUP);
		so_json_encode_to_fd(stop, dev->fd_in, true);
	}
	so_value_iterator_free(iter);

	so_value_free(stop);
	so_value_free(devices);
}
