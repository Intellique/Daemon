/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
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
*  Last modified: Mon, 23 Jan 2012 21:58:52 +0100                         *
\*************************************************************************/

#ifndef __STONEADMIN_SOCKET_H__
#define __STONEADMIN_SOCKET_H__

#include <stone/io.h>

struct st_io_socket {
	struct st_io_socket_ops {
		int (*connect)(struct st_io_socket * socket, const char * hostname, unsigned short port);
		void (*free)(struct st_io_socket * socket);
		struct st_stream_writer * (*get_input_stream)(struct st_io_socket * socket);
		struct st_stream_reader * (*get_output_stream)(struct st_io_socket * socket);
		int (*is_connected)(struct st_io_socket * socket);
	} * ops;
	void * data;
};

struct st_io_socket * st_io_socket_new(void);

#endif

