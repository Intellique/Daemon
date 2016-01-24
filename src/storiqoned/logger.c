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
// free
#include <stdlib.h>
// strlen
#include <string.h>
// access, close, unlink, write
#include <unistd.h>

#include <libstoriqone/file.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/process.h>
#include <libstoriqone/value.h>

#include "logger.h"


static struct so_process logger;
static int logger_in = -1;
static struct so_value * logger_config = NULL;

static void sod_logger_exit(void) __attribute__((destructor));
static void sod_logger_exited(int fd, short event, void * data);


static void sod_logger_exit() {
	so_value_free(logger_config);

	if (logger_in < 0)
		return;

	close(logger_in);
	logger_in = -1;
	so_process_free(&logger, 1);
}

static void sod_logger_exited(int fd __attribute__((unused)), short event, void * data __attribute__((unused))) {
	if (event & POLLERR) {
		so_poll_unregister(logger_in, POLLHUP);
		close(logger_in);
	}

	logger_in = -1;
	so_process_wait(&logger, 1, true);
	so_process_free(&logger, 1);

	so_log_write2(so_log_level_critical, so_log_type_daemon, gettext("Restarting logger"));

	sod_logger_start(NULL);
}

bool sod_logger_start(struct so_value * config) {
	if (logger_config == NULL)
		logger_config = so_value_share(config);

	struct so_value * socket = so_value_hashtable_get2(logger_config, "socket", false, false);
	if (socket == NULL || socket->type != so_value_hashtable)
		return false;

	struct so_value * path = so_value_hashtable_get2(socket, "path", false, false);
	if (path == NULL || path->type != so_value_string)
		return false;
	if (!access(so_value_string_get(path), F_OK))
		unlink(so_value_string_get(path));

	so_process_new(&logger, "logger", NULL, 0);
	logger_in = so_process_pipe_to(&logger);
	so_file_close_fd_on_exec(logger_in, true);
	so_process_set_null(&logger, so_process_stdout);
	so_process_set_null(&logger, so_process_stderr);
	so_process_set_nice(&logger, 4);
	so_process_start(&logger, 1);

	so_poll_register(logger_in, POLLHUP, sod_logger_exited, NULL, NULL);

	if (logger_config == NULL)
		logger_config = so_value_share(config);

	so_json_encode_to_fd(logger_config, logger_in, true);

	return true;
}

void sod_logger_stop() {
	struct so_value * stop = so_value_pack("{ss}", "command", "stop");
	so_json_encode_to_fd(stop, logger_in, true);
	so_value_free(stop);

	so_process_wait(&logger, 1, true);

	close(logger_in);
	logger_in = -1;

	so_process_free(&logger, 1);
}

