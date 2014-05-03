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

#ifndef __LIBSTONE_PROCESS_P_H__
#define __LIBSTONE_PROCESS_P_H__

#include <libstone/process.h>
#include <libstone/value.h>

void st_process_close_v1(struct st_process * process, enum st_process_std std);
void st_process_drop_environment_v1(struct st_process * process, const char * key);
void st_process_free_v1(struct st_process * process, unsigned int nb_process);
void st_process_new_v1(struct st_process * process, const char * process_name, const char ** params, unsigned int nb_params);
void st_process_pipe_v1(struct st_process * process_out, enum st_process_std out, struct st_process * process_in);
int st_process_pipe_from_v1(struct st_process * process_out, enum st_process_std out);
int st_process_pipe_to_v1(struct st_process * process_in);
void st_process_put_environment_v1(struct st_process * process, const char * key, const char * value);
void st_process_redir_err_to_out_v1(struct st_process * process);
void st_process_redir_out_to_err_v1(struct st_process * process);
void st_process_set_environment_v1(struct st_process * process, struct st_value * environment);
void st_process_set_fd_v1(struct st_process * process, enum st_process_std fd_process, int new_fd);
void st_process_set_nice_v1(struct st_process * process, int nice);
void st_process_start_v1(struct st_process * process, unsigned int nb_process);
void st_process_wait_v1(struct st_process * process, unsigned int nb_process);

#endif

