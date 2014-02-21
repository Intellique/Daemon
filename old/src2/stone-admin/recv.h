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
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Tue, 01 May 2012 20:55:32 +0200                         *
\*************************************************************************/

#ifndef __STONEADMIN_RECV_H__
#define __STONEADMIN_RECV_H__

struct st_stream_reader;
struct st_stream_writer;

enum stad_query {
	stad_query_unknown,
	stad_query_getline,
	stad_query_getpassword,
	stad_query_status,
};

enum stad_query stad_recv(struct st_stream_reader * reader, char ** prompt, int * status, json_t * lines, json_t ** data);
int stad_send(struct st_stream_writer * writer, json_t * data);

#endif

