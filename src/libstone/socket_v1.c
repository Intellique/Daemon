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

#include "socket_v1.h"
#include "socket/unix_v1.h"
#include "value_v1.h"

__asm__(".symver st_socket_server_v1, st_socket_server@@LIBSTONE_1.0");
int st_socket_v1(struct st_value * config) {
	return st_socket_unix_v1(config);
}

__asm__(".symver st_socket_server_v1, st_socket_server@@LIBSTONE_1.0");
bool st_socket_server_v1(struct st_value * config, st_socket_accept_f accept_callback) {
	return st_socket_unix_server_v1(config, accept_callback);
}

