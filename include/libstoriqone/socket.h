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

#ifndef __LIBSTORIQONE_SOCKET_H__
#define __LIBSTORIQONE_SOCKET_H__

// bool
#include <stdbool.h>

struct so_value;
typedef void (*so_socket_accept_f)(int fd_server, int fd_client, struct so_value * client);

int so_socket(struct so_value * config) __attribute__((nonnull,warn_unused_result));
int so_socket_accept_and_close(int fd, struct so_value * config) __attribute__((nonnull,warn_unused_result));
int so_socket_close(int fd, struct so_value * config) __attribute__((nonnull));
bool so_socket_server(struct so_value * config, so_socket_accept_f accept_callback) __attribute__((nonnull));
int so_socket_server_temp(struct so_value * config) __attribute__((nonnull));

#endif

