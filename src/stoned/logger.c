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

#include <stddef.h>

#include <libstone/process.h>

#include "logger.h"

static struct st_process logger;
static int logger_in = -1;
static int logger_out = -1;

static void std_logger_exit(void) __attribute__((destructor));


static void std_logger_exit() {
	st_process_free(&logger, 1);
}

void std_logger_start() {
	st_process_new(&logger, "logger", NULL, 0);
	logger_in = st_process_pipe_to(&logger);
	logger_out = st_process_pipe_from(&logger, st_process_stdout);
	st_process_close(&logger, st_process_stderr);
	st_process_start(&logger, 1);
}

