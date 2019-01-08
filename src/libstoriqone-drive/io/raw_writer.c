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
*  Copyright (C) 2013-2019, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// errno
#include <errno.h>
// free
#include <stdlib.h>
// recv
#include <sys/socket.h>
// recv
#include <sys/types.h>
// close
#include <unistd.h>

#include <libstoriqone/io.h>
#include <libstoriqone/json.h>
#include <libstoriqone/thread_pool.h>
#include <libstoriqone/value.h>

#include "io.h"
#include "../peer.h"

static void sodr_io_raw_writer_before_close(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_raw_writer_close(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_raw_writer_create_new_file(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_raw_writer_init(void) __attribute__((constructor));
static void sodr_io_raw_writer_reopen(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_raw_writer_write(struct sodr_peer * peer, struct so_value * request);

static struct sodr_command commands[] = {
	{ 0, "before close",    sodr_io_raw_writer_before_close },
	{ 0, "close",           sodr_io_raw_writer_close },
	{ 0, "create new file", sodr_io_raw_writer_create_new_file },
	{ 0, "reopen",          sodr_io_raw_writer_reopen },
	{ 0, "write",           sodr_io_raw_writer_write },

	{ 0, NULL, NULL },
};


void sodr_io_raw_writer(void * arg) {
	sodr_io_process(arg, commands);
}

static void sodr_io_raw_writer_init() {
	sodr_io_init(commands);
}


static void sodr_io_raw_writer_before_close(struct sodr_peer * peer, struct so_value * request) {
	ssize_t length = 0;
	so_value_unpack(request, "{s{sz}}", "params", "length", &length);

	if (length > peer->buffer_length)
		length = peer->buffer_length;

	ssize_t nb_read = peer->stream_writer->ops->before_close(peer->stream_writer, peer->buffer, length);
	int last_errno = peer->stream_writer->ops->last_errno(peer->stream_writer);
	ssize_t available_size = peer->stream_writer->ops->get_available_size(peer->stream_writer);

	peer->nb_total_bytes += nb_read;

	struct so_value * response = so_value_pack("{szsisz}",
		"returned", nb_read,
		"last errno", last_errno,
		"available size", available_size
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);

	if (nb_read > 0)
		send(peer->fd_data, peer->buffer, nb_read, MSG_NOSIGNAL);
}

static void sodr_io_raw_writer_close(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	int failed = peer->stream_writer->ops->close(peer->stream_writer);
	int last_errno = peer->stream_writer->ops->last_errno(peer->stream_writer);

	if (failed == 0) {
		peer->stream_writer->ops->free(peer->stream_writer);
		peer->stream_writer = NULL;
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

static void sodr_io_raw_writer_create_new_file(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	int failed = peer->stream_writer->ops->create_new_file(peer->stream_writer);

	int last_errno = 0;
	if (failed != 0)
		last_errno = peer->stream_writer->ops->last_errno(peer->stream_writer);

	ssize_t available_size = peer->stream_writer->ops->get_available_size(peer->stream_writer);

	struct so_value * response = so_value_pack("{sisisz}",
		"returned", failed,
		"last errno", last_errno,
		"available size", available_size
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_raw_writer_reopen(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	peer->stream_reader = peer->stream_writer->ops->reopen(peer->stream_writer);
	int last_errno = 0;
	if (peer->stream_reader == NULL)
		last_errno = peer->stream_writer->ops->last_errno(peer->stream_writer);
	else {
		peer->stream_writer->ops->free(peer->stream_writer);
		peer->stream_writer = NULL;

		sodr_io_print_throughtput(peer);
		peer->nb_total_bytes = 0;
	}

	struct so_value * response = so_value_pack("{sbsi}", "returned", peer->stream_reader != NULL, "last errno", last_errno);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);

	so_thread_pool_set_name("raw reader");
	sodr_io_raw_reader(peer);
}

static void sodr_io_raw_writer_write(struct sodr_peer * peer, struct so_value * request) {
	ssize_t length = 0;
	so_value_unpack(request, "{s{sz}}", "params", "length", &length);

	ssize_t nb_total_write = 0;
	while (nb_total_write < length) {
		ssize_t nb_will_read = length - nb_total_write;
		if (nb_will_read > peer->buffer_length)
			nb_will_read = peer->buffer_length;

		ssize_t nb_read = recv(peer->fd_data, peer->buffer, nb_will_read, 0);
		if (nb_read < 0) {
			ssize_t available_size = peer->stream_writer->ops->get_available_size(peer->stream_writer);
			struct so_value * response = so_value_pack("{sisisz}",
				"returned", -1,
				"last errno", errno,
				"available size", available_size
			);
			so_json_encode_to_fd(response, peer->fd_cmd, true);
			so_value_free(response);
			return;
		}

		ssize_t nb_write = peer->stream_writer->ops->write(peer->stream_writer, peer->buffer, nb_read);
		if (nb_write < 0) {
			int last_errno = peer->stream_writer->ops->last_errno(peer->stream_writer);
			ssize_t available_size = peer->stream_writer->ops->get_available_size(peer->stream_writer);
			struct so_value * response = so_value_pack("{sisisz}",
				"returned", -1,
				"last errno", last_errno,
				"available size", available_size
			);
			so_json_encode_to_fd(response, peer->fd_cmd, true);
			so_value_free(response);
			return;
		}

		nb_total_write += nb_write;
		peer->nb_total_bytes += nb_write;
	}

	ssize_t available_size = peer->stream_writer->ops->get_available_size(peer->stream_writer);
	struct so_value * response = so_value_pack("{szsisz}",
		"returned", nb_total_write,
		"last errno", 0,
		"available size", available_size
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}
