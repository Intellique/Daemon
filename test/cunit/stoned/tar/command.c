/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Tue, 02 Oct 2012 15:21:44 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// errno
#include <errno.h>
// pipe2
#include <fcntl.h>
// poll
#include <poll.h>
// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// fstat, pipe2
#include <sys/stat.h>
// fstat, pipe2
#include <sys/types.h>
// fstatfs
#include <sys/vfs.h>
// waitpid
#include <sys/wait.h>
// close, fstat, pipe2
#include <unistd.h>

#include "command.h"

int test_stoned_tar_command_new(char * const params[], const char * directory, const char * input, const char * output, const char * error) {
	int pid = fork();
	if (pid < 0)
		return -1;

	if (pid > 0)
		return pid;

	if (input) {
		int fd = open(input, O_RDONLY);
		dup2(fd, 0);
		close(fd);
	} else
		close(0);

	if (output) {
		int fd = open(output, O_WRONLY);
		dup2(fd, 1);
		close(fd);
	} //else
		//close(1);

	if (error) {
		int fd = open(error, O_WRONLY);
		dup2(fd, 2);
		close(fd);
	} //else
		//close(2);

	if (directory)
		chdir(directory);

	execvp(params[0], params);

	_exit(1);
}

int test_stoned_tar_command_wait(int pid) {
	int status = 0;
	waitpid(pid, &status, 0);

	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	else
		return -1;
}

