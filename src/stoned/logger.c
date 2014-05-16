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

// free
#include <stdlib.h>
// strlen
#include <string.h>
// access, close, unlink, write
#include <unistd.h>

#include <libstone/json.h>
#include <libstone/log.h>
#include <libstone/poll.h>
#include <libstone/process.h>
#include <libstone/value.h>

#include "logger.h"


static struct st_process logger;
static int logger_in = -1;
static struct st_value * logger_config = NULL;

static void std_logger_exit(void) __attribute__((destructor));
static void std_logger_exited(int fd, short event, void * data);


static void std_logger_exit() {
	st_value_free(logger_config);

	if (logger_in < 0)
		return;

	close(logger_in);
	logger_in = -1;
	st_process_free(&logger, 1);
}

static void std_logger_exited(int fd __attribute__((unused)), short event, void * data __attribute__((unused))) {
	if (event & POLLERR) {
		st_poll_unregister(logger_in);
		close(logger_in);
	}

	logger_in = -1;
	st_process_wait(&logger, 1);
	st_process_free(&logger, 1);

	st_log_write2(st_log_level_critical, st_log_type_daemon, "Restart logger");

	std_logger_start(NULL);
}

bool std_logger_start(struct st_value * config) {
	if (logger_config == NULL)
		logger_config = st_value_share(config);

	struct st_value * socket = st_value_hashtable_get2(logger_config, "socket", false, false);
	if (socket == NULL || socket->type != st_value_hashtable)
		return false;

	struct st_value * path = st_value_hashtable_get2(socket, "path", false, false);
	if (path == NULL || path->type != st_value_string)
		return false;
	if (!access(st_value_string_get(path), F_OK))
		unlink(st_value_string_get(path));

	st_process_new(&logger, "logger", NULL, 0);
	logger_in = st_process_pipe_to(&logger);
	st_process_close(&logger, st_process_stdout);
	st_process_close(&logger, st_process_stderr);
	st_process_set_nice(&logger, 4);
	st_process_start(&logger, 1);

	st_poll_register(logger_in, POLLHUP, std_logger_exited, NULL, NULL);

	if (logger_config == NULL)
		logger_config = st_value_share(config);

	char * str_config = st_json_encode_to_string(logger_config);
	write(logger_in, str_config, strlen(str_config));
	free(str_config);

	return true;
}

void std_logger_stop() {
	struct st_value * stop = st_value_pack("{ss}", "command", "stop");
	st_json_encode_to_fd(stop, logger_in, true);
	st_value_free(stop);

	st_process_wait(&logger, 1);

	close(logger_in);
	logger_in = -1;

	st_process_free(&logger, 1);
}

