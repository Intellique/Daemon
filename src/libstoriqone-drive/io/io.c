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

// dgettext
#include <libintl.h>
// poll
#include <poll.h>
// free
#include <stdlib.h>
// clock_gettime
#include <time.h>
// close
#include <unistd.h>

#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/format.h>
#include <libstoriqone/log.h>
#include <libstoriqone/io.h>
#include <libstoriqone/json.h>
#include <libstoriqone/string.h>
#include <libstoriqone/time.h>
#include <libstoriqone/value.h>

#include "io.h"
#include "../drive.h"
#include "../listen.h"
#include "../peer.h"


void sodr_io_init(struct sodr_command commands[]) {
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		commands[i].hash = so_string_compute_hash2(commands[i].name);
}

void sodr_io_print_throughtput(struct sodr_peer * peer) {
	struct timespec end_time = { 0, 0 };
	clock_gettime(CLOCK_MONOTONIC, &end_time);

	double time_spent = so_time_diff(&end_time, &peer->start_time) / 1000000000L;
	double speed = peer->nb_total_bytes / time_spent;

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	char buf_speed[24];
	so_file_convert_size_to_string((ssize_t) speed, buf_speed, 24);
	so_log_write(so_log_level_debug,
		dgettext("libstoriqone-drive", "[%s | %s | #%u]: Throughput: %s/s"),
		drive->vendor, drive->model, drive->index, buf_speed);
}

void sodr_io_process(struct sodr_peer * peer, struct sodr_command commands[]) {
	struct pollfd fd_cmd = { peer->fd_cmd, POLLIN | POLLHUP, 0 };

	int nb_events;
	while (nb_events = poll(&fd_cmd, 1, -1), nb_events > 0) {
		if (fd_cmd.revents & POLLHUP)
			break;

		struct so_value * request = so_json_parse_fd(peer->fd_cmd, -1);
		const char * command = NULL;
		if (request == NULL || so_value_unpack(request, "{sS}", "command", &command) < 0) {
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

	if (peer->stream_reader != NULL) {
		peer->stream_reader->ops->free(peer->stream_reader);
		peer->stream_reader = NULL;
	}
	if (peer->stream_writer != NULL) {
		peer->stream_writer->ops->free(peer->stream_writer);
		peer->stream_writer = NULL;
	}
	if (peer->format_reader != NULL) {
		peer->format_reader->ops->free(peer->format_reader);
		peer->format_reader = NULL;
	}
	if (peer->format_writer != NULL) {
		peer->format_writer->ops->free(peer->format_writer);
		peer->format_writer = NULL;
	}

	if (peer->db_connection != NULL) {
		peer->db_connection->ops->close(peer->db_connection);
		peer->db_connection = NULL;
	}

	if (peer->fd_cmd > -1) {
		close(peer->fd_cmd);
		peer->fd_cmd = -1;
	}
	if (peer->fd_data > -1) {
		close(peer->fd_data);
		peer->fd_data = -1;
	}

	peer->owned = false;
	sodr_peer_set(NULL);
	sodr_listen_remove_peer(peer);

	sodr_listen_reset_peer();

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;
	drive->status = so_drive_status_loaded_idle;
}
