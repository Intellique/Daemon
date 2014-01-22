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
*  Last modified: Mon, 18 Nov 2013 15:46:52 +0100                            *
\****************************************************************************/

// fcntl
#include <fcntl.h>
// bool
#include <stdbool.h>
// calloc, free, getenv
#include <stdlib.h>
// strdup
#include <string.h>
// waitpid
#include <sys/types.h>
// waitpid
#include <sys/wait.h>
// strcpy, strdup
#include <string.h>
// access, close, dup2, execv, fcntl, fork, pipe
#include <unistd.h>

#include <libstone/util/command.h>
#include <libstone/util/hashtable.h>
#include <libstone/util/string.h>
#include <libstone/util/util.h>

static void st_util_command_set_close_exe_flag(int fd, bool on);


void st_util_command_drop_environment(struct st_util_command * command, const char * key) {
	if (command == NULL || key == NULL)
		return;

	if (command->environment == NULL)
		command->environment = st_hashtable_new2(st_util_string_compute_hash, st_util_basic_free);

	st_hashtable_put(command->environment, strdup(key), st_hashtable_val_null());
}

void st_util_command_free(struct st_util_command * command, unsigned int nb_command) {
	if (command == NULL || nb_command < 1)
		return;

	st_util_command_wait(command, nb_command);

	unsigned int i;
	for (i = 0; i < nb_command; i++) {
		free(command->command);

		unsigned int j;
		for (j = 0; j < command[i].nb_parameters; j++)
			free(command[i].params[j]);
		free(command[i].params);
	}
}

void st_util_command_new(struct st_util_command * command, const char * command_name, const char ** params, unsigned int nb_params) {
	if (command == NULL || command_name == NULL || (params == NULL && nb_params > 0))
		return;

	bool found = false;
	if (access(command_name, R_OK | X_OK)) {
		char * path = getenv("PATH");
		if (path != NULL) {
			char * tok = path = strdup(path);

			char * next;
			for (next = NULL; !found && tok != NULL; tok = next) {
				next = strchr(tok, ':');
				if (next != NULL) {
					*next = '\0';
					next++;
				}

				char path_com[256];
				strcpy(path_com, tok);
				if (path_com[strlen(path_com) - 1] != '/')
					strcat(path_com, "/");
				strcat(path_com, command_name);

				if (!access(path_com, R_OK | X_OK)) {
					command->command = strdup(path_com);
					found = true;
				}
			}

			free(path);
		}
	}
	if (!found)
		command->command = strdup(command_name);

	command->params = calloc(nb_params + 2, sizeof(char *));
	command->nb_parameters = nb_params;

	command->environment = NULL;

	command->params[0] = strdup(command_name);
	unsigned int i;
	for (i = 0; i < nb_params; i++)
		command->params[i + 1] = strdup(params[i]);
	command->params[i + 1] = 0;

	command->pid = -1;
	for (i = 0; i < 3; i++) {
		command->fds[i].fd = i;
		command->fds[i].type = st_util_command_fd_type_default;
	}
	command->exited_code = 0;
}

void st_util_command_pipe(struct st_util_command * command_out, enum st_util_command_std out, struct st_util_command * command_in) {
	if (command_out == NULL || command_in == NULL)
		return;

	int fd_pipe[2];
	if (pipe(fd_pipe))
		return;

	st_util_command_set_fd(command_in, st_util_command_stdin, fd_pipe[0]);
	st_util_command_set_fd(command_out, out, fd_pipe[1]);
}

int st_util_command_pipe_from(struct st_util_command * command_out, enum st_util_command_std out) {
	if (command_out == NULL || out == st_util_command_stdin)
		return -1;

	int fd_pipe[2];
	if (pipe(fd_pipe))
		return -2;

	st_util_command_set_fd(command_out, out, fd_pipe[1]);
	st_util_command_set_close_exe_flag(fd_pipe[0], true);
	return fd_pipe[0];
}

int st_util_command_pipe_to(struct st_util_command * command_in) {
	if (command_in == NULL)
		return -1;

	int fd_pipe[2];
	if (pipe(fd_pipe))
		return -2;

	st_util_command_set_fd(command_in, st_util_command_stdin, fd_pipe[0]);
	st_util_command_set_close_exe_flag(fd_pipe[1], true);
	return fd_pipe[1];
}

void st_util_command_put_environment(struct st_util_command * command, const char * key, const char * value) {
	if (command == NULL || key == NULL || value == NULL)
		return;

	if (command->environment == NULL)
		command->environment = st_hashtable_new2(st_util_string_compute_hash, st_util_basic_free);

	st_hashtable_put(command->environment, strdup(key), st_hashtable_val_string(strdup(value)));
}

void st_util_command_redir_err_to_out(struct st_util_command * command) {
	if (command == NULL)
		return;

	command->fds[2].fd = 1;
	command->fds[2].type = st_util_command_fd_type_dup;
}

void st_util_command_redir_out_to_err(struct st_util_command * command) {
	if (command == NULL)
		return;

	command->fds[1].fd = 1;
	command->fds[1].type = st_util_command_fd_type_dup;
}

void st_util_command_set_environment(struct st_util_command * command, struct st_hashtable * environment) {
	if (command == NULL || environment == NULL)
		return;

	if (command->environment != NULL)
		st_hashtable_free(command->environment);

	command->environment = environment;
}

void st_util_command_set_fd(struct st_util_command * command, enum st_util_command_std fd_command, int new_fd) {
	if (command == NULL || fd_command >= 3)
		return;

	if (command->fds[fd_command].type == st_util_command_fd_type_set)
		close(command->fds[fd_command].fd);

	command->fds[fd_command].fd = new_fd;
	command->fds[fd_command].type = st_util_command_fd_type_set;

	st_util_command_set_close_exe_flag(new_fd, true);
}

static void st_util_command_set_close_exe_flag(int fd, bool on) {
	int flag = fcntl(fd, F_GETFD);

	if (on)
		flag |= FD_CLOEXEC;
	else
		flag &= !FD_CLOEXEC;

	fcntl(fd, F_SETFD, flag);
}

void st_util_command_start(struct st_util_command * command, unsigned int nb_command) {
	if (!command || nb_command < 1)
		return;

	unsigned int i;
	for (i = 0; i < nb_command; i++) {
		if (command[i].pid >= 0)
			continue;

		command[i].pid = fork();
		if (command[i].pid > 0) {
			unsigned int j;
			for (j = 0; j < 3; j++)
				if (command[i].fds[j].type == st_util_command_fd_type_set)
					close(command[i].fds[j].fd);
		} else if (command[i].pid == 0) {
			unsigned int j;
			for (j = 0; j < 3; j++) {
				if (command[i].fds[j].type == st_util_command_fd_type_default)
					continue;

				close(j);

				if (command[i].fds[j].type == st_util_command_fd_type_set) {
					dup2(command[i].fds[j].fd, j);
					close(command[i].fds[j].fd);
				}
			}
			for (j = 0; j < 3; j++) {
				if (command[i].fds[j].type == st_util_command_fd_type_dup)
					dup2(j, command[i].fds[j].fd);

				if (command[i].fds[j].type != st_util_command_fd_type_close)
					st_util_command_set_close_exe_flag(j, false);
			}

			if (command->environment != NULL) {
				struct st_hashtable_iterator * iter = st_hashtable_iterator_new(command->environment);
				while (st_hashtable_iterator_has_next(iter)) {
					const char * key = st_hashtable_iterator_next(iter);
					struct st_hashtable_value val = st_hashtable_get(command->environment, key);

					if (val.type == st_hashtable_value_null)
						unsetenv(key);
					else if (val.type == st_hashtable_value_string)
						setenv(key, val.value.string, true);
				}
				st_hashtable_iterator_free(iter);
			}

			execv(command[i].command, command[i].params);
		}
	}
}

void st_util_command_wait(struct st_util_command * command, unsigned int nb_command) {
	if (!command || nb_command < 1)
		return;

	unsigned int i;
	for (i = 0; i < nb_command; i++) {
		if (command[i].pid < 0)
			continue;

		int status = 0;
		int err = waitpid(command[i].pid, &status, 0);
		if (err > 0)
			command[i].exited_code = WEXITSTATUS(status);
	}
}

