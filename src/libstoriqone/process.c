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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

#define _GNU_SOURCE
// fcntl
#include <fcntl.h>
// bool
#include <stdbool.h>
// asprintf
#include <stdio.h>
// calloc, free, getenv, setenv, unsetenv
#include <stdlib.h>
// strchr, strdup, strrchr
#include <string.h>
// bzero
#include <strings.h>
// stat
#include <sys/stat.h>
// stat, waitpid
#include <sys/types.h>
// waitpid
#include <sys/wait.h>
// access, close, dup2, execv, fcntl, fork, nice, pipe, stat
#include <unistd.h>

#include <libstoriqone/process.h>
#include <libstoriqone/value.h>

static void so_process_set_close_exe_flag(int fd, bool on);

void so_process_close(struct so_process * process, enum so_process_std std) {
	if (process == NULL)
		return;

	process->fds[std].type = so_process_fd_type_close;
}

void so_process_drop_environment(struct so_process * process, const char * key) {
	if (process == NULL || key == NULL)
		return;

	if (process->environment == NULL)
		process->environment = so_value_new_hashtable2();

	so_value_hashtable_put(process->environment, so_value_new_string(key), true, so_value_new_null(), true);
}

void so_process_free(struct so_process * process, unsigned int nb_process) {
	if (process == NULL || nb_process < 1)
		return;

	unsigned int i;
	for (i = 0; i < nb_process; i++) {
		free(process->command);

		unsigned int j;
		for (j = 0; j <= process[i].nb_parameters; j++)
			free(process[i].params[j]);
		free(process[i].params);
	}
}

void so_process_new(struct so_process * process, const char * process_name, const char ** params, unsigned int nb_params) {
	if (process == NULL || process_name == NULL || (params == NULL && nb_params > 0))
		return;

	bzero(process, sizeof(struct so_process));

	struct stat file;
	if (access(process_name, R_OK | X_OK) == 0 && stat(process_name, &file) == 0 && S_ISREG(file.st_mode))
		process->command = strdup(process_name);

	if (process->command == NULL) {
		char * path = getenv("PATH");
		if (path != NULL) {
			char * tok = path = strdup(path);

			char * next;
			for (next = NULL; process->command == NULL && tok != NULL; tok = next) {
				next = strchr(tok, ':');
				if (next != NULL) {
					*next = '\0';
					next++;
				}

				char * path_com;
				asprintf(&path_com, "%s/%s", tok, process_name);

				if (access(path_com, R_OK | X_OK) == 0 && stat(path_com, &file) == 0 && S_ISREG(file.st_mode))
					process->command = strdup(path_com);

				free(path_com);
			}

			free(path);
		}
	}

	if (process->command == NULL)
		process->command = strdup(process_name);

	process->params = calloc(nb_params + 2, sizeof(char *));
	process->nb_parameters = nb_params;

	process->environment = NULL;
	process->nice = 0;

	char * ptr_cmd = strrchr(process->command, '/');
	if (ptr_cmd == NULL)
		ptr_cmd = process->command;
	else
		ptr_cmd++;

	process->params[0] = strdup(ptr_cmd);
	unsigned int i;
	for (i = 0; i < nb_params; i++)
		process->params[i + 1] = strdup(params[i]);
	process->params[i + 1] = NULL;

	process->pid = -1;
	int j;
	for (j = 0; j < 3; j++) {
		process->fds[j].fd = j;
		process->fds[j].type = so_process_fd_type_default;
	}
	process->exited_code = 0;
}

void so_process_pipe(struct so_process * process_out, enum so_process_std out, struct so_process * process_in) {
	if (process_out == NULL || process_in == NULL)
		return;

	int fd_pipe[2];
	if (pipe(fd_pipe))
		return;

	so_process_set_fd(process_in, so_process_stdin, fd_pipe[0]);
	so_process_set_fd(process_out, out, fd_pipe[1]);
}

int so_process_pipe_from(struct so_process * process_out, enum so_process_std out) {
	if (process_out == NULL || out == so_process_stdin)
		return -1;

	int fd_pipe[2];
	if (pipe(fd_pipe))
		return -2;

	so_process_set_fd(process_out, out, fd_pipe[1]);
	so_process_set_close_exe_flag(fd_pipe[0], true);
	return fd_pipe[0];
}

int so_process_pipe_to(struct so_process * process_in) {
	if (process_in == NULL)
		return -1;

	int fd_pipe[2];
	if (pipe(fd_pipe))
		return -2;

	so_process_set_fd(process_in, so_process_stdin, fd_pipe[0]);
	so_process_set_close_exe_flag(fd_pipe[1], true);
	return fd_pipe[1];
}

void so_process_put_environment(struct so_process * process, const char * key, const char * value) {
	if (process == NULL || key == NULL || value == NULL)
		return;

	if (process->environment == NULL)
		process->environment = so_value_new_hashtable2();

	so_value_hashtable_put(process->environment, so_value_new_string(key), true, so_value_new_string(value), true);
}

void so_process_redir_err_to_out(struct so_process * process) {
	if (process == NULL)
		return;

	process->fds[2].fd = 1;
	process->fds[2].type = so_process_fd_type_dup;
}

void so_process_redir_out_to_err(struct so_process * process) {
	if (process == NULL)
		return;

	process->fds[1].fd = 1;
	process->fds[1].type = so_process_fd_type_dup;
}

void so_process_set_environment(struct so_process * process, struct so_value * environment) {
	if (process == NULL || environment == NULL)
		return;

	if (process->environment != NULL)
		so_value_free(process->environment);

	process->environment = environment;
}

void so_process_set_fd(struct so_process * process, enum so_process_std fd_process, int new_fd) {
	if (process == NULL || fd_process >= 3)
		return;

	if (process->fds[fd_process].type == so_process_fd_type_set)
		close(process->fds[fd_process].fd);

	process->fds[fd_process].fd = new_fd;
	process->fds[fd_process].type = so_process_fd_type_set;

	so_process_set_close_exe_flag(new_fd, true);
}

void so_process_set_nice(struct so_process * process, int nice) {
	if (process == NULL)
		return;

	process->nice = nice;
}

static void so_process_set_close_exe_flag(int fd, bool on) {
	int flag = fcntl(fd, F_GETFD);

	if (on)
		flag |= FD_CLOEXEC;
	else
		flag &= !FD_CLOEXEC;

	fcntl(fd, F_SETFD, flag);
}

void so_process_start(struct so_process * process, unsigned int nb_process) {
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
				if (process[i].fds[j].type == so_process_fd_type_set)
					close(process[i].fds[j].fd);
		} else if (process[i].pid == 0) {
			int j;
			for (j = 0; j < 3; j++) {
				if (process[i].fds[j].type == so_process_fd_type_default)
					continue;

				close(j);

				if (process[i].fds[j].type == so_process_fd_type_set) {
					dup2(process[i].fds[j].fd, j);
					close(process[i].fds[j].fd);
				}
			}
			for (j = 0; j < 3; j++) {
				if (process[i].fds[j].type == so_process_fd_type_dup)
					dup2(j, process[i].fds[j].fd);

				if (process[i].fds[j].type != so_process_fd_type_close)
					so_process_set_close_exe_flag(j, false);
			}

			if (process->environment != NULL) {
				struct so_value_iterator * iter = so_value_hashtable_get_iterator(process->environment);
				while (so_value_iterator_has_next(iter)) {
					struct so_value * key = so_value_iterator_get_key(iter, false, false);
					struct so_value * value = so_value_iterator_get_value(iter, false);

					if (value->type == so_value_null)
						unsetenv(so_value_string_get(key));
					else
						setenv(so_value_string_get(key), so_value_string_get(value), true);
				}
				so_value_iterator_free(iter);
			}

			if (process->nice != 0)
				nice(process->nice);

			execv(process[i].command, process[i].params);

			_exit(1);
		}
	}
}

void so_process_wait(struct so_process * process, unsigned int nb_process) {
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

