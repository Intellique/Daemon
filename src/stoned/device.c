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

#include <libstone/json.h>
#include <libstone/database.h>
#include <libstone/poll.h>
#include <libstone/process.h>
#include <libstone/value.h>

#include "device.h"

struct std_device {
	struct st_process process;
	int fd_in;
	struct st_value * config;
};

static struct st_value * devices = NULL;

static void std_device_free(void * d);
static void std_device_exited(int fd, short event, void * data);


void std_device_configure(struct st_value * logger, struct st_value * db_config) {
	struct st_database * db = st_database_get_default_driver();
	if (db == NULL)
		return;

	struct st_database_config * config = db->ops->get_default_config();
	if (config == NULL)
		return;

	struct st_database_connection * connection = config->ops->connect(config);
	if (connection == NULL)
		return;

	if (devices == NULL)
		devices = st_value_new_linked_list();

	struct st_value * real_changers = connection->ops->get_changers(connection);
	struct st_value_iterator * iter = st_value_list_get_iterator(real_changers);
	while (st_value_iterator_has_next(iter)) {
		struct st_value * changer = st_value_iterator_get_value(iter, true);

		struct std_device * dev = malloc(sizeof(struct std_device));
		st_process_new(&dev->process, "scsi_changer", NULL, 0);
		dev->fd_in = st_process_pipe_to(&dev->process);
		st_process_close(&dev->process, st_process_stdout);
		st_process_close(&dev->process, st_process_stderr);

		st_poll_register(dev->fd_in, POLLHUP, std_device_exited, dev, std_device_free);

		static int i_changer = 0;
		char * path;
		asprintf(&path, "run/changer_%d.socket", i_changer);
		i_changer++;

		dev->config = st_value_pack("{sOsOsOs{ssss}}", "changer", changer, "logger", logger, "database", db_config, "socket", "domain", "unix", "path", path);

		st_value_list_push(devices, st_value_new_custom(dev, std_device_free), true);

		st_process_start(&dev->process, 1);
		st_json_encode_to_fd(dev->config, dev->fd_in, true);

		free(path);
	}
	st_value_iterator_free(iter);
}

static void std_device_exited(int fd, short event, void * data) {
}

static void std_device_free(void * d) {
	struct std_device * dev = d;
	st_process_free(&dev->process, 1);
	close(dev->fd_in);
	st_value_free(dev->config);
	free(dev);
}

void std_device_stop() {
}

