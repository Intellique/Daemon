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

// close, unlink
#include <unistd.h>

#include <libstone/json.h>
#include <libstone/log.h>
#include <libstone/poll.h>
#include <libstone/process.h>
#include <libstone/value.h>

#include "config.h"
#include "logger.h"

#define SOCKET_PATH DAEMON_SOCKET_DIR "/logger.socket"


static struct st_process logger;
static int logger_in = -1;
static struct st_value * logger_config = NULL;

static void std_logger_exit(void) __attribute__((destructor));
static void std_logger_exited(int fd, short event, void * data);


static void std_logger_exit() {
	if (logger_in < 0)
		return;

	close(logger_in);
	st_process_free(&logger, 1);

	st_value_free(logger_config);
}

static void std_logger_exited(int fd __attribute__((unused)), short event __attribute__((unused)), void * data __attribute__((unused))) {
	logger_in = -1;
	st_process_free(&logger, 1);

	st_log_write(st_log_level_critical, st_log_type_daemon, "Restart logger");

	std_logger_start(NULL);
}

struct st_value * std_logger_get_config() {
	if (logger_config == NULL)
		return NULL;

	return st_value_hashtable_get2(logger_config, "socket", true);
}

void std_logger_start(struct st_value * config) {
	unlink(SOCKET_PATH);

	st_process_new(&logger, "logger", NULL, 0);
	logger_in = st_process_pipe_to(&logger);
	st_process_close(&logger, st_process_stdout);
	st_process_close(&logger, st_process_stderr);
	st_process_set_nice(&logger, 4);
	st_process_start(&logger, 1);

	st_poll_register(logger_in, POLLHUP, std_logger_exited, NULL, NULL);

	if (logger_config == NULL)
		logger_config = st_value_pack("{sos{ssss}}", "module", st_value_hashtable_get2(config, "log", true), "socket", "domain", "unix", "path", SOCKET_PATH);

	st_json_encode_to_fd(logger_config, logger_in);
}

