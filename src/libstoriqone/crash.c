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

// snprintf
#include <stdio.h>
// signal
#include <signal.h>
// getpid, waitpid
#include <sys/types.h>
// waitpid
#include <sys/wait.h>
// close, execlp, fork, getpid
#include <unistd.h>

#include <libstoriqone/crash.h>

#include "config.h"

static pid_t so_crash_pid = -1;
static char so_crash_prog_name[256];

static void so_crash(int signal);


static void so_crash(int signal __attribute__((unused))) {
	pid_t child = fork();

	if (child == 0) {
		char str_pid[8];
		snprintf(str_pid, 8, "%d", so_crash_pid);

		close(1);
		close(2);

		execlp("gcore", "gcore", "-o", so_crash_prog_name, str_pid, NULL);

		_exit(0);
	}

	int status = 0;
	waitpid(child, &status, 0);

	_exit(0);
}

void so_crash_init(const char * prog_name) {
	so_crash_pid = getpid();
	snprintf(so_crash_prog_name, 256, DAEMON_CRASH_DIR "/%s", prog_name);

	signal(SIGSEGV, so_crash);
	signal(SIGABRT, so_crash);
}

