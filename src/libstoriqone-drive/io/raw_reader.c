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

// errno
#include <errno.h>
// free
#include <stdlib.h>
// send
#include <sys/socket.h>
// send
#include <sys/types.h>
// close
#include <unistd.h>

#include <libstoriqone/io.h>
#include <libstoriqone/json.h>
#include <libstoriqone/value.h>

#include "io.h"
#include "../listen.h"
#include "../peer.h"

static void sodr_io_raw_reader_close(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_raw_reader_end_of_file(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_raw_reader_forward(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_raw_reader_init(void) __attribute__((constructor));
static void sodr_io_raw_reader_read(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_raw_reader_rewind(struct sodr_peer * peer, struct so_value * request);

static struct sodr_command commands[] = {
	{ 0, "close",       sodr_io_raw_reader_close },
	{ 0, "end of file", sodr_io_raw_reader_end_of_file },
	{ 0, "forward",     sodr_io_raw_reader_forward },
	{ 0, "read",        sodr_io_raw_reader_read },
	{ 0, "rewind",      sodr_io_raw_reader_rewind },

	{ 0, NULL, NULL },
};


void sodr_io_raw_reader(void * arg) {
	sodr_io_process(arg, commands);
}

static void sodr_io_raw_reader_init() {
	sodr_io_init(commands);
}


static void sodr_io_raw_reader_close(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	int failed = peer->stream_reader->ops->close(peer->stream_reader);
	int last_errno = peer->stream_reader->ops->last_errno(peer->stream_reader);

	if (failed == 0) {
		peer->stream_reader->ops->free(peer->stream_reader);
		peer->stream_reader = NULL;
		close(peer->fd_data);
		peer->fd_data = -1;
		free(peer->buffer);
		peer->buffer = NULL;
		peer->buffer_length = 0;
	}

	sodr_io_print_throughtput(peer);

	struct so_value * response = so_value_pack("{sisi}", "returned", failed, "last errno", last_errno);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);

	close(peer->fd_cmd);
	peer->fd_cmd = -1;
}

static void sodr_io_raw_reader_end_of_file(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	bool eof = peer->stream_reader->ops->end_of_file(peer->stream_reader);
	int last_errno = peer->stream_reader->ops->last_errno(peer->stream_reader);

	struct so_value * response = so_value_pack("{sbsi}", "returned", eof, "last errno", last_errno);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_raw_reader_forward(struct sodr_peer * peer, struct so_value * request) {
	off_t offset = 0;
	so_value_unpack(request, "{s{sz}}",
		"params",
			"offset", &offset
	);

	off_t new_position = peer->stream_reader->ops->forward(peer->stream_reader, offset);
	int last_errno = peer->stream_reader->ops->last_errno(peer->stream_reader);

	struct so_value * response = so_value_pack("{szsi}",
		"returned", new_position,
		"last errno", last_errno
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_raw_reader_read(struct sodr_peer * peer, struct so_value * request) {
	ssize_t length = 0;
	so_value_unpack(request, "{s{sz}}", "params", "length", &length);

	ssize_t nb_total_read = 0;
	while (nb_total_read < length) {
		ssize_t nb_will_read = length - nb_total_read;
		if (nb_will_read > peer->buffer_length)
			nb_will_read = peer->buffer_length;

		ssize_t nb_read = peer->stream_reader->ops->read(peer->stream_reader, peer->buffer, nb_will_read);
		if (nb_read < 0) {
			int last_errno = peer->stream_reader->ops->last_errno(peer->stream_reader);
			struct so_value * response = so_value_pack("{sisi}", "returned", -1, "last errno", last_errno);
			so_json_encode_to_fd(response, peer->fd_cmd, true);
			so_value_free(response);
			return;
		}
		if (nb_read == 0)
			break;

		nb_total_read += nb_read;
		peer->nb_total_bytes += nb_read;

		send(peer->fd_data, peer->buffer, nb_read, 0);
	}

	struct so_value * response = so_value_pack("{szsi}", "returned", nb_total_read, "last errno", 0);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_raw_reader_rewind(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	int failed = peer->stream_reader->ops->rewind(peer->stream_reader);
	int last_errno = peer->stream_reader->ops->last_errno(peer->stream_reader);

	struct so_value * response = so_value_pack("{sisi}", "returned", failed, "last errno", last_errno);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

