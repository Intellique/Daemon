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

// free
#include <stdlib.h>
// strcmp
#include <string.h>
// recv
#include <sys/socket.h>
// recv, send
#include <sys/types.h>
// read, send
#include <unistd.h>

#include <libstone/checksum.h>
#include <libstone/io.h>
#include <libstone/json.h>
#include <libstone/socket.h>
#include <libstone/poll.h>
#include <libstone/slot.h>
#include <libstone/string.h>
#include <libstone/value.h>

#include "drive.h"
#include "listen.h"
#include "peer.h"

static struct st_database_connection * stdr_db = NULL;
static struct st_value * stdr_config = NULL;
static unsigned int stdr_nb_clients = 0;
static struct stdr_peer * stdr_current_peer = NULL;

static void stdr_socket_accept(int fd_server, int fd_client, struct st_value * client);
static void stdr_socket_message(int fd, short event, void * data);

static void stdr_socket_command_find_best_block_size(struct stdr_peer * peer, struct st_value * request, int fd);
static void stdr_socket_command_format_media(struct stdr_peer * peer, struct st_value * request, int fd);
static void stdr_socket_command_get_raw_reader(struct stdr_peer * peer, struct st_value * request, int fd);
static void stdr_socket_command_get_raw_writer(struct stdr_peer * peer, struct st_value * request, int fd);
static void stdr_socket_command_lock(struct stdr_peer * peer, struct st_value * request, int fd);
static void stdr_socket_command_release(struct stdr_peer * peer, struct st_value * request, int fd);
static void stdr_socket_command_sync(struct stdr_peer * peer, struct st_value * request, int fd);

static struct stdr_socket_command {
	unsigned long hash;
	char * name;
	void (*function)(struct stdr_peer * peer, struct st_value * request, int fd);
} commands[] = {
	{ 0, "find best block size", stdr_socket_command_find_best_block_size },
	{ 0, "format media",         stdr_socket_command_format_media },
	{ 0, "get raw reader",       stdr_socket_command_get_raw_reader },
	{ 0, "get raw writer",       stdr_socket_command_get_raw_writer },
	{ 0, "lock",                 stdr_socket_command_lock },
	{ 0, "release",              stdr_socket_command_release },
	{ 0, "sync",                 stdr_socket_command_sync },

	{ 0, NULL, NULL }
};


void stdr_listen_configure(struct st_value * config) {
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		commands[i].hash = st_string_compute_hash2(commands[i].name);

	if (stdr_config != NULL)
		st_value_free(stdr_config);
	stdr_config = st_value_share(config);

	st_socket_server(config, stdr_socket_accept);
}

bool stdr_listen_is_locked() {
	return stdr_current_peer != NULL;
}

unsigned int stdr_listen_nb_clients() {
	return stdr_nb_clients;
}

void stdr_listen_set_db_connection(struct st_database_connection * db) {
	stdr_db = db;
}


static void stdr_socket_accept(int fd_server __attribute__((unused)), int fd_client, struct st_value * client __attribute__((unused))) {
	struct stdr_peer * peer = stdr_peer_new(fd_client);

	st_poll_register(fd_client, POLLIN | POLLHUP, stdr_socket_message, peer, NULL);
	stdr_nb_clients++;
}

static void stdr_socket_message(int fd, short event, void * data) {
	struct stdr_peer * peer = data;

	if (event & POLLHUP) {
		stdr_nb_clients--;

		if (peer == stdr_current_peer)
			stdr_current_peer = NULL;
		stdr_peer_free(peer);
		return;
	}

	struct st_value * request = st_json_parse_fd(fd, -1);
	char * command = NULL;
	if (request == NULL || st_value_unpack(request, "{ss}", "command", &command) < 0) {
		if (request != NULL)
			st_value_free(request);
		return;
	}

	const unsigned long hash = st_string_compute_hash2(command);
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		if (hash == commands[i].hash) {
			commands[i].function(peer, request, fd);
			break;
		}
	st_value_free(request);

	if (commands[i].name == NULL) {
		struct st_value * response = st_value_new_boolean(true);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
	}
}


static void stdr_socket_command_find_best_block_size(struct stdr_peer * peer, struct st_value * request __attribute__((unused)), int fd) {
	if (stdr_current_peer != peer) {
		struct st_value * response = st_value_pack("{sb}", "returned", -1L);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	struct st_drive_driver * driver = stdr_drive_get();
	struct st_drive * drive = driver->device;

	ssize_t block_size = drive->ops->find_best_block_size(stdr_db);
	struct st_value * response = st_value_pack("{si}", "returned", block_size);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

static void stdr_socket_command_format_media(struct stdr_peer * peer, struct st_value * request __attribute__((unused)), int fd) {
	if (stdr_current_peer == peer) {
		struct st_drive_driver * driver = stdr_drive_get();
		struct st_drive * drive = driver->device;

		long int failed = drive->ops->format_media(stdr_db);
		struct st_value * response = st_value_pack("{si}", "returned", failed);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
	} else {
		struct st_value * response = st_value_pack("{si}", "returned", -1L);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
	}
}

static void stdr_socket_command_get_raw_reader(struct stdr_peer * peer, struct st_value * request, int fd) {
	long int position = -1;
	char * cookie = NULL;
	st_value_unpack(request, "{siss}", "file position", &position, "cookie", &cookie);

	if (cookie == NULL || peer->cookie == NULL || strcmp(cookie, peer->cookie) != 0) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	free(cookie);

	struct st_drive_driver * driver = stdr_drive_get();
	struct st_drive * drive = driver->device;

	if (drive->slot->media == NULL) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	struct st_stream_reader * reader = drive->ops->get_raw_reader(position, stdr_db);

	struct st_value * tmp_config = st_value_copy(stdr_config, true);
	int tmp_socket = st_socket_server_temp(tmp_config);

	struct st_value * response = st_value_pack("{sbsOsi}",
		"status", true,
		"socket", tmp_config,
		"block size", reader->ops->get_block_size(reader)
	);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);

	int data_socket = st_socket_accept_and_close(tmp_socket, tmp_config);
	st_value_free(tmp_config);

	ssize_t buffer_size = reader->ops->get_block_size(reader);
	char * buffer = malloc(buffer_size);

	struct st_value * command = st_json_parse_fd(fd, -1);

	bool stop = true;
	st_value_unpack(command, "{sbsi}", "stop", &stop);
	st_value_free(command);

	long int l_errno = 0;
	while (!stop) {
		char * str_command = NULL;
		st_value_unpack(command, "{ss}", "command", &str_command);

		if (!strcmp(str_command, "forward")) {
			off_t offset = 0;
			st_value_unpack(command, "{si}", "offset", &offset);
			st_value_free(command);

			ssize_t new_position = reader->ops->forward(reader, offset);

			response = st_value_pack("{sbsisisb}",
				"status", true,
				"new position", new_position,
				"errno", l_errno,
				"close", false
			);
			st_json_encode_to_fd(response, fd, true);
			st_value_free(response);
		} else if (!strcmp(str_command, "read")) {
			long int length = -1;
			ssize_t nb_total_read = 0;

			while (nb_total_read < length) {
				ssize_t will_read = length - nb_total_read;
				if (will_read > buffer_size)
					will_read = buffer_size;

				ssize_t nb_read = reader->ops->read(reader, buffer, will_read);
				if (nb_read < 0) {
					l_errno = reader->ops->last_errno(reader);
					break;
				}
				if (nb_read == 0)
					break;

				ssize_t nb_write = send(data_socket, buffer, nb_read, MSG_NOSIGNAL);
				if (nb_write > 0) {
					nb_total_read += nb_write;
				} else if (nb_write < 0) {
					break;
				}
			}

			response = st_value_pack("{sbsisisb}",
				"status", true,
				"nb read", nb_total_read,
				"errno", l_errno,
				"close", false
			);

			st_json_encode_to_fd(response, fd, true);
			st_value_free(response);
		}

		command = st_json_parse_fd(fd, -1);

		if (command != NULL)
			st_value_unpack(command, "{sbsi}", "stop", &stop);
		else
			break;
		st_value_free(command);
	}

	free(buffer);
	close(data_socket);

	response = st_value_pack("{sbsb}",
		"status", true,
		"close", true
	);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

static void stdr_socket_command_get_raw_writer(struct stdr_peer * peer, struct st_value * request, int fd) {
	bool append = true;
	char * cookie = NULL;
	st_value_unpack(request, "{sbss}", "append", &append, "cookie", &cookie);

	if (cookie == NULL || peer->cookie == NULL || strcmp(cookie, peer->cookie) != 0) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	free(cookie);

	struct st_drive_driver * driver = stdr_drive_get();
	struct st_drive * drive = driver->device;

	if (drive->slot->media == NULL) {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
		return;
	}

	struct st_stream_writer * writer = drive->ops->get_raw_writer(append, stdr_db);

	struct st_value * tmp_config = st_value_copy(stdr_config, true);
	int tmp_socket = st_socket_server_temp(tmp_config);

	struct st_value * response = st_value_pack("{sbsOsi}",
		"status", true,
		"socket", tmp_config,
		"block size", writer->ops->get_block_size(writer)
	);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);

	int data_socket = st_socket_accept_and_close(tmp_socket, tmp_config);
	st_value_free(tmp_config);

	ssize_t buffer_size = writer->ops->get_block_size(writer);
	char * buffer = malloc(buffer_size);

	struct st_value * command = st_json_parse_fd(fd, -1);

	long int length = -1;
	bool stop = true;
	st_value_unpack(command, "{sbsi}", "stop", &stop, "length", &length);
	st_value_free(command);

	long int l_errno = 0;
	while (!stop && length >= 0) {
		ssize_t nb_total_write = 0;

		while (nb_total_write < length) {
			ssize_t will_write = length - nb_total_write;
			if (will_write > buffer_size)
				will_write = buffer_size;

			ssize_t nb_read = recv(data_socket, buffer, will_write, 0);
			if (nb_read < 0)
				break;

			ssize_t nb_write = writer->ops->write(writer, buffer, nb_read);
			if (nb_write > 0)
				nb_total_write += nb_write;
			else if (nb_write < 0) {
				l_errno = writer->ops->last_errno(writer);
				break;
			}
		}

		response = st_value_pack("{sbsisisb}",
			"status", true,
			"nb write", nb_total_write,
			"errno", l_errno,
			"close", false
		);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);

		command = st_json_parse_fd(fd, -1);

		if (command != NULL)
			st_value_unpack(command, "{sbsi}", "stop", &stop, "length", &length);
		else
			break;
		st_value_free(command);
	}

	free(buffer);
	close(data_socket);

	response = st_value_pack("{sbsb}",
		"status", true,
		"close", false
	);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

static void stdr_socket_command_lock(struct stdr_peer * peer, struct st_value * request __attribute__((unused)), int fd) {
	if (stdr_current_peer == NULL) {
		stdr_current_peer = peer;

		free(peer->cookie);
		peer->cookie = st_checksum_gen_salt(NULL, 32);

		struct st_value * response = st_value_pack("{sbss}", "status", true, "cookie", peer->cookie);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
	} else {
		struct st_value * response = st_value_pack("{sb}", "status", false);
		st_json_encode_to_fd(response, fd, true);
		st_value_free(response);
	}
}

static void stdr_socket_command_release(struct stdr_peer * peer, struct st_value * request __attribute__((unused)), int fd) {
	bool ok = stdr_current_peer == peer;

	if (ok)
		stdr_current_peer = NULL;

	free(peer->cookie);
	peer->cookie = NULL;

	struct st_value * response = st_value_pack("{sb}", "status", ok);
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

static void stdr_socket_command_sync(struct stdr_peer * peer __attribute__((unused)), struct st_value * request __attribute__((unused)), int fd) {
	struct st_drive_driver * driver = stdr_drive_get();
	struct st_drive * dr = driver->device;

	struct st_value * response = st_value_pack("{so}", "drive", st_drive_convert(dr, true));
	st_json_encode_to_fd(response, fd, true);
	st_value_free(response);
}

