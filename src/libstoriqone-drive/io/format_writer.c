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
// dgettext
#include <libintl.h>
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
#include <libstoriqone/log.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/thread_pool.h>
#include <libstoriqone/value.h>

#include "io.h"
#include "../drive.h"
#include "../peer.h"

static void sodr_io_format_writer_add_file(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_add_label(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_close(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_compute_size_of_file(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_end_of_file(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_free(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_get_alternate_path(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_init(void) __attribute__((constructor));
static void sodr_io_format_writer_reopen(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_restart_file(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_write(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_format_writer_write_metadata(struct sodr_peer * peer, struct so_value * request);

static struct sodr_command commands[] = {
	{ 0, "add file",             sodr_io_format_writer_add_file },
	{ 0, "add label",            sodr_io_format_writer_add_label },
	{ 0, "close",                sodr_io_format_writer_close },
	{ 0, "compute size of file", sodr_io_format_writer_compute_size_of_file },
	{ 0, "end of file",          sodr_io_format_writer_end_of_file },
	{ 0, "free",                 sodr_io_format_writer_free },
	{ 0, "get alternate path",   sodr_io_format_writer_get_alternate_path },
	{ 0, "reopen",               sodr_io_format_writer_reopen },
	{ 0, "restart file",         sodr_io_format_writer_restart_file },
	{ 0, "write",                sodr_io_format_writer_write },
	{ 0, "write metadata",       sodr_io_format_writer_write_metadata },

	{ 0, NULL, NULL },
};


void sodr_io_format_writer(void * arg) {
	sodr_io_process(arg, commands);
}

static void sodr_io_format_writer_init() {
	sodr_io_init(commands);
}


static void sodr_io_format_writer_add_file(struct sodr_peer * peer, struct so_value * request) {
	struct so_value * vfile = NULL;
	const char * selected_path = NULL;
	so_value_unpack(request, "{s{sosS}}", "params", "file", &vfile, "selected path", &selected_path);

	struct so_format_file file;
	so_format_file_init(&file);
	so_format_file_sync(&file, vfile);

	ssize_t current_position = peer->format_writer->ops->position(peer->format_writer);
	enum so_format_writer_status status = peer->format_writer->ops->add_file(peer->format_writer, &file, selected_path);
	int last_errno = peer->format_writer->ops->last_errno(peer->format_writer);
	ssize_t position = peer->format_writer->ops->position(peer->format_writer);
	ssize_t available_size = peer->format_writer->ops->get_available_size(peer->format_writer);

	so_format_file_free(&file);

	peer->nb_total_bytes += position - current_position;

	struct so_value * response = so_value_pack("{sisiszsz}",
		"returned", status,
		"last errno", last_errno,
		"position", position,
		"available size", available_size
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_writer_add_label(struct sodr_peer * peer, struct so_value * request) {
	char * label = NULL;
	so_value_unpack(request, "{s{so}}", "params", "label", &label);

	ssize_t current_position = peer->format_writer->ops->position(peer->format_writer);
	enum so_format_writer_status status = peer->format_writer->ops->add_label(peer->format_writer, label);
	int last_errno = peer->format_writer->ops->last_errno(peer->format_writer);
	ssize_t position = peer->format_writer->ops->position(peer->format_writer);
	ssize_t available_size = peer->format_writer->ops->get_available_size(peer->format_writer);

	free(label);

	peer->nb_total_bytes += position - current_position;

	struct so_value * response = so_value_pack("{sisiszsz}",
		"returned", status,
		"last errno", last_errno,
		"position", position,
		"available size", available_size
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_writer_close(struct sodr_peer * peer, struct so_value * request) {
	bool change_volume = false;
	so_value_unpack(request, "{s{sb}}", "params", "change volume", &change_volume);

	int failed = peer->format_writer->ops->close(peer->format_writer, change_volume);
	int last_errno = peer->format_writer->ops->last_errno(peer->format_writer);

	struct so_value * response = so_value_pack("{sisi}", "returned", failed, "last errno", last_errno);

	if (failed == 0 && peer->has_checksums) {
		struct so_value * digests = peer->format_writer->ops->get_digests(peer->format_writer);
		so_value_hashtable_put2(response, "digests", digests, true);
	}

	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_writer_compute_size_of_file(struct sodr_peer * peer, struct so_value * request) {
	struct so_value * vfile = NULL;
	so_value_unpack(request, "{s{so}}", "params", "file", &vfile);

	struct so_format_file file;
	so_format_file_init(&file);
	so_format_file_sync(&file, vfile);

	ssize_t size = peer->format_writer->ops->compute_size_of_file(peer->format_writer, &file);

	so_format_file_free(&file);

	struct so_value * response = so_value_pack("{sz}", "returned", size);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_writer_end_of_file(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	ssize_t current_position = peer->format_writer->ops->position(peer->format_writer);
	ssize_t eof = peer->format_writer->ops->end_of_file(peer->format_writer);
	int last_errno = peer->format_writer->ops->last_errno(peer->format_writer);

	ssize_t position = peer->format_writer->ops->position(peer->format_writer);
	ssize_t available_size = peer->format_writer->ops->get_available_size(peer->format_writer);

	peer->nb_total_bytes += position - current_position;

	struct so_value * response = so_value_pack("{szsiszsz}",
		"returned", eof,
		"last errno", last_errno,
		"position", position,
		"available size", available_size
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_writer_free(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	sodr_io_print_throughtput(peer);

	peer->format_writer->ops->free(peer->format_writer);
	peer->format_writer = NULL;
	close(peer->fd_data);
	peer->fd_data = -1;
	free(peer->buffer);
	peer->buffer = NULL;
	peer->buffer_length = 0;
	peer->has_checksums = false;

	close(peer->fd_cmd);
	peer->fd_cmd = -1;
}

static void sodr_io_format_writer_get_alternate_path(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	char * alternate_path = peer->format_writer->ops->get_alternate_path(peer->format_writer);

	struct so_value * response = so_value_pack("{ss}", "returned", alternate_path);

	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);

	free(alternate_path);
}

static void sodr_io_format_writer_reopen(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;

	sodr_io_print_throughtput(peer);

	struct so_media * media = drive->slot->media;

	const char * media_name = NULL;
	if (media != NULL)
		media_name = media->name;

	int position = peer->format_writer->ops->file_position(peer->format_writer);

	so_log_write(so_log_level_notice,
		dgettext("libstoriqone-drive", "[%s %s #%u]: reopen media '%s' for reading at position #%d"),
		drive->vendor, drive->model, drive->index, media_name, position);

	peer->format_reader = peer->format_writer->ops->reopen(peer->format_writer);

	bool ok = peer->format_reader != NULL;
	struct so_value * response = so_value_pack("{sb}", "status", ok);
	if (ok && peer->has_checksums) {
		struct so_value * digests = peer->format_writer->ops->get_digests(peer->format_writer);
		so_value_hashtable_put2(response, "digests", digests, true);
	}
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);

	if (ok) {
		so_thread_pool_set_name("format reader");
		sodr_io_format_reader(peer);

		peer->format_writer->ops->free(peer->format_writer);
		peer->format_writer = NULL;
	}
}

static void sodr_io_format_writer_restart_file(struct sodr_peer * peer, struct so_value * request) {
	struct so_value * vfile = NULL;
	so_value_unpack(request, "{s{so}}", "params", "file", &vfile);

	struct so_format_file file;
	so_format_file_init(&file);
	so_format_file_sync(&file, vfile);

	ssize_t current_position = peer->format_writer->ops->position(peer->format_writer);
	enum so_format_writer_status status = peer->format_writer->ops->restart_file(peer->format_writer, &file);
	int last_errno = peer->format_writer->ops->last_errno(peer->format_writer);
	ssize_t new_position = peer->format_writer->ops->position(peer->format_writer);
	ssize_t available_size = peer->format_writer->ops->get_available_size(peer->format_writer);

	so_format_file_free(&file);

	peer->nb_total_bytes += new_position - current_position;

	struct so_value * response = so_value_pack("{sisiszsz}",
		"returned", status,
		"last errno", last_errno,
		"position", new_position,
		"available size", available_size
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_writer_write(struct sodr_peer * peer, struct so_value * request) {
	ssize_t length = 0;
	so_value_unpack(request, "{s{sz}}", "params", "length", &length);

	ssize_t nb_total_write = 0;
	while (nb_total_write < length) {
		ssize_t nb_will_read = length - nb_total_write;
		if (nb_will_read > peer->buffer_length)
			nb_will_read = peer->buffer_length;

		ssize_t nb_read = recv(peer->fd_data, peer->buffer, nb_will_read, 0);

		if (nb_read < 0) {
			struct so_value * response = so_value_pack("{sisi}", "returned", -1, "last errno", errno);
			so_json_encode_to_fd(response, peer->fd_cmd, true);
			so_value_free(response);
			return;
		}

		ssize_t nb_write = peer->format_writer->ops->write(peer->format_writer, peer->buffer, nb_read);
		if (nb_write < 0) {
			int last_errno = peer->format_writer->ops->last_errno(peer->format_writer);
			struct so_value * response = so_value_pack("{sisi}", "returned", -1, "last errno", last_errno);
			so_json_encode_to_fd(response, peer->fd_cmd, true);
			so_value_free(response);
			return;
		}

		nb_total_write += nb_write;
		peer->nb_total_bytes += nb_write;
	}

	ssize_t position = peer->format_writer->ops->position(peer->format_writer);
	ssize_t available_size = peer->format_writer->ops->get_available_size(peer->format_writer);

	struct so_value * response = so_value_pack("{szsiszsz}",
		"returned", nb_total_write,
		"last errno", 0,
		"position", position,
		"available size", available_size
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_format_writer_write_metadata(struct sodr_peer * peer, struct so_value * request) {
	struct so_value * metadata = NULL;
	so_value_unpack(request, "{s{so}}", "params", "metadata", &metadata);

	ssize_t nb_write = peer->format_writer->ops->write_metadata(peer->format_writer, metadata);

	ssize_t position = peer->format_writer->ops->position(peer->format_writer);
	ssize_t available_size = peer->format_writer->ops->get_available_size(peer->format_writer);

	int last_errno = 0;
	if (nb_write < 0)
		last_errno = peer->format_writer->ops->last_errno(peer->format_writer);

	struct so_value * response = so_value_pack("{szsiszsz}",
		"returned", nb_write,
		"last errno", last_errno,
		"position", position,
		"available size", available_size
	);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}
