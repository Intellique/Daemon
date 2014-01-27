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
*  Last modified: Mon, 25 Mar 2013 21:26:40 +0100                            *
\****************************************************************************/

#ifndef __STONE_UTIL_COMMAND_H__
#define __STONE_UTIL_COMMAND_H__

struct st_hashtable;

enum st_util_command_fd_type {
	st_util_command_fd_type_default,
	st_util_command_fd_type_close,
	st_util_command_fd_type_set,
	st_util_command_fd_type_dup,
};

struct st_util_command_fd {
	int fd;
	enum st_util_command_fd_type type;
};

enum st_util_command_std {
    st_util_command_stdin  = 0,
    st_util_command_stdout = 1,
    st_util_command_stderr = 2,
};

struct st_util_command {
	/**
	 * \brief Command name
	 */
	char * command;

	/**
	 * \brief parameters
	 */
	char ** params;
	unsigned int nb_parameters;

	struct st_hashtable * environment;

	/**
	 * \brief Pid of process or -1
	 */
	int pid;
	struct st_util_command_fd fds[3];

	/**
	 * \brief code that process has returned
	 */
	int exited_code;
};

void st_util_command_drop_environment(struct st_util_command * command, const char * key);
void st_util_command_free(struct st_util_command * command, unsigned int nb_command);
void st_util_command_new(struct st_util_command * command, const char * command_name, const char ** params, unsigned int nb_params);
void st_util_command_pipe(struct st_util_command * command_out, enum st_util_command_std out, struct st_util_command * command_in);
int st_util_command_pipe_from(struct st_util_command * command_out, enum st_util_command_std out);
int st_util_command_pipe_to(struct st_util_command * command_in);
void st_util_command_put_environment(struct st_util_command * command, const char * key, const char * value);
void st_util_command_redir_err_to_out(struct st_util_command * command);
void st_util_command_redir_out_to_err(struct st_util_command * command);
void st_util_command_set_environment(struct st_util_command * command, struct st_hashtable * environment);
void st_util_command_set_fd(struct st_util_command * command, enum st_util_command_std fd_command, int new_fd);
void st_util_command_start(struct st_util_command * command, unsigned int nb_command);
void st_util_command_wait(struct st_util_command * command, unsigned int nb_command);

#endif

