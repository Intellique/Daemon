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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// dgettext
#include <libintl.h>
// free
#include <stdlib.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>
// recv
#include <sys/socket.h>
// recv, send
#include <sys/types.h>
// read, send
#include <unistd.h>

#include <libstoriqone/checksum.h>
#include <libstoriqone/file.h>
#include <libstoriqone/io.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

#include "drive.h"
#include "listen.h"
#include "peer.h"

static struct so_database_connection * sodr_db = NULL;
static struct so_value * sodr_config = NULL;
static unsigned int sodr_nb_clients = 0;
static struct sodr_peer * sodr_current_peer = NULL;

static void sodr_socket_accept(int fd_server, int fd_client, struct so_value * client);
static void sodr_socket_message(int fd, short event, void * data);

static void sodr_socket_command_check_header(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_check_support(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_find_best_block_size(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_format_media(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_get_raw_reader(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_get_raw_writer(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_lock(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_release(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_sync(struct sodr_peer * peer, struct so_value * request, int fd);

static struct sodr_socket_command {
	unsigned long hash;
	char * name;
	void (*function)(struct sodr_peer * peer, struct so_value * request, int fd);
} commands[] = {
	{ 0, "check header",         sodr_socket_command_check_header },
	{ 0, "check support",        sodr_socket_command_check_support },
	{ 0, "find best block size", sodr_socket_command_find_best_block_size },
	{ 0, "format media",         sodr_socket_command_format_media },
	{ 0, "get raw reader",       sodr_socket_command_get_raw_reader },
	{ 0, "get raw writer",       sodr_socket_command_get_raw_writer },
	{ 0, "lock",                 sodr_socket_command_lock },
	{ 0, "release",              sodr_socket_command_release },
	{ 0, "sync",                 sodr_socket_command_sync },

	{ 0, NULL, NULL }
};


void sodr_listen_configure(struct so_value * config) {
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		commands[i].hash = so_string_compute_hash2(commands[i].name);

	if (sodr_config != NULL)
		so_value_free(sodr_config);
	sodr_config = so_value_share(config);

	so_socket_server(config, sodr_socket_accept);
}

bool sodr_listen_is_locked() {
	return sodr_current_peer != NULL;
}

unsigned int sodr_listen_nb_clients() {
	return sodr_nb_clients;
}

void sodr_listen_set_db_connection(struct so_database_connection * db) {
	sodr_db = db;
}


static void sodr_socket_accept(int fd_server __attribute__((unused)), int fd_client, struct so_value * client __attribute__((unused))) {
	struct sodr_peer * peer = sodr_peer_new(fd_client);

	so_poll_register(fd_client, POLLIN | POLLHUP, sodr_socket_message, peer, NULL);
	sodr_nb_clients++;
}

static void sodr_socket_message(int fd, short event, void * data) {
	struct sodr_peer * peer = data;

	if (event & POLLHUP) {
		sodr_nb_clients--;

		if (peer == sodr_current_peer)
			sodr_current_peer = NULL;
		sodr_peer_free(peer);
		return;
	}

	struct so_value * request = so_json_parse_fd(fd, -1);
	char * command = NULL;
	if (request == NULL || so_value_unpack(request, "{ss}", "command", &command) < 0) {
		if (request != NULL)
			so_value_free(request);
		return;
	}

	const unsigned long hash = so_string_compute_hash2(command);
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		if (hash == commands[i].hash) {
			commands[i].function(peer, request, fd);
			break;
		}
	so_value_free(request);

	if (commands[i].name == NULL) {
		struct so_value * response = so_value_new_boolean(true);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	}
}


static void sodr_socket_command_check_header(struct sodr_peer * peer, struct so_value * request __attribute__((unused)), int fd) {
	if (sodr_current_peer != peer) {
		struct so_value * response = so_value_pack("{sb}", "returned", -1L);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;
	struct so_media * media = drive->slot->media;

	const char * media_name = NULL;
	if (media != NULL)
		media_name = media->name;

	so_log_write(so_log_level_notice, dgettext("libstoriqone-drive", "[%s %s #%u]: checking header of media (%s)"), drive->vendor, drive->model, drive->index, media_name);

	bool ok = drive->ops->check_header(sodr_db);
	struct so_value * response = so_value_pack("{sb}", "returned", ok);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

static void sodr_socket_command_check_support(struct sodr_peer * peer __attribute__((unused)), struct so_value * request, int fd) {
	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	struct so_value * media_format = NULL;
	bool for_writing = false;

	so_value_unpack(request, "{s{sosb}}", "params", "format", &media_format, "for writing", &for_writing);

	struct so_media_format * format = malloc(sizeof(struct so_media_format));
	bzero(format, sizeof(struct so_media_format));
	so_media_format_sync(format, media_format);

	bool ok = drive->ops->check_support(format, for_writing, NULL);

	so_media_format_free(format);

	struct so_value * returned = so_value_pack("{sb}", "returned", ok);
	so_json_encode_to_fd(returned, fd, true);
	so_value_free(returned);
}

static void sodr_socket_command_find_best_block_size(struct sodr_peer * peer, struct so_value * request __attribute__((unused)), int fd) {
	if (sodr_current_peer != peer) {
		struct so_value * response = so_value_pack("{sb}", "returned", -1L);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;
	struct so_media * media = drive->slot->media;

	const char * media_name = NULL;
	if (media != NULL)
		media_name = media->name;

	so_log_write(so_log_level_notice, dgettext("libstoriqone-drive", "[%s %s #%u]: looking for best block size for media (%s)"), drive->vendor, drive->model, drive->index, media_name);

	ssize_t block_size = drive->ops->find_best_block_size(sodr_db);
	struct so_value * response = so_value_pack("{si}", "returned", block_size);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);

	if (block_size > 0) {
		char buf[8];
		so_file_convert_size_to_string(block_size, buf, 8);

		so_log_write(so_log_level_notice, dgettext("libstoriqone-drive", "[%s %s #%u]: block size found for media (%s): %s"), drive->vendor, drive->model, drive->index, media_name, buf);
	} else
		so_log_write(so_log_level_error, dgettext("libstoriqone-drive", "[%s %s #%u]: failed to find best block size (%s)"), drive->vendor, drive->model, drive->index, media_name);
}

static void sodr_socket_command_format_media(struct sodr_peer * peer, struct so_value * request, int fd) {
	if (sodr_current_peer == peer) {
		struct so_value * vpool = NULL;
		so_value_unpack(request, "{s{so}}", "params", "pool", &vpool);

		struct so_pool * pool = malloc(sizeof(struct so_pool));
		bzero(pool, sizeof(struct so_pool));
		so_pool_sync(pool, vpool);
		so_value_free(vpool);

		struct so_drive_driver * driver = sodr_drive_get();
		struct so_drive * drive = driver->device;
		struct so_media * media = drive->slot->media;

		const char * media_name = NULL;
		if (media != NULL)
			media_name = media->name;

		so_log_write(so_log_level_notice, dgettext("libstoriqone-drive", "[%s %s #%u]: formatting media (%s)"), drive->vendor, drive->model, drive->index, media_name);

		long int failed = drive->ops->format_media(pool, sodr_db);
		struct so_value * response = so_value_pack("{si}", "returned", failed);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		so_pool_free(pool);

		if (failed == 0)
			so_log_write(so_log_level_notice, dgettext("libstoriqone-drive", "[%s %s #%u]: media (%s) formatted with success"), drive->vendor, drive->model, drive->index, media_name);
		else
			so_log_write(so_log_level_error, dgettext("libstoriqone-drive", "[%s %s #%u]: failed to format media (%s)"), drive->vendor, drive->model, drive->index, media_name);
	} else {
		struct so_value * response = so_value_pack("{si}", "returned", -1L);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	}
}

static void sodr_socket_command_get_raw_reader(struct sodr_peer * peer, struct so_value * request, int fd) {
	long int position = -1;
	char * cookie = NULL;
	so_value_unpack(request, "{siss}", "file position", &position, "cookie", &cookie);

	if (cookie == NULL || peer->cookie == NULL || strcmp(cookie, peer->cookie) != 0) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	free(cookie);

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	if (drive->slot->media == NULL) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_media * media = drive->slot->media;

	const char * media_name = NULL;
	if (media != NULL)
		media_name = media->name;

	so_log_write(so_log_level_notice, dgettext("libstoriqone-drive", "[%s %s #%u]: open media (%s) for reading at position #%ld"), drive->vendor, drive->model, drive->index, media_name, position);

	struct so_stream_reader * reader = drive->ops->get_raw_reader(position, sodr_db);

	struct so_value * tmp_config = so_value_copy(sodr_config, true);
	int tmp_socket = so_socket_server_temp(tmp_config);

	struct so_value * response = so_value_pack("{sbsOsi}",
		"status", true,
		"socket", tmp_config,
		"block size", reader->ops->get_block_size(reader)
	);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);

	int data_socket = so_socket_accept_and_close(tmp_socket, tmp_config);
	so_value_free(tmp_config);

	ssize_t buffer_size = reader->ops->get_block_size(reader);
	char * buffer = malloc(buffer_size);

	struct so_value * command = so_json_parse_fd(fd, -1);

	bool stop = true;
	so_value_unpack(command, "{sbsi}", "stop", &stop);
	so_value_free(command);

	long int l_errno = 0;
	while (!stop) {
		char * str_command = NULL;
		so_value_unpack(command, "{ss}", "command", &str_command);

		if (!strcmp(str_command, "forward")) {
			off_t offset = 0;
			so_value_unpack(command, "{si}", "offset", &offset);
			so_value_free(command);

			ssize_t new_position = reader->ops->forward(reader, offset);

			response = so_value_pack("{sbsisisb}",
				"status", true,
				"new position", new_position,
				"errno", l_errno,
				"close", false
			);
			so_json_encode_to_fd(response, fd, true);
			so_value_free(response);
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

			response = so_value_pack("{sbsisisb}",
				"status", true,
				"nb read", nb_total_read,
				"errno", l_errno,
				"close", false
			);

			so_json_encode_to_fd(response, fd, true);
			so_value_free(response);
		}

		command = so_json_parse_fd(fd, -1);

		if (command != NULL)
			so_value_unpack(command, "{sbsi}", "stop", &stop);
		else
			break;
		so_value_free(command);
	}

	so_log_write(so_log_level_notice, dgettext("libstoriqone-drive", "[%s %s #%u]: close media (%s)"), drive->vendor, drive->model, drive->index, media_name);

	free(buffer);
	close(data_socket);

	response = so_value_pack("{sbsb}",
		"status", true,
		"close", true
	);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

static void sodr_socket_command_get_raw_writer(struct sodr_peer * peer, struct so_value * request, int fd) {
	char * cookie = NULL;
	so_value_unpack(request, "{ss}", "cookie", &cookie);

	if (cookie == NULL || peer->cookie == NULL || strcmp(cookie, peer->cookie) != 0) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	free(cookie);

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	if (drive->slot->media == NULL) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_media * media = drive->slot->media;

	const char * media_name = NULL;
	if (media != NULL)
		media_name = media->name;

	so_log_write(so_log_level_notice, dgettext("libstoriqone-drive", "[%s %s #%u]: open media (%s) for writing"), drive->vendor, drive->model, drive->index, media_name);

	struct so_stream_writer * writer = drive->ops->get_raw_writer(sodr_db);

	struct so_value * tmp_config = so_value_copy(sodr_config, true);
	int tmp_socket = so_socket_server_temp(tmp_config);

	struct so_value * response = so_value_pack("{sbsOsi}",
		"status", true,
		"socket", tmp_config,
		"block size", writer->ops->get_block_size(writer)
	);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);

	int data_socket = so_socket_accept_and_close(tmp_socket, tmp_config);
	so_value_free(tmp_config);

	ssize_t buffer_size = writer->ops->get_block_size(writer);
	char * buffer = malloc(buffer_size);

	struct so_value * command = so_json_parse_fd(fd, -1);

	long int length = -1;
	bool stop = true;
	so_value_unpack(command, "{sbsi}", "stop", &stop, "length", &length);
	so_value_free(command);

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

		response = so_value_pack("{sbsisisb}",
			"status", true,
			"nb write", nb_total_write,
			"errno", l_errno,
			"close", false
		);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);

		command = so_json_parse_fd(fd, -1);

		if (command != NULL)
			so_value_unpack(command, "{sbsi}", "stop", &stop, "length", &length);
		else
			break;
		so_value_free(command);
	}

	so_log_write(so_log_level_notice, dgettext("libstoriqone-drive", "[%s %s #%u]: close media (%s)"), drive->vendor, drive->model, drive->index, media_name);

	free(buffer);
	close(data_socket);

	response = so_value_pack("{sbsb}",
		"status", true,
		"close", false
	);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

static void sodr_socket_command_lock(struct sodr_peer * peer, struct so_value * request __attribute__((unused)), int fd) {
	if (sodr_current_peer == NULL) {
		sodr_current_peer = peer;

		free(peer->cookie);
		peer->cookie = so_checksum_gen_salt(NULL, 32);

		struct so_value * returned = so_value_pack("{s{sbss}}", "returned", "locked", true, "cookie", peer->cookie);
		so_json_encode_to_fd(returned, fd, true);
		so_value_free(returned);
	} else {
		struct so_value * returned = so_value_pack("{s{sbsn}}", "returned", "locked", false, "cookie");
		so_json_encode_to_fd(returned, fd, true);
		so_value_free(returned);
	}
}

static void sodr_socket_command_release(struct sodr_peer * peer, struct so_value * request __attribute__((unused)), int fd) {
	bool ok = sodr_current_peer == peer;

	if (ok)
		sodr_current_peer = NULL;

	free(peer->cookie);
	peer->cookie = NULL;

	struct so_value * response = so_value_pack("{sb}", "status", ok);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

static void sodr_socket_command_sync(struct sodr_peer * peer __attribute__((unused)), struct so_value * request __attribute__((unused)), int fd) {
	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * dr = driver->device;

	struct so_value * response = so_value_pack("{so}", "returned", so_drive_convert(dr, true));
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

