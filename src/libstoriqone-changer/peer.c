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

// bzero
#include <strings.h>
// free, malloc
#include <stdlib.h>

#include <libstoriqone/value.h>

#include "peer.h"

void sochgr_peer_free(struct sochgr_peer * peer) {
	free(peer->job_id);

	if (peer->request != NULL)
		so_value_free(peer->request);

	free(peer);
}

struct sochgr_peer * sochgr_peer_new(int fd, bool defer) {
	struct sochgr_peer * peer = malloc(sizeof(struct sochgr_peer));
	bzero(peer, sizeof(struct sochgr_peer));

	peer->fd = fd;
	peer->job_id = NULL;
	peer->nb_waiting_medias = 0;

	peer->defer = defer;
	peer->request = NULL;
	peer->command = NULL;

	peer->next = NULL;

	return peer;
}
