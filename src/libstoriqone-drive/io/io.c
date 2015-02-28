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

// poll
#include <poll.h>
// NULL
#include <stddef.h>

#include <libstoriqone/json.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

#include "io.h"
#include "../peer.h"


void sodr_io_init(struct sodr_command commands[]) {
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		commands[i].hash = so_string_compute_hash2(commands[i].name);
}

void sodr_io_process(struct sodr_peer * peer, struct sodr_command commands[]) {
	struct pollfd fd_cmd = { peer->fd_cmd, POLLIN | POLLHUP, 0 };

	int nb_events;
	while (nb_events = poll(&fd_cmd, 1, -1), nb_events > 0) {
		if (fd_cmd.revents & POLLHUP)
			break;

		struct so_value * request = so_json_parse_fd(peer->fd_cmd, -1);
		char * command = NULL;
		if (request == NULL || so_value_unpack(request, "{ss}", "command", &command) < 0) {
			if (request != NULL)
				so_value_free(request);
			break;
		}

		const unsigned long hash = so_string_compute_hash2(command);
		unsigned int i;
		for (i = 0; commands[i].name != NULL; i++)
			if (hash == commands[i].hash) {
				commands[i].function(peer, request);
				break;
			}
		so_value_free(request);

		if (commands[i].name == NULL) {
			struct so_value * response = so_value_new_boolean(true);
			so_json_encode_to_fd(response, peer->fd_cmd, true);
			so_value_free(response);
		}

		if (peer->fd_cmd < 0)
			break;

		fd_cmd.revents = 0;
	}
}

