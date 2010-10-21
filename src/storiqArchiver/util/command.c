/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2010, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Thu, 21 Oct 2010 09:07:01 +0200                       *
\***********************************************************************/

// fcntl
#include <fcntl.h>
// calloc, free, getenv
#include <stdlib.h>
// strcpy, strdup
#include <string.h>
// waitpid
#include <sys/types.h>
// waitpid
#include <sys/wait.h>
// access, close, dup2, execv, fcntl, fork, pipe
#include <unistd.h>

#include <storiqArchiver/util/command.h>


void command_setCloseExeFlag(int fd, int on);


void command_free(struct command * command, unsigned int nbCommand) {
	if (!command || !nbCommand)
		return;

	command_wait(command, nbCommand);

	if (command->command)
		free(command->command);
	command->command = 0;

	unsigned int i;
	for (i = 0; i < nbCommand; i++) {
		unsigned int j;
		for (j = 0; j < command[i].nbParameters; j++)
			if (command[i].params[j])
				free(command[i].params[j]);
		free(command[i].params);
	}
}

void command_init(struct command * command, unsigned int nbCommand) {
	if (!command || !nbCommand)
		return;

	unsigned int i;
	for (i = 0; i < nbCommand; i++) {
		command[i].command = 0;
		command[i].params = 0;
		command[i].nbParameters = 0;

		command[i].pid = -1;
		unsigned int j;
		for (j = 0; j < 3; j++) {
			command[i].fds[j].fd = j;
			command[i].fds[j].type = command_fd_type_default;
		}

		command[i].exitedCode = -1;
	}
}

void command_new(struct command * com, const char * command, const char ** params, unsigned int nbParams) {
	if (!com || !command)
		return;

	short found = 0;
	if (access(command, R_OK | X_OK)) {
		char * path = getenv("PATH");
		if (path) {
			char * tok = path = strdup(path);

			char * next;
			for (next = 0; !found && tok; tok = next) {
				next = strchr(tok, ':');
				if (next) {
					*next = '\0';
					next++;
				}

				char path_com[256];
				strcpy(path_com, tok);
				if (path_com[strlen(path_com) - 1] != '/')
					strcat(path_com, "/");
				strcat(path_com, command);

				if (!access(path_com, R_OK | X_OK)) {
					com->command = strdup(path_com);
					found = 1;
				}
			}

			free(path);
		}
	}
	if (!found)
		com->command = strdup(command);

	com->params = calloc(nbParams + 2, sizeof(char *));
	com->nbParameters = nbParams;

	com->params[0] = strdup(command);
	unsigned int i;
	for (i = 0; i < nbParams; i++)
		com->params[i + 1] = strdup(params[i]);
	com->params[i + 1] = 0;

	com->pid = -1;
	for (i = 0; i < 3; i++) {
		com->fds[i].fd = i;
		com->fds[i].type = command_fd_type_default;
	}
	com->exitedCode = 0;
}

void command_pipe(struct command * command_out, enum command_std out, struct command * command_in) {
	if (!command_out || !command_in || out == command_stdin)
		return;

	int fd_pipe[2];
	if (pipe(fd_pipe))
		return;

	command_setFd(command_in, command_stdin, fd_pipe[0]);
	command_setFd(command_out, out, fd_pipe[1]);
}

int command_pipe_from(struct command * command, enum command_std out) {
	if (!command || out == command_stdin)
		return -1;

	int fd_pipe[2];
	if (pipe(fd_pipe))
		return -2;

	command_setFd(command, out, fd_pipe[1]);
	return fd_pipe[0];
}

int command_pipe_to(struct command * command) {
	if (!command)
		return -1;

	int fd_pipe[2];
	if (pipe(fd_pipe))
		return -2;

	command_setFd(command, command_stdin, fd_pipe[0]);
	return fd_pipe[1];
}

void command_redir_err_to_out(struct command * command) {
	if (!command)
		return;

	command->fds[2].fd = 1;
	command->fds[2].type = command_fd_type_dup;
}

void command_redir_out_to_err(struct command * command) {
	if (!command)
		return;

	command->fds[1].fd = 2;
	command->fds[1].type = command_fd_type_dup;
}

void command_setFd(struct command * command, enum command_std fd_command, int new_fd) {
	if (!command || fd_command >= 3)
		return;

	if (command->fds[fd_command].type == command_fd_type_set)
		close(command->fds[fd_command].fd);

	command->fds[fd_command].fd = new_fd;
	command->fds[fd_command].type = command_fd_type_set;

	command_setCloseExeFlag(new_fd, 1);
}

void command_setCloseExeFlag(int fd, int on) {
	int flag = fcntl(fd, F_GETFD);

	if (on)
		flag |= FD_CLOEXEC;
	else
		flag &= !FD_CLOEXEC;

	fcntl(fd, F_SETFD, flag);
}

void command_start(struct command * command, unsigned int nbCommand) {
	if (!command || nbCommand < 1)
		return;

	unsigned int i;
	for (i = 0; i < nbCommand; i++) {
		if (command[i].pid >= 0)
			continue;

		command[i].pid = fork();
		if (command[i].pid > 0) {
			unsigned int j;
			for (j = 0; j < 3; j++)
				if (command[i].fds[j].type == command_fd_type_set)
					close(command[i].fds[j].fd);
		} else if (command[i].pid == 0) {
			unsigned int j;
			for (j = 0; j < 3; j++) {
				if (command[i].fds[j].type == command_fd_type_default)
					continue;
				close(j);
				if (command[i].fds[j].type == command_fd_type_set) {
					dup2(command[i].fds[j].fd, j);
					close(command[i].fds[j].fd);
				}
			}
			for (j = 0; j < 3; j++) {
				if (command[i].fds[j].type == command_fd_type_dup)
					dup2(j, command[i].fds[j].fd);
				if (command[i].fds[j].type != command_fd_type_close)
					command_setCloseExeFlag(j, 0);
			}

			execv(command[i].command, command[i].params);
		}
	}
}

void command_wait(struct command * command, unsigned int nbCommand) {
	if (!command || nbCommand < 1)
		return;

	unsigned int i;
	for (i = 0; i < nbCommand; i++) {
		if (command[i].pid < 0)
			continue;

		int status = 0;

		int err = waitpid(command[i].pid, &status, 0);
		if (err > 0)
			command[i].exitedCode = WEXITSTATUS(status);
	}
}

