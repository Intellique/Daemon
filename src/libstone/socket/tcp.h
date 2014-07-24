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

#ifndef __LIBSTONE_SOCKET_TCP_P_H__
#define __LIBSTONE_SOCKET_TCP_P_H__

#include "../socket.h"

int st_socket_tcp_v1(struct st_value * config);
int st_socket_tcp_accept_and_close_v1(int fd, struct st_value * config);
int st_socket_tcp6_accept_and_close_v1(int fd, struct st_value * config);
int st_socket_tcp_close_v1(int fd, struct st_value * config);
int st_socket_tcp6_v1(struct st_value * config);
int st_socket_tcp6_close_v1(int fd, struct st_value * config);
bool st_socket_tcp_server_v1(struct st_value * config, st_socket_accept_f accept_callback);
bool st_socket_tcp6_server_v1(struct st_value * config, st_socket_accept_f accept_callback);
int st_socket_server_temp_tcp_v1(struct st_value * config);
int st_socket_server_temp_tcp6_v1(struct st_value * config);

#endif

