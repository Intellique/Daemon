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

#define _GNU_SOURCE
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// close
#include <unistd.h>

#include <libstone/changer.h>
#include <libstone/json.h>
#include <libstone/log.h>
#include <libstone/database.h>
#include <libstone/poll.h>
#include <libstone/process.h>
#include <libstone/value.h>

#include "device.h"

#include "config.h"

struct std_device {
	char * process_name;
	struct st_process process;
	int fd_in;
	struct st_value * config;
};

static struct st_value * devices = NULL;
static struct st_value * devices_json = NULL;

static void std_device_free(void * d);
static void std_device_exited(int fd, short event, void * data);


void std_device_configure(struct st_value * logger, struct st_value * db_config, struct st_database_connection * connection) {
	if (connection == NULL)
		return;

	if (devices == NULL) {
		devices = st_value_new_linked_list();
		devices_json = st_value_new_linked_list();
	}

	static int i_changer = 0, i_drives = 0;

	// get scsi changers
	struct st_value * real_changers = connection->ops->get_changers(connection);
	struct st_value_iterator * iter = st_value_list_get_iterator(real_changers);
	while (st_value_iterator_has_next(iter)) {
		struct st_value * changer = st_value_iterator_get_value(iter, true);

		struct std_device * dev = malloc(sizeof(struct std_device));
		dev->process_name = "scsi_changer";
		st_process_new(&dev->process, dev->process_name, NULL, 0);
		dev->fd_in = st_process_pipe_to(&dev->process);
		st_process_close(&dev->process, st_process_stdout);
		st_process_close(&dev->process, st_process_stderr);

		st_poll_register(dev->fd_in, POLLHUP, std_device_exited, dev, NULL);

		char * path;
		asprintf(&path, DAEMON_SOCKET_DIR "/changer_%d.socket", i_changer);

		st_value_hashtable_put2(changer, "socket", st_value_pack("{ssss}", "domain", "unix", "path", path), true);
		dev->config = st_value_pack("{sOsOsO}", "changer", changer, "logger", logger, "database", db_config);

		free(path);

		struct st_value * drives;
		st_value_unpack(changer, "{so}", "drives", &drives);
		struct st_value_iterator * iter = st_value_list_get_iterator(drives);
		while (st_value_iterator_has_next(iter)) {
			struct st_value * drive = st_value_iterator_get_value(iter, false);

			asprintf(&path, "run/changer_%d_drive_%d.socket", i_changer, i_drives);
			i_drives++;

			st_value_hashtable_put2(drive, "socket", st_value_pack("{ssss}", "domain", "unix", "path", path), true);

			free(path);
		}
		st_value_iterator_free(iter);

		st_value_list_push(devices, st_value_new_custom(dev, std_device_free), true);
		st_value_list_push(devices_json, changer, false);

		st_process_start(&dev->process, 1);
		st_json_encode_to_fd(dev->config, dev->fd_in, true);

		i_changer++;
	}
	st_value_iterator_free(iter);
	st_value_free(real_changers);

	// get standalone drives

	// get virtual tape libraries
	struct st_value * vtl_changers = connection->ops->get_vtls(connection);
	iter = st_value_list_get_iterator(vtl_changers);
	while (st_value_iterator_has_next(iter)) {
		struct st_value * changer = st_value_iterator_get_value(iter, true);

		struct st_value * drives = NULL;
		long long nb_drives = 0;
		st_value_unpack(changer, "{sosi}", "drives", &drives, "nb drives", &nb_drives);

		long long int i;
		for (i = 0; i < nb_drives; i++) {
			char * path;
			asprintf(&path, "run/changer_%d_drive_%d.socket", i_changer, i_drives);
			i_drives++;

			st_value_list_push(drives, st_value_pack("{s{ssss}}", "socket", "domain", "unix", "path", path), true);

			free(path);
		}

		struct std_device * dev = malloc(sizeof(struct std_device));
		dev->process_name = "vtl_changer";
		st_process_new(&dev->process, dev->process_name, NULL, 0);
		dev->fd_in = st_process_pipe_to(&dev->process);
		st_process_close(&dev->process, st_process_stdout);
		st_process_close(&dev->process, st_process_stderr);

		st_poll_register(dev->fd_in, POLLHUP, std_device_exited, dev, NULL);

		char * path;
		asprintf(&path, DAEMON_SOCKET_DIR "/changer_%d.socket", i_changer);

		st_value_hashtable_put2(changer, "socket", st_value_pack("{ssss}", "domain", "unix", "path", path), true);
		dev->config = st_value_pack("{sOsOsO}", "changer", changer, "logger", logger, "database", db_config);

		free(path);
	}
	st_value_iterator_free(iter);
	st_value_free(vtl_changers);
}

static void std_device_exited(int fd __attribute__((unused)), short event, void * data) {
	struct std_device * dev = data;

	if (event & POLLERR) {
		st_poll_unregister(dev->fd_in, POLLHUP);
		close(dev->fd_in);
	}

	dev->fd_in = -1;
	st_process_wait(&dev->process, 1);
	st_process_free(&dev->process, 1);

	st_log_write2(st_log_level_critical, st_log_type_daemon, "Restart changer: %s", dev->process_name);

	st_process_new(&dev->process, dev->process_name, NULL, 0);
	dev->fd_in = st_process_pipe_to(&dev->process);
	st_process_close(&dev->process, st_process_stdout);
	st_process_close(&dev->process, st_process_stderr);

	st_poll_register(dev->fd_in, POLLHUP, std_device_exited, dev, NULL);

	st_process_start(&dev->process, 1);
	st_json_encode_to_fd(dev->config, dev->fd_in, true);
}

static void std_device_free(void * d) {
	struct std_device * dev = d;
	st_process_free(&dev->process, 1);
	close(dev->fd_in);
	st_value_free(dev->config);
	free(dev);
}

struct st_value * std_device_get(bool shared) {
	if (shared)
		return st_value_share(devices_json);
	return devices_json;
}

void std_device_stop() {
	struct st_value * stop = st_value_pack("{ss}", "command", "stop");

	struct st_value_iterator * iter = st_value_list_get_iterator(devices);
	while (st_value_iterator_has_next(iter)) {
		struct st_value * elt = st_value_iterator_get_value(iter, false);
		struct std_device * dev = st_value_custom_get(elt);

		st_json_encode_to_fd(stop, dev->fd_in, true);
		st_poll_unregister(dev->fd_in, POLLHUP);
	}
	st_value_iterator_free(iter);

	st_value_free(stop);
	st_value_free(devices);
}

