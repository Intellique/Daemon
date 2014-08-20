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
// pthread_sigmask, sigaddset, sigemptyset
#include <signal.h>
// asprintf
#include <stdio.h>
// free, getenv, setenv
#include <stdlib.h>
// strstr
#include <string.h>
// access
#include <unistd.h>

#include <libstone/file.h>

#include "env.h"

#include "config.h"

bool std_env_setup() {
	if (!access(DAEMON_SOCKET_DIR, F_OK))
		st_file_rm(DAEMON_SOCKET_DIR);

	if (st_file_mkdir(DAEMON_SOCKET_DIR, 0700) != 0)
		return false;

	char * path = getenv("PATH");
	char * new_path;
	asprintf(&new_path, DAEMON_BIN_DIR ":%s", path);
	setenv("PATH", new_path, true);
	free(new_path);

	// ignore SIGPIPE
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &set, NULL);

	return true;
}

