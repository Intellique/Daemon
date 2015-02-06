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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_DRIVE_PEER_H__
#define __LIBSTORIQONE_DRIVE_PEER_H__

// ssize_t
#include <sys/types.h>

struct sodr_peer {
	int fd;

	struct so_stream_reader * stream_reader;
	struct so_stream_writer * stream_writer;

	struct so_format_reader * format_reader;
	struct so_format_writer * format_writer;

	int fd_data;
	char * buffer;
	ssize_t buffer_length;
};

void sodr_peer_free(struct sodr_peer * peer);
struct sodr_peer * sodr_peer_new(int fd);

#endif

