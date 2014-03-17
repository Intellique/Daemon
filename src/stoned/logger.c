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

// close
#include <unistd.h>

#include <libstone/json.h>
#include <libstone/process.h>
#include <libstone/value.h>

#include "config.h"
#include "logger.h"

static struct st_process logger;
static int logger_in = -1;
static int logger_out = -1;
static struct st_value * logger_config = NULL;

static void std_logger_exit(void) __attribute__((destructor));


static void std_logger_exit() {
	close(logger_in);
	close(logger_out);
	st_process_free(&logger, 1);

	st_value_free(logger_config);
}

struct st_value * std_logger_get_config() {
	if (logger_config == NULL)
		return NULL;

	return st_value_hashtable_get2(logger_config, "socket", true);
}

void std_logger_start(struct st_value * config) {
	st_process_new(&logger, "logger", NULL, 0);
	logger_in = st_process_pipe_to(&logger);
	logger_out = st_process_pipe_from(&logger, st_process_stdout);
	st_process_close(&logger, st_process_stderr);
	st_process_set_nice(&logger, 4);
	st_process_start(&logger, 1);

	if (logger_config == NULL)
		logger_config = st_value_pack("{sos{ssss}}", "module", st_value_hashtable_get2(config, "log", true), "socket", "type", "unix", "path", "run/logger.socket");

	st_json_encode_to_fd(logger_config, logger_in);
}

