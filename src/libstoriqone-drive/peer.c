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

// bzero
#include <strings.h>
// free, malloc
#include <stdlib.h>
// close
#include <unistd.h>

#include <libstoriqone/format.h>
#include <libstoriqone/io.h>

#include "peer.h"

void sodr_peer_free(struct sodr_peer * peer) {
	if (peer->stream_reader != NULL) {
		peer->stream_reader->ops->close(peer->stream_reader);
		peer->stream_reader->ops->free(peer->stream_reader);
	}

	if (peer->stream_writer != NULL) {
		peer->stream_writer->ops->close(peer->stream_writer);
		peer->stream_writer->ops->free(peer->stream_writer);
	}

	if (peer->format_reader != NULL) {
		peer->format_reader->ops->close(peer->format_reader);
		peer->format_reader->ops->free(peer->format_reader);
	}

	if (peer->format_writer != NULL) {
		peer->format_writer->ops->close(peer->format_writer);
		peer->format_writer->ops->free(peer->format_writer);
	}

	if (peer->fd_data >= 0)
		close(peer->fd_data);

	free(peer->buffer);

	free(peer);
}

struct sodr_peer * sodr_peer_new(int fd) {
	struct sodr_peer * peer = malloc(sizeof(struct sodr_peer));
	bzero(peer, sizeof(struct sodr_peer));

	peer->fd = fd;

	peer->stream_reader = NULL;
	peer->stream_writer = NULL;

	peer->format_reader = NULL;
	peer->format_writer = NULL;

	peer->fd_data = -1;
	peer->buffer = NULL;
	peer->buffer_length = 0;
	peer->has_checksums = false;

	return peer;
}

