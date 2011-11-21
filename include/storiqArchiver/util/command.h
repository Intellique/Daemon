/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 21 Nov 2011 13:44:27 +0100                         *
\*************************************************************************/

#ifndef __STORIQARCHIVER_COMMAND_H__
#define __STORIQARCHIVER_COMMAND_H__

/**
 * \private
 */
enum command_fd_type {
	command_fd_type_default,
	command_fd_type_close,
	command_fd_type_set,
	command_fd_type_dup,
};

/**
 * \private
 */
struct command_fd {
	int fd;
	enum command_fd_type type;
};

/**
 * \struct command
 * \brief This structure contains one command
 */
struct command {
	/**
	 * \brief Command name
	 */
	char * command;

	/**
	 * \brief parameters
	 */
	char ** params;
	unsigned int nbParameters;

	/**
	 * \brief Pid of process or -1
	 */
	int pid;
	struct command_fd fds[3];

	/**
	 * \brief code that process has returned
	 */
	int exitedCode;
};

enum command_std {
    command_stdin  = 0,
    command_stdout = 1,
    command_stderr = 2,
};

void command_free(struct command * command, unsigned int nbCommand);
void command_init(struct command * command, unsigned int nbCommand);
void command_new(struct command * com, const char * command, const char ** params, unsigned int nbParams);
void command_pipe(struct command * command_out, enum command_std out, struct command * command_in);
int command_pipe_from(struct command * command_out, enum command_std out);
int command_pipe_to(struct command * command_in);
void command_redir_err_to_out(struct command * command);
void command_redir_out_to_err(struct command * command);
void command_setFd(struct command * command, enum command_std fd_command, int new_fd);
void command_start(struct command * command, unsigned int nbCommand);
void command_wait(struct command * command, unsigned int nbCommand);

#endif

