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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_SOCKET_TCP_P_H__
#define __LIBSTORIQONE_SOCKET_TCP_P_H__

int so_socket_tcp(struct so_value * config);
int so_socket_tcp_accept_and_close(int fd, struct so_value * config);
int so_socket_tcp6_accept_and_close(int fd, struct so_value * config);
int so_socket_tcp_close(int fd, struct so_value * config);
int so_socket_tcp6(struct so_value * config);
int so_socket_tcp6_close(int fd, struct so_value * config);
bool so_socket_tcp_server(struct so_value * config, so_socket_accept_f accept_callback);
bool so_socket_tcp6_server(struct so_value * config, so_socket_accept_f accept_callback);
bool so_socket_tcp_from_template(struct so_value * socket_template, so_socket_accept_f accept_callback);
bool so_socket_tcp6_from_template(struct so_value * socket_template, so_socket_accept_f accept_callback);
int so_socket_server_temp_tcp(struct so_value * config);
int so_socket_server_temp_tcp6(struct so_value * config);

#endif
