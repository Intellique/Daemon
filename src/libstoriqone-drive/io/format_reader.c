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

#include <libstoriqone/format.h>
#include <libstoriqone/json.h>
#include <libstoriqone/value.h>

#include "io.h"
#include "../listen.h"
#include "../peer.h"

static void sodr_io_format_reader_close(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_reader_end_of_file(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_reader_forward(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_reader_get_header(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_reader_get_root(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_reader_init(void) __attribute__((constructor));
static void sodr_io_format_reader_read(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_reader_read_to_end_of_data(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_reader_rewind(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_reader_skip_file(struct sodr_peer * peer, struct so_value * request);

static struct sodr_command commands[] = {
	{ 0, "close",               sodr_io_format_reader_close },
	{ 0, "end of file",         sodr_io_format_reader_end_of_file },
	{ 0, "forward",             sodr_io_format_reader_forward },
	{ 0, "get header",          sodr_io_format_reader_get_header },
	{ 0, "get root",            sodr_io_format_reader_get_root },
	{ 0, "read",                sodr_io_format_reader_read },
	{ 0, "read to end of data", sodr_io_format_reader_read_to_end_of_data },
	{ 0, "rewind",              sodr_io_format_reader_rewind },
	{ 0, "skip file",           sodr_io_format_reader_skip_file },

	{ 0, NULL, NULL },
};


void sodr_io_format_reader(void * arg) {
	sodr_io_process(arg, commands);
}

static void sodr_io_format_reader_init() {
	sodr_io_init(commands);
}


static void sodr_io_format_reader_close(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	int failed = peer->format_reader->ops->close(peer->format_reader);
	int last_errno = peer->format_reader->ops->last_errno(peer->format_reader);

	struct so_value * response = so_value_pack("{sisi}", "returned", failed, "last errno", last_errno);

	if (failed == 0) {
		if (peer->has_checksums) {
			struct so_value * digests = peer->format_reader->ops->get_digests(peer->format_reader);
			so_value_hashtable_put2(response, "digests", digests, true);
		}

		peer->format_reader->ops->free(peer->format_reader);
		peer->format_reader = NULL;
		close(peer->fd_data);
		peer->fd_data = -1;
		free(peer->buffer);
		peer->buffer = NULL;
		peer->buffer_length = 0;
		peer->has_checksums = false;
	}

	sodr_io_print_throughtput(peer);

	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);

	close(peer->fd_cmd);
	peer->fd_cmd = -1;
}

static void sodr_io_format_reader_end_of_file(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	bool eof = peer->format_reader->ops->end_of_file(peer->format_reader);
	int last_errno = peer->format_reader->ops->last_errno(peer->format_reader);

	struct so_value * response = so_value_pack("{sbsi}", "returned", eof, "last errno", last_errno);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_reader_forward(struct sodr_peer * peer, struct so_value * request) {
	off_t offset = (off_t) -1;

	so_value_unpack(request, "{s{sz}}",
		"params",
			"offset", &offset
	);

	ssize_t current_position = peer->format_reader->ops->position(peer->format_reader);
	enum so_format_reader_header_status status = peer->format_reader->ops->forward(peer->format_reader, offset);
	ssize_t new_position = peer->format_reader->ops->position(peer->format_reader);
	int last_errno = peer->format_reader->ops->last_errno(peer->format_reader);

	peer->nb_total_bytes += new_position - current_position;

	struct so_value * response = so_value_pack("{siszsi}",
		"returned", status,
		"position", new_position,
		"last errno", last_errno
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_reader_get_header(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	struct so_format_file file;
	so_format_file_init(&file);

	ssize_t current_position = peer->format_reader->ops->position(peer->format_reader);
	enum so_format_reader_header_status status = peer->format_reader->ops->get_header(peer->format_reader, &file);
	ssize_t new_position = peer->format_reader->ops->position(peer->format_reader);
	int last_errno = peer->format_reader->ops->last_errno(peer->format_reader);

	peer->nb_total_bytes += new_position - current_position;

	struct so_value * response = so_value_pack("{s{siso}szsi}",
		"returned",
			"status", status,
			"file", so_format_file_convert(&file),
		"position", new_position,
		"last errno", last_errno
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);

	so_format_file_free(&file);
}

static void sodr_io_format_reader_get_root(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	char * root = peer->format_reader->ops->get_root(peer->format_reader);

	struct so_value * response = so_value_pack("{ss}", "returned", root);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);

	free(root);
}

static void sodr_io_format_reader_read(struct sodr_peer * peer, struct so_value * request) {
	ssize_t length = 0;
	so_value_unpack(request, "{s{sz}}", "params", "length", &length);

	ssize_t nb_total_read = 0;
	while (nb_total_read < length) {
		ssize_t nb_will_read = length - nb_total_read;
		if (nb_will_read > peer->buffer_length)
			nb_will_read = peer->buffer_length;

		ssize_t nb_read = peer->format_reader->ops->read(peer->format_reader, peer->buffer, nb_will_read);
		if (nb_read < 0) {
			int last_errno = peer->format_reader->ops->last_errno(peer->format_reader);
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

	ssize_t position = peer->format_reader->ops->position(peer->format_reader);

	struct so_value * response = so_value_pack("{szszsi}",
		"returned", nb_total_read,
		"position", position,
		"last errno", 0
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_reader_read_to_end_of_data(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	ssize_t current_position = peer->format_reader->ops->position(peer->format_reader);
	ssize_t new_position = peer->format_reader->ops->read_to_end_of_data(peer->format_reader);
	int last_errno = peer->format_reader->ops->last_errno(peer->format_reader);

	peer->nb_total_bytes += new_position - current_position;

	struct so_value * response = so_value_pack("{szsi}",
		"returned", new_position,
		"last errno", last_errno
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_reader_rewind(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	sodr_io_print_throughtput(peer);
	peer->nb_total_bytes = 0;

	int failed = peer->format_reader->ops->rewind(peer->format_reader);
	int last_errno = peer->format_reader->ops->last_errno(peer->format_reader);

	struct so_value * response = so_value_pack("{sisi}", "returned", failed, "last errno", last_errno);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_reader_skip_file(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	enum so_format_reader_header_status status = peer->format_reader->ops->skip_file(peer->format_reader);
	ssize_t position = peer->format_reader->ops->position(peer->format_reader);
	int last_errno = peer->format_reader->ops->last_errno(peer->format_reader);

	struct so_value * response = so_value_pack("{siszsi}",
		"returned", status,
		"position", position,
		"last errno", last_errno
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

