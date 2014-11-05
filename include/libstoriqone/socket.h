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

#ifndef __LIBSTONE_SOCKET_H__
#define __LIBSTONE_SOCKET_H__

// bool
#include <stdbool.h>

struct st_value;
typedef void (*st_socket_accept_f)(int fd_server, int fd_client, struct st_value * client);

int st_socket(struct st_value * config) __attribute__((nonnull,warn_unused_result));
int st_socket_accept_and_close(int fd, struct st_value * config) __attribute__((nonnull,warn_unused_result));
int st_socket_close(int fd, struct st_value * config) __attribute__((nonnull));
bool st_socket_server(struct st_value * config, st_socket_accept_f accept_callback) __attribute__((nonnull));
int st_socket_server_temp(struct st_value * config) __attribute__((nonnull));

#endif

