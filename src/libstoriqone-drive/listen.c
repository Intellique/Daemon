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

#define _GNU_SOURCE
// errno
#include <errno.h>
// dgettext
#include <libintl.h>
// free
#include <stdlib.h>
// asprintf
#include <stdio.h>
// strcmp, strdup
#include <string.h>
// bzero
#include <strings.h>
// send
#include <sys/socket.h>
// send
#include <sys/types.h>
// clock_gettime
#include <time.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/checksum.h>
#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/format.h>
#include <libstoriqone/io.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/string.h>
#include <libstoriqone/thread_pool.h>
#include <libstoriqone/value.h>
#include <libstoriqone-drive/log.h>

#include "drive.h"
#include "io/io.h"
#include "listen.h"
#include "peer.h"

static struct so_database_connection * sodr_db = NULL;
static struct so_value * sodr_config = NULL;
static unsigned int sodr_nb_clients = 0;
static struct sodr_peer * sodr_current_peer = NULL;
static struct sodr_peer * first_peer = NULL, * last_peer = NULL;
static char * sodr_current_id = NULL;

static void sodr_socket_accept(int fd_server, int fd_client, struct so_value * client);
static void sodr_socket_message(int fd, short event, void * data);

static void sodr_socket_command_check_header(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_check_support(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_create_archive_volume(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_erase_media(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_format_media(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_get_raw_reader(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_get_raw_writer(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_get_writer(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_init_peer(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_open_archive_volume(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_release_drive(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_scan_media(struct sodr_peer * peer, struct so_value * request, int fd);
static void sodr_socket_command_sync(struct sodr_peer * peer, struct so_value * request, int fd);

static void sodr_worker_command_create_archive_volume(void * params);
static void sodr_worker_command_erase_media(void * peer);
static void sodr_worker_command_format_media(void * params);
static void sodr_worker_command_get_raw_reader(void * params);
static void sodr_worker_command_get_raw_writer(void * peer);
static void sodr_worker_command_open_archive_volume(void * peer);

static struct sodr_socket_command {
	unsigned long hash;
	char * name;
	void (*function)(struct sodr_peer * peer, struct so_value * request, int fd);
} commands[] = {
	{ 0, "check header",          sodr_socket_command_check_header },
	{ 0, "check support",         sodr_socket_command_check_support },
	{ 0, "create archive volume", sodr_socket_command_create_archive_volume },
	{ 0, "erase media",           sodr_socket_command_erase_media },
	{ 0, "format media",          sodr_socket_command_format_media },
	{ 0, "get raw reader",        sodr_socket_command_get_raw_reader },
	{ 0, "get raw writer",        sodr_socket_command_get_raw_writer },
	{ 0, "get writer",            sodr_socket_command_get_writer },
	{ 0, "init peer",             sodr_socket_command_init_peer },
	{ 0, "open archive volume",   sodr_socket_command_open_archive_volume },
	{ 0, "release drive",         sodr_socket_command_release_drive },
	{ 0, "scan media",            sodr_socket_command_scan_media },
	{ 0, "sync",                  sodr_socket_command_sync },

	{ 0, NULL, NULL }
};

struct sodr_socket_params_create_archive_volume {
	struct sodr_peer * peer;
	struct so_value * volume;
	struct so_value * checksums;
};

struct sodr_socket_params_erase_media {
	struct sodr_peer * peer;
	bool quick_mode;
};

struct sodr_socket_params_format_media {
	struct sodr_peer * peer;
	struct so_pool * pool;
	struct so_value * option;
};

struct sodr_socket_params_parse_archive {
	struct sodr_peer * peer;
	unsigned int archive_position;
	struct so_value * checksums;
};

struct sodr_socket_params_get_raw_reader {
	struct sodr_peer * peer;
	int position;
};

struct sodr_socket_params_open_archive_volume {
	struct sodr_peer * peer;
	struct so_value * volume;
	struct so_value * checksums;
};


void sodr_listen_configure(struct so_value * config) {
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		commands[i].hash = so_string_compute_hash2(commands[i].name);

	if (sodr_config != NULL)
		so_value_free(sodr_config);
	sodr_config = so_value_copy(config, true);

	so_socket_from_template(sodr_config, sodr_socket_accept);
}

struct so_value * sodr_listen_get_socket_config() {
	return sodr_config;
}

bool sodr_listen_is_locked() {
	return sodr_current_peer != NULL;
}

unsigned int sodr_listen_nb_clients() {
	return sodr_nb_clients;
}

void sodr_listen_remove_peer(struct sodr_peer * peer) {
	if (!peer->disconnected || peer->owned)
		return;

	sodr_nb_clients--;

	if (peer == sodr_current_peer)
		sodr_listen_reset_peer();

	struct sodr_peer * previous = peer->previous;
	struct sodr_peer * next = peer->next;
	if (peer == first_peer)
		first_peer = next;
	if (peer == last_peer)
		last_peer = previous;
	if (next != NULL)
		next->previous = previous;
	if (previous != NULL)
		previous->next = next;

	sodr_peer_free(peer);
}

void sodr_listen_reset_peer() {
	free(sodr_current_id);
	sodr_current_id = NULL;
	sodr_current_peer = NULL;
}

void sodr_listen_set_db_connection(struct so_database_connection * db) {
	sodr_db = db;
}

void sodr_listen_set_peer_id(const char * id) {
	free(sodr_current_id);
	sodr_current_id = strdup(id);

	struct sodr_peer * peer;
	for (peer = first_peer; peer != NULL; peer = peer->next)
		if (strcmp(id, peer->job_id) == 0) {
			sodr_current_peer = peer;
			break;
		}
}


static void sodr_socket_accept(int fd_server __attribute__((unused)), int fd_client, struct so_value * client __attribute__((unused))) {
	struct sodr_peer * peer = sodr_peer_new(fd_client, last_peer);
	if (first_peer == NULL)
		first_peer = last_peer = peer;
	else
		last_peer = peer;

	so_poll_register(fd_client, POLLIN | POLLHUP, sodr_socket_message, peer, NULL);
	sodr_nb_clients++;
}

static void sodr_socket_message(int fd, short event, void * data) {
	struct sodr_peer * peer = data;

	if (event & POLLHUP) {
		peer->disconnected = true;
		sodr_listen_remove_peer(peer);

		return;
	}

	struct so_value * request = so_json_parse_fd(fd, -1);
	const char * command = NULL;
	if (request == NULL || so_value_unpack(request, "{sS}", "command", &command) < 0) {
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


static void sodr_socket_command_check_header(struct sodr_peer * peer, struct so_value * request, int fd) {
	const char * job_id = NULL;
	so_value_unpack(request, "{s{sS}}", "params", "job id", &job_id);

	if (job_id == NULL || strcmp(sodr_current_id, job_id) != 0) {
		struct so_value * response = so_value_pack("{sb}", "returned", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	sodr_peer_set(peer);

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;
	struct so_media * media = drive->slot->media;

	const char * media_name = so_media_get_name(media);

	sodr_log_add_record(so_job_status_running, sodr_db, so_log_level_notice, so_job_record_notif_normal,
		dgettext("libstoriqone-drive", "[%s %s #%u]: checking header of media '%s'"),
		drive->vendor, drive->model, drive->index, media_name);

	bool ok = drive->ops->check_header(sodr_db);
	struct so_value * response = so_value_pack("{sb}", "returned", ok);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);

	sodr_peer_set(NULL);
}

static void sodr_socket_command_check_support(struct sodr_peer * peer, struct so_value * request, int fd) {
	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	sodr_peer_set(peer);

	struct so_value * media_format = NULL;
	bool for_writing = false;

	so_value_unpack(request, "{s{sosb}}",
		"params",
			"format", &media_format,
			"for writing", &for_writing
	);

	struct so_media_format * format = malloc(sizeof(struct so_media_format));
	bzero(format, sizeof(struct so_media_format));
	so_media_format_sync(format, media_format);

	bool ok = drive->ops->check_support(format, for_writing, NULL);

	so_media_format_free(format);

	struct so_value * returned = so_value_pack("{sb}", "returned", ok);
	so_json_encode_to_fd(returned, fd, true);
	so_value_free(returned);
}

static void sodr_socket_command_create_archive_volume(struct sodr_peer * peer, struct so_value * request, int fd) {
	const char * job_id = NULL;
	struct so_value * checksums = NULL;
	so_value_unpack(request, "{s{sSso}}",
		"params",
			"job id", &job_id,
			"checksums", &checksums
	);

	if (job_id == NULL || sodr_current_id == NULL || strcmp(job_id, sodr_current_id) != 0) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	if (drive->slot->media == NULL) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	sodr_peer_set(peer);

	struct sodr_socket_params_create_archive_volume * params = malloc(sizeof(struct sodr_socket_params_create_archive_volume));
	bzero(params, sizeof(struct sodr_socket_params_create_archive_volume));
	params->peer = peer;
	params->checksums = so_value_share(checksums);

	so_value_unpack(request, "{s{sO}}",
		"params",
			"archive volume", &params->volume
	);

	peer->owned = true;
	so_thread_pool_run("format writer", sodr_worker_command_create_archive_volume, params);
}

static void sodr_socket_command_erase_media(struct sodr_peer * peer, struct so_value * request, int fd) {
	const char * job_id = NULL;
	so_value_unpack(request, "{s{sS}}", "params", "job id", &job_id);

	if (job_id == NULL || strcmp(sodr_current_id, job_id) != 0) {
		struct so_value * response = so_value_pack("{si}", "returned", -1);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	sodr_peer_set(peer);

	struct sodr_socket_params_erase_media * params = malloc(sizeof(struct sodr_socket_params_erase_media));
	bzero(params, sizeof(struct sodr_socket_params_erase_media));
	params->peer = peer;
	so_value_unpack(request, "{s{sb}}",
		"params",
			"quick mode", &params->quick_mode
	);

	peer->owned = true;
	so_thread_pool_run("erase media", sodr_worker_command_erase_media, params);
}

static void sodr_socket_command_format_media(struct sodr_peer * peer, struct so_value * request, int fd) {
	const char * job_id = NULL;
	so_value_unpack(request, "{s{sS}}", "params", "job id", &job_id);

	if (job_id == NULL || strcmp(sodr_current_id, job_id) != 0) {
		struct so_value * response = so_value_pack("{si}", "returned", -1);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	sodr_peer_set(peer);

	struct so_value * vpool = NULL;
	struct so_value * option = NULL;
	so_value_unpack(request, "{s{sosO}}",
		"params",
			"pool", &vpool,
			"option", &option
	);

	struct sodr_socket_params_format_media * params = malloc(sizeof(struct sodr_socket_params_format_media));
	bzero(params, sizeof(struct sodr_socket_params_format_media));

	params->peer = peer;
	params->pool = malloc(sizeof(struct so_pool));
	bzero(params->pool, sizeof(struct so_pool));
	so_pool_sync(params->pool, vpool);
	params->option = option;

	peer->owned = true;
	so_thread_pool_run("format media", sodr_worker_command_format_media, params);
}

static void sodr_socket_command_get_raw_reader(struct sodr_peer * peer, struct so_value * request, int fd) {
	int position = -1;
	const char * job_id = NULL;
	so_value_unpack(request, "{s{sSsi}}",
		"params",
			"job id", &job_id,
			"file position", &position
	);

	if (job_id == NULL || sodr_current_id == NULL || strcmp(job_id, sodr_current_id) != 0) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	sodr_peer_set(peer);

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	if (drive->slot->media == NULL) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct sodr_socket_params_get_raw_reader * params = malloc(sizeof(struct sodr_socket_params_get_raw_reader));
	bzero(params, sizeof(struct sodr_socket_params_get_raw_reader));
	params->peer = peer;
	params->position = position;

	peer->owned = true;
	so_thread_pool_run("raw reader", sodr_worker_command_get_raw_reader, params);
}

static void sodr_socket_command_get_raw_writer(struct sodr_peer * peer, struct so_value * request, int fd) {
	const char * job_id = NULL;
	so_value_unpack(request, "{s{sS}}", "params", "job id", &job_id);

	if (job_id == NULL || sodr_current_id == NULL || strcmp(job_id, sodr_current_id) != 0) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	sodr_peer_set(peer);

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	if (drive->slot->media == NULL) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	peer->owned = true;
	so_thread_pool_run("raw writer", sodr_worker_command_get_raw_writer, peer);
}

static void sodr_socket_command_get_writer(struct sodr_peer * peer, struct so_value * request, int fd) {
	const char * job_id = NULL;
	struct so_value * checksums = NULL;
	so_value_unpack(request, "{s{sSso}}",
		"params",
			"job id", &job_id,
			"checksums", &checksums
	);

	if (job_id == NULL || sodr_current_id == NULL || strcmp(job_id, sodr_current_id) != 0) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	sodr_peer_set(peer);

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	if (drive->slot->media == NULL) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_media * media = drive->slot->media;

	const char * media_name = so_media_get_name(media);

	sodr_log_add_record(so_job_status_running, sodr_db, so_log_level_notice, so_job_record_notif_normal,
		dgettext("libstoriqone-drive", "[%s %s #%u]: open media '%s' for writing"),
		drive->vendor, drive->model, drive->index, media_name);

	peer->format_writer = drive->ops->get_writer(checksums, sodr_db);

	if (peer->format_writer == NULL) {
		sodr_log_add_record(so_job_status_running, sodr_db, so_log_level_error, so_job_record_notif_important,
			dgettext("libstoriqone-drive", "[%s %s #%u]: failed to open media '%s' for writing"),
			drive->vendor, drive->model, drive->index, media_name);

		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	peer->buffer_length = 16384;
	peer->buffer = malloc(peer->buffer_length);
	peer->has_checksums = so_value_list_get_length(checksums) > 0;

	struct so_value * socket_cmd_config = so_value_copy(sodr_config, true);
	struct so_value * socket_data_config = so_value_copy(sodr_config, true);

	int cmd_socket = so_socket_server_temp(socket_cmd_config);
	int data_socket = so_socket_server_temp(socket_data_config);

	struct so_value * response = so_value_pack("{sbs{sOsO}szsisz}",
		"status", true,
		"socket",
			"command", socket_cmd_config,
			"data", socket_data_config,
		"block size", peer->format_writer->ops->get_block_size(peer->format_writer),
		"file position", peer->format_writer->ops->file_position(peer->format_writer),
		"available size", peer->format_writer->ops->get_available_size(peer->format_writer)
	);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);

	peer->fd_cmd = so_socket_accept_and_close(cmd_socket, socket_cmd_config);
	peer->fd_data = so_socket_accept_and_close(data_socket, socket_data_config);

	so_value_free(socket_cmd_config);
	so_value_free(socket_data_config);

	peer->owned = true;
	so_thread_pool_run("format writer", sodr_io_format_writer, peer);
}

static void sodr_socket_command_init_peer(struct sodr_peer * peer, struct so_value * request, int fd) {
	so_value_unpack(request, "{s{s{sssu}}}",
		"params",
			"job",
				"id", &peer->job_id,
				"num run", &peer->job_num_run
	);

	struct so_value * response = so_value_pack("{sb}", "returned", true);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	sodr_peer_set(peer);
	sodr_log_add_record(so_job_status_running, sodr_db, so_log_level_debug, so_job_record_notif_normal,
		dgettext("libstoriqone-drive", "[%s %s #%u]: New connection from job (id: %s, num runs: %u)"),
		drive->vendor, drive->model, drive->index, peer->job_id, peer->job_num_run);
	sodr_peer_set(NULL);
}

static void sodr_socket_command_open_archive_volume(struct sodr_peer * peer, struct so_value * request, int fd) {
	const char * job_id = NULL;
	struct so_value * archive_volume = NULL;
	struct so_value * checksums = NULL;
	so_value_unpack(request, "{s{sSsoso}}",
		"params",
			"job id", &job_id,
			"archive volume", &archive_volume,
			"checksums", &checksums
	);

	if (job_id == NULL || sodr_current_id == NULL || archive_volume == NULL || strcmp(job_id, sodr_current_id) != 0) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	if (drive->slot->media == NULL) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	sodr_peer_set(peer);

	struct sodr_socket_params_open_archive_volume * params = malloc(sizeof(struct sodr_socket_params_open_archive_volume));
	bzero(params, sizeof(struct sodr_socket_params_open_archive_volume));
	params->peer = peer;
	params->volume = so_value_share(archive_volume);
	params->checksums = so_value_share(checksums);

	peer->owned = true;
	so_thread_pool_run("format reader", sodr_worker_command_open_archive_volume, params);
}

static void sodr_socket_command_release_drive(struct sodr_peer * peer, struct so_value * request __attribute__((unused)), int fd) {
	const bool ok = sodr_current_peer == peer;
	if (sodr_current_peer == peer)
		sodr_current_peer = NULL;

	struct so_value * response = so_value_pack("{sb}", "returned", ok);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

static void sodr_socket_command_scan_media(struct sodr_peer * peer, struct so_value * request, int fd) {
	const char * job_id = NULL;
	so_value_unpack(request, "{s{sS}}", "params", "job key", &job_id);

	if (job_id == NULL || sodr_current_id == NULL || strcmp(job_id, sodr_current_id) != 0) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	struct so_media * media = drive->slot->media;

	const char * media_name = so_media_get_name(media);

	so_log_write(so_log_level_notice,
		dgettext("libstoriqone-drive", "[%s %s #%u]: open media '%s' for importing archives"),
		drive->vendor, drive->model, drive->index, media_name);

	struct so_value * socket_cmd_config = so_value_copy(sodr_config, true);
	int cmd_socket = so_socket_server_temp(socket_cmd_config);

	struct so_value * response = so_value_pack("{sbs{sO}}",
		"status", true,
		"socket",
			"command", socket_cmd_config
	);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);

	peer->fd_cmd = so_socket_accept_and_close(cmd_socket, socket_cmd_config);

	so_value_free(socket_cmd_config);

	peer->db_connection = sodr_db->config->ops->connect(sodr_db->config);

	peer->owned = true;
	so_thread_pool_run("import media", sodr_io_import_media, peer);
}

static void sodr_socket_command_sync(struct sodr_peer * peer __attribute__((unused)), struct so_value * request __attribute__((unused)), int fd) {
	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * dr = driver->device;

	struct so_value * response = so_value_pack("{so}", "returned", so_drive_convert(dr, true));
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}


static void sodr_worker_command_create_archive_volume(void * data) {
	struct sodr_socket_params_create_archive_volume * params = data;

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;
	struct so_media * media = drive->slot->media;
	const char * media_name = so_media_get_name(media);

	struct so_database_connection * db_connect = sodr_db->config->ops->connect(sodr_db->config);

	struct so_archive_volume vol;
	bzero(&vol, sizeof(vol));
	so_archive_volume_sync(&vol, params->volume);

	sodr_log_add_record(so_job_status_running, db_connect, so_log_level_notice, so_job_record_notif_normal,
		dgettext("libstoriqone-drive", "[%s %s #%u]: open media '%s' for writing"),
		drive->vendor, drive->model, drive->index, media_name);

	params->peer->format_writer = drive->ops->create_archive_volume(&vol, params->checksums, db_connect);

	if (params->peer->format_writer == NULL) {
		sodr_log_add_record(so_job_status_running, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("libstoriqone-drive", "[%s %s #%u]: failed to open media '%s' for creating new archive volume"),
			drive->vendor, drive->model, drive->index, media_name);

		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, params->peer->fd, true);
		so_value_free(response);

		params->peer->owned = false;
		sodr_peer_set(NULL);

		vol.media = NULL;
		so_archive_volume_free(&vol);
		so_value_free(params->checksums);
		free(params);
		db_connect->ops->free(db_connect);

		return;
	}

	params->peer->buffer_length = 65536;
	params->peer->buffer = malloc(params->peer->buffer_length);
	params->peer->has_checksums = so_value_list_get_length(params->checksums) > 0;

	struct so_value * socket_cmd_config = so_value_copy(sodr_config, true);
	struct so_value * socket_data_config = so_value_copy(sodr_config, true);

	int cmd_socket = so_socket_server_temp(socket_cmd_config);
	int data_socket = so_socket_server_temp(socket_data_config);

	struct so_value * response = so_value_pack("{sbs{sOsO}szsiszso}",
		"status", true,
		"socket",
			"command", socket_cmd_config,
			"data", socket_data_config,
		"block size", params->peer->format_writer->ops->get_block_size(params->peer->format_writer),
		"file position", params->peer->format_writer->ops->file_position(params->peer->format_writer),
		"available size", params->peer->format_writer->ops->get_available_size(params->peer->format_writer),
		"archive volume", so_archive_volume_convert(&vol)
	);
	so_json_encode_to_fd(response, params->peer->fd, true);
	so_value_free(response);

	params->peer->fd_cmd = so_socket_accept_and_close(cmd_socket, socket_cmd_config);
	params->peer->fd_data = so_socket_accept_and_close(data_socket, socket_data_config);

	so_value_free(socket_cmd_config);
	so_value_free(socket_data_config);

	sodr_io_format_writer(params->peer);

	vol.media = NULL;
	so_archive_volume_free(&vol);
	so_value_free(params->checksums);
	free(params);
	db_connect->ops->free(db_connect);
}

static void sodr_worker_command_erase_media(void * arg) {
	struct sodr_socket_params_erase_media * params = arg;

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;
	struct so_media * media = drive->slot->media;

	struct so_database_connection * db_connect = sodr_db->config->ops->connect(sodr_db->config);

	sodr_log_add_record(so_job_status_running, db_connect, so_log_level_notice, so_job_record_notif_normal,
		dgettext("libstoriqone-drive", "[%s %s #%u]: Erasing media '%s' (mode: %s)"),
		drive->vendor, drive->model, drive->index, media->name,
		params->quick_mode ? dgettext("libstoriqone-drive", "quick") : dgettext("libstoriqone-drive", "long"));

	int failed = drive->ops->erase_media(params->quick_mode, db_connect);
	struct so_value * response = so_value_pack("{si}", "returned", failed);
	so_json_encode_to_fd(response, params->peer->fd, true);
	so_value_free(response);

	if (failed == 0)
		sodr_log_add_record(so_job_status_running, db_connect, so_log_level_notice, so_job_record_notif_normal,
			dgettext("libstoriqone-drive", "[%s %s #%u]: media '%s' erased successfully"),
			drive->vendor, drive->model, drive->index, media->name);
	else
		sodr_log_add_record(so_job_status_running, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("libstoriqone-drive", "[%s %s #%u]: failed to erase media '%s'"),
			drive->vendor, drive->model, drive->index, media->name);

	params->peer->owned = false;
	sodr_peer_set(NULL);

	free(params);
	db_connect->ops->free(db_connect);
}

static void sodr_worker_command_format_media(void * data) {
	struct sodr_socket_params_format_media * params = data;

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;
	struct so_media * media = drive->slot->media;

	struct so_database_connection * db_connect = sodr_db->config->ops->connect(sodr_db->config);

	sodr_log_add_record(so_job_status_running, db_connect, so_log_level_notice, so_job_record_notif_normal,
		dgettext("libstoriqone-drive", "[%s %s #%u]: formatting media '%s'"),
		drive->vendor, drive->model, drive->index, media->name);

	int failed = drive->ops->format_media(params->pool, params->option, db_connect);
	struct so_value * response = so_value_pack("{si}", "returned", failed);
	so_json_encode_to_fd(response, params->peer->fd, true);
	so_value_free(response);

	if (failed == 0)
		so_log_write(so_log_level_notice,
			dgettext("libstoriqone-drive", "[%s %s #%u]: media '%s' formatted successfully"),
			drive->vendor, drive->model, drive->index, media->name);
	else
		so_log_write(so_log_level_error,
			dgettext("libstoriqone-drive", "[%s %s #%u]: failed to format media '%s'"),
			drive->vendor, drive->model, drive->index, media->name);

	params->peer->owned = false;
	sodr_peer_set(NULL);

	db_connect->ops->free(db_connect);
	free(params);
}

static void sodr_worker_command_get_raw_reader(void * data) {
	struct sodr_socket_params_get_raw_reader * params = data;

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;
	struct so_media * media = drive->slot->media;
	const char * media_name = so_media_get_name(media);

	struct so_database_connection * db_connect = sodr_db->config->ops->connect(sodr_db->config);

	sodr_log_add_record(so_job_status_running, db_connect, so_log_level_notice, so_job_record_notif_normal,
		dgettext("libstoriqone-drive", "[%s %s #%u]: open media '%s' for reading at position #%d"),
		drive->vendor, drive->model, drive->index, media_name, params->position);

	params->peer->stream_reader = drive->ops->get_raw_reader(params->position, db_connect);

	if (params->peer->stream_reader == NULL) {
		sodr_log_add_record(so_job_status_running, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("libstoriqone-drive", "[%s %s #%u]: failed to open media '%s' for reading at position #%d"),
			drive->vendor, drive->model, drive->index, media_name, params->position);

		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, params->peer->fd, true);
		so_value_free(response);

		params->peer->owned = false;
		sodr_peer_set(NULL);

		return;
	}

	params->peer->buffer_length = 16384;
	params->peer->buffer = malloc(params->peer->buffer_length);

	clock_gettime(CLOCK_MONOTONIC, &params->peer->start_time);

	struct so_value * socket_cmd_config = so_value_copy(sodr_config, true);
	struct so_value * socket_data_config = so_value_copy(sodr_config, true);

	int cmd_socket = so_socket_server_temp(socket_cmd_config);
	int data_socket = so_socket_server_temp(socket_data_config);

	struct so_value * response = so_value_pack("{sbs{sOsO}sz}",
		"status", true,
		"socket",
			"command", socket_cmd_config,
			"data", socket_data_config,
		"block size", params->peer->stream_reader->ops->get_block_size(params->peer->stream_reader)
	);
	so_json_encode_to_fd(response, params->peer->fd, true);
	so_value_free(response);

	params->peer->fd_cmd = so_socket_accept_and_close(cmd_socket, socket_cmd_config);
	params->peer->fd_data = so_socket_accept_and_close(data_socket, socket_data_config);

	so_value_free(socket_cmd_config);
	so_value_free(socket_data_config);

	sodr_io_raw_reader(params->peer);

	params->peer->owned = false;
	sodr_peer_set(NULL);

	db_connect->ops->free(db_connect);
	free(params);
}

static void sodr_worker_command_get_raw_writer(void * arg) {
	struct sodr_peer * peer = arg;

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;
	struct so_media * media = drive->slot->media;

	const char * media_name = so_media_get_name(media);

	struct so_database_connection * db_connect = sodr_db->config->ops->connect(sodr_db->config);

	sodr_log_add_record(so_job_status_running, db_connect, so_log_level_notice, so_job_record_notif_normal,
		dgettext("libstoriqone-drive", "[%s %s #%u]: open media '%s' for writing"),
		drive->vendor, drive->model, drive->index, media_name);

	peer->stream_writer = drive->ops->get_raw_writer(db_connect);

	if (peer->stream_writer == NULL) {
		sodr_log_add_record(so_job_status_running, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("libstoriqone-drive", "[%s %s #%u]: failed to open media '%s' for writing"),
			drive->vendor, drive->model, drive->index, media_name);

		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, peer->fd, true);
		so_value_free(response);

		peer->owned = false;
		sodr_peer_set(NULL);

		return;
	}

	peer->buffer_length = 16384;
	peer->buffer = malloc(peer->buffer_length);

	struct so_value * socket_cmd_config = so_value_copy(sodr_config, true);
	struct so_value * socket_data_config = so_value_copy(sodr_config, true);

	int cmd_socket = so_socket_server_temp(socket_cmd_config);
	int data_socket = so_socket_server_temp(socket_data_config);

	struct so_value * response = so_value_pack("{sbs{sOsO}szsisz}",
		"status", true,
		"socket",
			"command", socket_cmd_config,
			"data", socket_data_config,
		"block size", peer->stream_writer->ops->get_block_size(peer->stream_writer),
		"file position", peer->stream_writer->ops->file_position(peer->stream_writer),
		"available size", peer->stream_writer->ops->get_available_size(peer->stream_writer)
	);
	so_json_encode_to_fd(response, peer->fd, true);
	so_value_free(response);

	peer->fd_cmd = so_socket_accept_and_close(cmd_socket, socket_cmd_config);
	peer->fd_data = so_socket_accept_and_close(data_socket, socket_data_config);

	so_value_free(socket_cmd_config);
	so_value_free(socket_data_config);

	sodr_io_raw_writer(peer);

	peer->owned = false;
	sodr_peer_set(NULL);

	db_connect->ops->free(db_connect);
}

static void sodr_worker_command_open_archive_volume(void * data) {
	struct sodr_socket_params_open_archive_volume * params = data;

	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;
	struct so_media * media = drive->slot->media;
	const char * media_name = so_media_get_name(media);

	struct so_database_connection * db_connect = sodr_db->config->ops->connect(sodr_db->config);

	struct so_archive_volume vol;
	bzero(&vol, sizeof(vol));
	so_archive_volume_sync(&vol, params->volume);

	sodr_log_add_record(so_job_status_running, db_connect, so_log_level_notice, so_job_record_notif_normal,
		dgettext("libstoriqone-drive", "[%s %s #%u]: open media '%s' for reading at position #%d"),
		drive->vendor, drive->model, drive->index, media_name, vol.media_position);

	params->peer->format_reader = drive->ops->open_archive_volume(&vol, params->checksums, db_connect);

	if (params->peer->format_reader == NULL) {
		sodr_log_add_record(so_job_status_running, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("libstoriqone-drive", "[%s %s #%u]: failed to open media '%s' for reading at position #%d"),
			drive->vendor, drive->model, drive->index, media_name, vol.media_position);

		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, params->peer->fd, true);
		so_value_free(response);

		params->peer->owned = false;
		sodr_peer_set(NULL);

		vol.media = NULL;
		so_archive_volume_free(&vol);
		so_value_free(params->checksums);
		free(params);
		db_connect->ops->free(db_connect);

		return;
	}

	params->peer->buffer_length = 65536;
	params->peer->buffer = malloc(params->peer->buffer_length);
	params->peer->has_checksums = so_value_list_get_length(params->checksums) > 0;

	struct so_value * socket_cmd_config = so_value_copy(sodr_config, true);
	struct so_value * socket_data_config = so_value_copy(sodr_config, true);

	int cmd_socket = so_socket_server_temp(socket_cmd_config);
	int data_socket = so_socket_server_temp(socket_data_config);

	struct so_value * response = so_value_pack("{sbs{sOsO}sz}",
		"status", true,
		"socket",
			"command", socket_cmd_config,
			"data", socket_data_config,
		"block size", params->peer->format_reader->ops->get_block_size(params->peer->format_reader)
	);
	so_json_encode_to_fd(response, params->peer->fd, true);
	so_value_free(response);

	params->peer->fd_cmd = so_socket_accept_and_close(cmd_socket, socket_cmd_config);
	params->peer->fd_data = so_socket_accept_and_close(data_socket, socket_data_config);

	so_value_free(socket_cmd_config);
	so_value_free(socket_data_config);

	sodr_io_format_reader(params->peer);

	vol.media = NULL;
	so_archive_volume_free(&vol);
	so_value_free(params->volume);
	so_value_free(params->checksums);
	free(params);
	db_connect->ops->free(db_connect);
}
