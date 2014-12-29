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

// errno
#include <errno.h>
// dgettext
#include <libintl.h>
// free
#include <stdlib.h>
// strcmp, strdup
#include <string.h>
// bzero, strdup
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
static char * sodr_current_key = NULL;

static void sodr_socket_accept(int fd_server, int fd_client, struct so_value * client);
static void sodr_socket_message(int fd, short event, void * data);

static void sodr_socket_command_check_header(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_check_support(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_find_best_block_size(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_format_media(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_get_raw_reader(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_get_raw_writer(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_sync(struct sodr_peer * peer, struct so_value * request, int fd);

static void sodr_socket_command_reader_close(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_reader_end_of_file(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_reader_forward(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_reader_read(struct sodr_peer * peer, struct so_value * request, int fd);

static void sodr_socket_command_writer_before_close(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_writer_close(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_writer_reopen(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_writer_write(struct sodr_peer * peer, struct so_value * request, int fd);

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
	{ 0, "sync",                 sodr_socket_command_sync },

	{ 0, "reader: close",       sodr_socket_command_reader_close },
	{ 0, "reader: end of file", sodr_socket_command_reader_end_of_file },
	{ 0, "reader: forward",     sodr_socket_command_reader_forward },
	{ 0, "reader: read",        sodr_socket_command_reader_read },

	{ 0, "writer: before close", sodr_socket_command_writer_before_close },
	{ 0, "writer: close",        sodr_socket_command_writer_close },
	{ 0, "writer: reopen",       sodr_socket_command_writer_reopen },
	{ 0, "writer: write",        sodr_socket_command_writer_write },

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

void sodr_listen_set_peer_key(const char * key) {
	free(sodr_current_key);
	sodr_current_key = strdup(key);
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

static void sodr_socket_command_find_best_block_size(struct sodr_peer * peer __attribute__((unused)), struct so_value * request, int fd) {
	char * job_key = NULL;
	so_value_unpack(request, "{s{ss}}", "params", "job key", &job_key);

	if (strcmp(sodr_current_key, job_key) != 0) {
		struct so_value * response = so_value_pack("{sb}", "returned", -1L);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		free(job_key);
		return;
	}

	free(job_key);

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

static void sodr_socket_command_format_media(struct sodr_peer * peer __attribute__((unused)), struct so_value * request, int fd) {
	char * job_key = NULL;
	so_value_unpack(request, "{s{ss}}", "params", "job key", &job_key);

	if (!strcmp(sodr_current_key, job_key)) {
		struct so_value * vpool = NULL;
		so_value_unpack(request, "{s{so}}", "params", "pool", &vpool);

		struct so_pool * pool = malloc(sizeof(struct so_pool));
		bzero(pool, sizeof(struct so_pool));
		so_pool_sync(pool, vpool);

		struct so_drive_driver * driver = sodr_drive_get();
		struct so_drive * drive = driver->device;
		struct so_media * media = drive->slot->media;

		char * media_name = NULL;
		if (media != NULL)
			media_name = strdup(media->name);

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

		free(media_name);
	} else {
		struct so_value * response = so_value_pack("{si}", "returned", -1L);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	}

	free(job_key);
}

static void sodr_socket_command_get_raw_reader(struct sodr_peer * peer, struct so_value * request, int fd) {
	long int position = -1;
	char * job_key = NULL;
	so_value_unpack(request, "{s{sssi}}", "params", "job key", &job_key, "file position", &position);

	if (job_key == NULL || sodr_current_key == NULL || strcmp(job_key, sodr_current_key) != 0) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	free(job_key);

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

	peer->reader = drive->ops->get_raw_reader(position, sodr_db);

	peer->buffer_length = peer->reader->ops->get_block_size(peer->reader);
	peer->buffer = malloc(peer->buffer_length);

	struct so_value * tmp_config = so_value_copy(sodr_config, true);
	int tmp_socket = so_socket_server_temp(tmp_config);

	struct so_value * response = so_value_pack("{sbsOsi}",
		"status", true,
		"socket", tmp_config,
		"block size", peer->buffer_length
	);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);

	peer->fd_data = so_socket_accept_and_close(tmp_socket, tmp_config);
	so_value_free(tmp_config);
}

static void sodr_socket_command_get_raw_writer(struct sodr_peer * peer, struct so_value * request, int fd) {
	char * job_key = NULL;
	so_value_unpack(request, "{s{ss}}", "params", "job key", &job_key);

	if (job_key == NULL || sodr_current_key == NULL || strcmp(job_key, sodr_current_key) != 0) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	free(job_key);

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

	peer->writer = drive->ops->get_raw_writer(sodr_db);

	peer->buffer_length = peer->writer->ops->get_block_size(peer->writer);
	peer->buffer = malloc(peer->buffer_length);

	long file_position = peer->writer->ops->file_position(peer->writer);

	struct so_value * tmp_config = so_value_copy(sodr_config, true);
	int tmp_socket = so_socket_server_temp(tmp_config);

	struct so_value * response = so_value_pack("{sbsOsisi}",
		"status", true,
		"socket", tmp_config,
		"block size", peer->buffer_length,
		"file position", file_position
	);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);

	peer->fd_data = so_socket_accept_and_close(tmp_socket, tmp_config);
	so_value_free(tmp_config);
}

static void sodr_socket_command_sync(struct sodr_peer * peer __attribute__((unused)), struct so_value * request __attribute__((unused)), int fd) {
	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * dr = driver->device;

	struct so_value * response = so_value_pack("{so}", "returned", so_drive_convert(dr, true));
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}


static void sodr_socket_command_reader_close(struct sodr_peer * peer, struct so_value * request __attribute__((unused)), int fd) {
	if (peer->reader == NULL) {
		struct so_value * response = so_value_pack("{si}", "returned", -1L);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	} else {
		long int failed = peer->reader->ops->close(peer->reader);
		long int last_errno = peer->reader->ops->last_errno(peer->reader);

		if (failed == 0) {
			peer->reader->ops->free(peer->reader);
			peer->reader = NULL;
			close(peer->fd_data);
			peer->fd_data = -1;
			free(peer->buffer);
			peer->buffer = NULL;
			peer->buffer_length = 0;
		}

		struct so_value * response = so_value_pack("{sisi}", "returned", failed, "last errno", last_errno);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	}
}

static void sodr_socket_command_reader_end_of_file(struct sodr_peer * peer, struct so_value * request __attribute__((unused)), int fd) {
	if (peer->reader == NULL) {
		struct so_value * response = so_value_pack("{sb}", "returned", true);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	} else {
		bool eof = peer->reader->ops->end_of_file(peer->reader);
		long int last_errno = peer->reader->ops->last_errno(peer->reader);

		struct so_value * response = so_value_pack("{sbsi}", "returned", eof, "last errno", last_errno);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	}
}

static void sodr_socket_command_reader_forward(struct sodr_peer * peer, struct so_value * request, int fd) {
	if (peer->reader == NULL) {
		struct so_value * response = so_value_pack("{si}", "returned", -1);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	} else {
		off_t next_position = 0;
		so_value_unpack(request, "{s{sosb}}", "params", "offset", &next_position);

		off_t new_position = peer->reader->ops->forward(peer->reader, next_position);
		long int last_errno = peer->reader->ops->last_errno(peer->reader);

		struct so_value * response = so_value_pack("{sisi}", "returned", new_position, "last errno", last_errno);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	}
}

static void sodr_socket_command_reader_read(struct sodr_peer * peer, struct so_value * request, int fd) {
	if (peer->reader == NULL) {
		struct so_value * response = so_value_pack("{si}", "returned", -1L);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	} else {
		ssize_t length = 0;
		so_value_unpack(request, "{s{si}}", "params", "length", &length);

		ssize_t nb_total_read = 0;
		while (nb_total_read < length) {
			ssize_t nb_will_read = length - nb_total_read;
			if (nb_will_read > peer->buffer_length)
				nb_will_read = peer->buffer_length;

			ssize_t nb_read = peer->reader->ops->read(peer->reader, peer->buffer, nb_will_read);
			if (nb_read < 0) {
				long int last_errno = peer->reader->ops->last_errno(peer->reader);
				struct so_value * response = so_value_pack("{sisi}", "returned", -1L, "last errno", (long) last_errno);
				so_json_encode_to_fd(response, fd, true);
				so_value_free(response);
				return;
			}

			nb_total_read += nb_read;

			send(peer->fd_data, peer->buffer, nb_read, 0);
		}

		struct so_value * response = so_value_pack("{sisi}", "returned", nb_total_read, "last errno", 0L);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	}
}


static void sodr_socket_command_writer_before_close(struct sodr_peer * peer, struct so_value * request, int fd) {
	if (peer->writer == NULL) {
		struct so_value * response = so_value_pack("{si}", "returned", -1L);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	} else {
		ssize_t length = 0;
		so_value_unpack(request, "{s{si}}", "params", "length", &length);

		if (length > peer->buffer_length)
			length = peer->buffer_length;

		ssize_t nb_read = peer->writer->ops->before_close(peer->writer, peer->buffer, length);
		long int last_errno = peer->writer->ops->last_errno(peer->writer);

		struct so_value * response = so_value_pack("{sisi}", "returned", nb_read, "last errno", last_errno);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);

		if (nb_read > 0)
			send(peer->fd_data, peer->buffer, nb_read, MSG_NOSIGNAL);
	}
}

static void sodr_socket_command_writer_close(struct sodr_peer * peer, struct so_value * request __attribute__((unused)), int fd) {
	if (peer->writer == NULL) {
		struct so_value * response = so_value_pack("{si}", "returned", -1L);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	} else {
		long int failed = peer->writer->ops->close(peer->writer);
		long int last_errno = peer->writer->ops->last_errno(peer->writer);

		if (failed == 0) {
			peer->writer->ops->free(peer->writer);
			peer->writer = NULL;
			close(peer->fd_data);
			peer->fd_data = -1;
			free(peer->buffer);
			peer->buffer = NULL;
			peer->buffer_length = 0;
		}

		struct so_value * response = so_value_pack("{sisi}", "returned", failed, "last errno", last_errno);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	}
}

static void sodr_socket_command_writer_reopen(struct sodr_peer * peer, struct so_value * request __attribute__((unused)), int fd) {
	if (peer->writer == NULL) {
		struct so_value * response = so_value_pack("{sb}", "returned", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	} else {
		peer->reader = peer->writer->ops->reopen(peer->writer);
		long int last_errno = 0;
		if (peer->reader == NULL)
			last_errno = peer->writer->ops->last_errno(peer->writer);
		else {
			peer->writer->ops->free(peer->writer);
			peer->writer = NULL;
		}

		struct so_value * response = so_value_pack("{sbsi}", "returned", peer->reader != NULL, "last errno", last_errno);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	}
}

static void sodr_socket_command_writer_write(struct sodr_peer * peer, struct so_value * request, int fd) {
	if (peer->writer == NULL) {
		struct so_value * response = so_value_pack("{si}", "returned", -1L);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	} else {
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
				so_json_encode_to_fd(response, fd, true);
				so_value_free(response);
				return;
			}

			ssize_t nb_write = peer->writer->ops->write(peer->writer, peer->buffer, nb_read);
			if (nb_write < 0) {
				long int last_errno = peer->writer->ops->last_errno(peer->writer);
				struct so_value * response = so_value_pack("{sisi}", "returned", -1L, "last errno", last_errno);
				so_json_encode_to_fd(response, fd, true);
				so_value_free(response);
				return;
			}

			nb_total_write += nb_write;
		}

		struct so_value * response = so_value_pack("{sisi}", "returned", nb_total_write, "last errno", 0L);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	}
}

