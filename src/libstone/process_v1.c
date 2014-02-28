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
// fcntl
#include <fcntl.h>
// bool
#include <stdbool.h>
// asprintf
#include <stdio.h>
// free, getenv, setenv, unsetenv
#include <stdlib.h>
// strchr, strcpy, strdup
#include <string.h>
// waitpid
#include <sys/types.h>
// waitpid
#include <sys/wait.h>
// access, close, dup2, execv, fcntl, fork, pipe
#include <unistd.h>

#include "process_v1.h"
#include "string_v1.h"
#include "value_v1.h"

static void st_process_set_close_exe_flag(int fd, bool on);

__asm__(".symver st_process_drop_environment_v1, st_process_drop_environment@@LIBSTONE_1.0");
void st_process_drop_environment_v1(struct st_process * process, const char * key) {
	if (process == NULL || key == NULL)
		return;

	if (process->environment == NULL)
		process->environment = st_value_new_hashtable_v1(st_string_compute_hash_v1);

	st_value_hashtable_put_v1(process->environment, st_value_new_string_v1(key), true, st_value_new_null_v1(), true);
}

__asm__(".symver st_process_free_v1, st_process_free@@LIBSTONE_1.0");
void st_process_free_v1(struct st_process * process, unsigned int nb_process) {
	if (process == NULL || nb_process < 1)
		return;

	st_process_wait_v1(process, nb_process);

	unsigned int i;
	for (i = 0; i < nb_process; i++) {
		free(process->command);

		unsigned int j;
		for (j = 0; j <= process[i].nb_parameters; j++)
			free(process[i].params[j]);
		free(process[i].params);
	}
}

__asm__(".symver st_process_new_v1, st_process_new@@LIBSTONE_1.0");
void st_process_new_v1(struct st_process * process, const char * process_name, const char ** params, unsigned int nb_params) {
	if (process == NULL || process_name == NULL || (params == NULL && nb_params > 0))
		return;

	bool found = false;
	if (access(process_name, R_OK | X_OK)) {
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

				char * path_com;
				asprintf(&path_com, "%s/%s", tok, process_name);

				if (!access(path_com, R_OK | X_OK)) {
					process->command = strdup(path_com);
					found = true;
				}

				free(path_com);
			}

			free(path);
		}
	}
	if (!found)
		process->command = strdup(process_name);

	process->params = calloc(nb_params + 2, sizeof(char *));
	process->nb_parameters = nb_params;

	process->environment = NULL;

	process->params[0] = strdup(process_name);
	unsigned int i;
	for (i = 0; i < nb_params; i++)
		process->params[i + 1] = strdup(params[i]);
	process->params[i + 1] = NULL;

	process->pid = -1;
	for (i = 0; i < 3; i++) {
		process->fds[i].fd = i;
		process->fds[i].type = st_process_fd_type_default;
	}
	process->exited_code = 0;
}

__asm__(".symver st_process_pipe_v1, st_process_pipe@@LIBSTONE_1.0");
void st_process_pipe_v1(struct st_process * process_out, enum st_process_std out, struct st_process * process_in) {
	if (process_out == NULL || process_in == NULL)
		return;

	int fd_pipe[2];
	if (pipe(fd_pipe))
		return;

	st_process_set_fd_v1(process_in, st_process_stdin, fd_pipe[0]);
	st_process_set_fd_v1(process_out, out, fd_pipe[1]);
}

__asm__(".symver st_process_pipe_from_v1, st_process_pipe_from@@LIBSTONE_1.0");
int st_process_pipe_from_v1(struct st_process * process_out, enum st_process_std out) {
	if (process_out == NULL || out == st_process_stdin)
		return -1;

	int fd_pipe[2];
	if (pipe(fd_pipe))
		return -2;

	st_process_set_fd_v1(process_out, out, fd_pipe[1]);
	st_process_set_close_exe_flag(fd_pipe[0], true);
	return fd_pipe[0];
}

__asm__(".symver st_process_pipe_to_v1, st_process_pipe_to@@LIBSTONE_1.0");
int st_process_pipe_to_v1(struct st_process * process_in) {
	if (process_in == NULL)
		return -1;

	int fd_pipe[2];
	if (pipe(fd_pipe))
		return -2;

	st_process_set_fd_v1(process_in, st_process_stdin, fd_pipe[0]);
	st_process_set_close_exe_flag(fd_pipe[1], true);
	return fd_pipe[1];
}

__asm__(".symver st_process_put_environment_v1, st_process_put_environment@@LIBSTONE_1.0");
void st_process_put_environment_v1(struct st_process * process, const char * key, const char * value) {
	if (process == NULL || key == NULL || value == NULL)
		return;

	if (process->environment == NULL)
		process->environment = st_value_new_hashtable_v1(st_string_compute_hash_v1);

	st_value_hashtable_put_v1(process->environment, st_value_new_string_v1(key), true, st_value_new_string_v1(value), true);
}

__asm__(".symver st_process_redir_err_to_out_v1, st_process_redir_err_to_out@@LIBSTONE_1.0");
void st_process_redir_err_to_out_v1(struct st_process * process) {
	if (process == NULL)
		return;

	process->fds[2].fd = 1;
	process->fds[2].type = st_process_fd_type_dup;
}

__asm__(".symver st_process_redir_out_to_err_v1, st_process_redir_out_to_err@@LIBSTONE_1.0");
void st_process_redir_out_to_err_v1(struct st_process * process) {
	if (process == NULL)
		return;

	process->fds[1].fd = 1;
	process->fds[1].type = st_process_fd_type_dup;
}

__asm__(".symver st_process_set_environment_v1, st_process_set_environment@@LIBSTONE_1.0");
void st_process_set_environment_v1(struct st_process * process, struct st_value * environment) {
	if (process == NULL || environment == NULL)
		return;

	if (process->environment != NULL)
		st_value_free_v1(process->environment);

	process->environment = environment;
}

__asm__(".symver st_process_set_fd_v1, st_process_set_fd@@LIBSTONE_1.0");
void st_process_set_fd_v1(struct st_process * process, enum st_process_std fd_process, int new_fd) {
	if (process == NULL || fd_process >= 3)
		return;

	if (process->fds[fd_process].type == st_process_fd_type_set)
		close(process->fds[fd_process].fd);

	process->fds[fd_process].fd = new_fd;
	process->fds[fd_process].type = st_process_fd_type_set;

	st_process_set_close_exe_flag(new_fd, true);
}

static void st_process_set_close_exe_flag(int fd, bool on) {
	int flag = fcntl(fd, F_GETFD);

	if (on)
		flag |= FD_CLOEXEC;
	else
		flag &= !FD_CLOEXEC;

	fcntl(fd, F_SETFD, flag);
}

__asm__(".symver st_process_start_v1, st_process_start@@LIBSTONE_1.0");
void st_process_start_v1(struct st_process * process, unsigned int nb_process) {
	if (!process || nb_process < 1)
		return;

	unsigned int i;
	for (i = 0; i < nb_process; i++) {
		if (process[i].pid >= 0)
			continue;

		process[i].pid = fork();
		if (process[i].pid > 0) {
			unsigned int j;
			for (j = 0; j < 3; j++)
				if (process[i].fds[j].type == st_process_fd_type_set)
					close(process[i].fds[j].fd);
		} else if (process[i].pid == 0) {
			unsigned int j;
			for (j = 0; j < 3; j++) {
				if (process[i].fds[j].type == st_process_fd_type_default)
					continue;

				close(j);

				if (process[i].fds[j].type == st_process_fd_type_set) {
					dup2(process[i].fds[j].fd, j);
					close(process[i].fds[j].fd);
				}
			}
			for (j = 0; j < 3; j++) {
				if (process[i].fds[j].type == st_process_fd_type_dup)
					dup2(j, process[i].fds[j].fd);

				if (process[i].fds[j].type != st_process_fd_type_close)
					st_process_set_close_exe_flag(j, false);
			}

			if (process->environment != NULL) {
				struct st_value_iterator * iter = st_value_hashtable_get_iterator_v1(process->environment);
				while (st_value_iterator_has_next_v1(iter)) {
					struct st_value * key = st_value_iterator_get_key_v1(iter, false, false);
					struct st_value * value = st_value_iterator_get_value_v1(iter, false);

					if (value->type == st_value_null)
						unsetenv(key->value.string);
					else
						setenv(key->value.string, value->value.string, true);
				}
				st_value_iterator_free_v1(iter);
			}

			execv(process[i].command, process[i].params);
		}
	}
}

__asm__(".symver st_process_start_v1, st_process_start@@LIBSTONE_1.0");
void st_process_wait_v1(struct st_process * process, unsigned int nb_process) {
	if (!process || nb_process < 1)
		return;

	unsigned int i;
	for (i = 0; i < nb_process; i++) {
		if (process[i].pid < 0)
			continue;

		int status = 0;
		int err = waitpid(process[i].pid, &status, 0);
		if (err > 0)
			process[i].exited_code = WEXITSTATUS(status);
	}
}

