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
// recv
#include <sys/socket.h>
// recv
#include <sys/types.h>
// close
#include <unistd.h>

#include <libstoriqone/format.h>
#include <libstoriqone/json.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

#include "io.h"
#include "../peer.h"

static void sodr_io_format_writer_add_file(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_add_label(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_close(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_end_of_file(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_init(void) __attribute__((constructor));
static void sodr_io_format_writer_restart_file(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_write(struct sodr_peer * peer, struct so_value * request);

static struct sodr_command commands[] = {
	{ 0, "add file",     sodr_io_format_writer_add_file },
	{ 0, "add label",    sodr_io_format_writer_add_label },
	{ 0, "close",        sodr_io_format_writer_close },
	{ 0, "end of file",  sodr_io_format_writer_end_of_file },
	{ 0, "restart file", sodr_io_format_writer_restart_file },
	{ 0, "write",        sodr_io_format_writer_write },

	{ 0, NULL, NULL },
};


void sodr_io_format_writer(void * arg) {
	sodr_io_process(arg, commands);
}

static void sodr_io_format_writer_init() {
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		commands[i].hash = so_string_compute_hash2(commands[i].name);
}


static void sodr_io_format_writer_add_file(struct sodr_peer * peer, struct so_value * request) {
	struct so_value * vfile = NULL;
	so_value_unpack(request, "{s{so}}", "params", "file", &vfile);

	struct so_format_file file;
	so_format_file_init(&file);
	so_format_file_sync(&file, vfile);

	enum so_format_writer_status status = peer->format_writer->ops->add_file(peer->format_writer, &file);
	long int last_errno = peer->format_writer->ops->last_errno(peer->format_writer);
	ssize_t position = peer->format_writer->ops->position(peer->format_writer);
	ssize_t available_size = peer->format_writer->ops->get_available_size(peer->format_writer);

	so_format_file_free(&file);

	struct so_value * response = so_value_pack("{sisisisi}",
		"returned", (long) status,
		"last errno", last_errno,
		"position", position,
		"available size", available_size
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_writer_add_label(struct sodr_peer * peer, struct so_value * request) {
	char * label = NULL;
	so_value_unpack(request, "{s{so}}", "params", "lable", &label);

	enum so_format_writer_status status = peer->format_writer->ops->add_label(peer->format_writer, label);
	long int last_errno = peer->format_writer->ops->last_errno(peer->format_writer);
	ssize_t position = peer->format_writer->ops->position(peer->format_writer);
	ssize_t available_size = peer->format_writer->ops->get_available_size(peer->format_writer);

	free(label);

	struct so_value * response = so_value_pack("{sisisisi}",
		"returned", (long) status,
		"last errno", last_errno,
		"position", position,
		"available size", available_size
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_writer_close(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	long int failed = peer->format_writer->ops->close(peer->format_writer);
	long int last_errno = peer->format_writer->ops->last_errno(peer->format_writer);

	struct so_value * response = so_value_pack("{sisi}", "returned", failed, "last errno", last_errno);

	if (failed == 0) {
		if (peer->has_checksums) {
			struct so_value * digests = peer->format_writer->ops->get_digests(peer->format_writer);
			so_value_hashtable_put2(response, "digests", digests, true);
		}

		peer->format_writer->ops->free(peer->format_writer);
		peer->format_writer = NULL;
		close(peer->fd_data);
		peer->fd_data = -1;
		free(peer->buffer);
		peer->buffer = NULL;
		peer->buffer_length = 0;
		peer->has_checksums = false;
	}

	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);

	close(peer->fd_cmd);
	peer->fd_cmd = -1;
}

static void sodr_io_format_writer_end_of_file(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	bool eof = peer->format_writer->ops->end_of_file(peer->format_writer);
	long int last_errno = peer->format_writer->ops->last_errno(peer->format_writer);

	struct so_value * response = so_value_pack("{sbsi}", "returned", eof, "last errno", last_errno);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_writer_restart_file(struct sodr_peer * peer, struct so_value * request) {
	struct so_value * vfile = NULL;
	ssize_t position = 0;
	so_value_unpack(request, "{s{sosi}}", "params", "file", &vfile, "position", &position);

	struct so_format_file file;
	so_format_file_init(&file);
	so_format_file_sync(&file, vfile);

	enum so_format_writer_status status = peer->format_writer->ops->restart_file(peer->format_writer, &file, position);
	long int last_errno = peer->format_writer->ops->last_errno(peer->format_writer);
	ssize_t new_position = peer->format_writer->ops->position(peer->format_writer);
	ssize_t available_size = peer->format_writer->ops->get_available_size(peer->format_writer);

	so_format_file_free(&file);

	struct so_value * response = so_value_pack("{sisisisi}",
		"returned", (long) status,
		"last errno", last_errno,
		"position", new_position,
		"available size", available_size
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_writer_write(struct sodr_peer * peer, struct so_value * request) {
	ssize_t length = 0;
	so_value_unpack(request, "{s{si}}", "params", "length", &length);

	ssize_t nb_total_write = 0;
	while (nb_total_write < length) {
		ssize_t nb_will_read = length - nb_total_write;
		if (nb_will_read > peer->buffer_length)
			nb_will_read = peer->buffer_length;

		ssize_t nb_read = recv(peer->fd_data, peer->buffer, nb_will_read, 0);

		if (nb_read < 0) {
			struct so_value * response = so_value_pack("{sisi}", "returned", -1L, "last errno", (long) errno);
			so_json_encode_to_fd(response, peer->fd_cmd, true);
			so_value_free(response);
			return;
		}

		ssize_t nb_write = peer->format_writer->ops->write(peer->format_writer, peer->buffer, nb_read);
		if (nb_write < 0) {
			long int last_errno = peer->format_writer->ops->last_errno(peer->format_writer);
			struct so_value * response = so_value_pack("{sisi}", "returned", -1L, "last errno", last_errno);
			so_json_encode_to_fd(response, peer->fd_cmd, true);
			so_value_free(response);
			return;
		}

		nb_total_write += nb_write;
	}

	ssize_t position = peer->format_writer->ops->position(peer->format_writer);
	ssize_t available_size = peer->format_writer->ops->get_available_size(peer->format_writer);

	struct so_value * response = so_value_pack("{sisisisi}",
		"returned", nb_total_write,
		"last errno", 0L,
		"position", position,
		"available size", available_size
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

