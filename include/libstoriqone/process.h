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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

#ifndef __LIBSTONE_PROCESS_H__
#define __LIBSTONE_PROCESS_H__

typedef int (*so_process_sub_callback)(void * arg);

struct so_value;

enum so_process_fd_type {
	so_process_fd_type_default,
	so_process_fd_type_close,
	so_process_fd_type_set,
	so_process_fd_type_dup,
	so_process_fd_type_set_null,
};

struct so_process_fd {
	int fd;
	enum so_process_fd_type type;
};

enum so_process_std {
    so_process_stdin  = 0,
    so_process_stdout = 1,
    so_process_stderr = 2,
};

struct so_process {
	/**
	 * \brief Command name
	 */
	char * command;

	/**
	 * \brief parameters
	 */
	char ** params;
	unsigned int nb_parameters;

	struct so_value * environment;
	int nice;

	/**
	 * \brief Pid of process or -1
	 */
	int pid;
	struct so_process_fd fds[3];

	/**
	 * \brief code that process has returned
	 */
	int exited_code;
};

void so_process_close(struct so_process * process, enum so_process_std std);
void so_process_drop_environment(struct so_process * process, const char * key);
void so_process_free(struct so_process * process, unsigned int nb_process);
void so_process_new(struct so_process * process, const char * process_name, const char ** params, unsigned int nb_params);
void so_process_pipe(struct so_process * process_out, enum so_process_std out, struct so_process * process_in);
int so_process_pipe_from(struct so_process * process_out, enum so_process_std out);
int so_process_pipe_to(struct so_process * process_in);
void so_process_put_environment(struct so_process * process, const char * key, const char * value);
void so_process_redir_err_to_out(struct so_process * process);
void so_process_redir_out_to_err(struct so_process * process);
void so_process_set_environment(struct so_process * process, struct so_value * environment);
void so_process_set_fd(struct so_process * process, enum so_process_std fd_process, int new_fd);
void so_process_set_nice(struct so_process * process, int nice);
void so_process_set_null(struct so_process * process, enum so_process_std fd);
void so_process_start(struct so_process * process, unsigned int nb_process);
void so_process_wait(struct so_process * process, unsigned int nb_process);

int so_process_fork_and_do(so_process_sub_callback function, void * arg);

#endif

