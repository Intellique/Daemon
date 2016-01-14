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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_DRIVE_PEER_H__
#define __LIBSTORIQONE_DRIVE_PEER_H__

// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>
// time_t
#include <sys/time.h>

struct sodr_peer {
	int fd;
	char * job_id;
	unsigned int job_num_run;

	struct so_stream_reader * stream_reader;
	struct so_stream_writer * stream_writer;

	struct so_format_reader * format_reader;
	struct so_format_writer * format_writer;

	int fd_cmd;
	int fd_data;
	char * buffer;
	ssize_t buffer_length;
	bool has_checksums;

	struct timespec start_time;
	ssize_t nb_total_bytes;

	bool disconnected;
	volatile bool owned;

	struct sodr_peer * next;
	struct sodr_peer * previous;
};

void sodr_peer_free(struct sodr_peer * peer);
struct sodr_peer * sodr_peer_new(int fd, struct sodr_peer * previous);

#endif

