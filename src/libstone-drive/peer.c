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

// bzero
#include <strings.h>
// free, malloc
#include <stdlib.h>
// close
#include <unistd.h>

#include <libstone-drive/io.h>

#include "peer.h"

void stdr_peer_free(struct stdr_peer * peer) {
	free(peer->cookie);

	if (peer->reader != NULL)
		peer->reader->ops->free(peer->reader);
	peer->reader = NULL;

	if (peer->data_socket > -1)
		close(peer->data_socket);

	free(peer);
}

struct stdr_peer * stdr_peer_new(int fd) {
	struct stdr_peer * peer = malloc(sizeof(struct stdr_peer));
	bzero(peer, sizeof(struct stdr_peer));

	peer->fd = fd;
	peer->data_socket = -1;

	return peer;
}

