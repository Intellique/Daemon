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

// dgettext
#include <libintl.h>
// malloc
#include <stdlib.h>
// bzero
#include <strings.h>
// close
#include <unistd.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>

#include "io.h"
#include "../drive.h"
#include "../listen.h"
#include "../peer.h"

static void sodr_io_import_media_count_archives(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_import_media_finish_import_media(struct sodr_peer * peer, struct so_value * request);
static void sodr_io_import_media_init(void) __attribute__((constructor));
static void sodr_io_import_media_parse_archive(struct sodr_peer * peer, struct so_value * request);

static struct sodr_command commands[] = {
	{ 0, "count archives",       sodr_io_import_media_count_archives },
	{ 0, "finish import media",  sodr_io_import_media_finish_import_media },
	{ 0, "parse archive",        sodr_io_import_media_parse_archive },

	{ 0, NULL, NULL },
};


void sodr_io_import_media(void * arg) {
	sodr_io_process(arg, commands);
}

static void sodr_io_import_media_init() {
	sodr_io_init(commands);
}


static void sodr_io_import_media_count_archives(struct sodr_peer * peer, struct so_value * request __attribute__((unused))) {
	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;
	struct so_media * media = drive->slot->media;

	const char * media_name = NULL;
	if (media != NULL)
		media_name = media->name;

	so_log_write(so_log_level_notice,
		dgettext("libstoriqone-drive", "[%s %s #%u]: counting archives from media '%s'"),
		drive->vendor, drive->model, drive->index, media_name);

	unsigned int nb_archives = drive->ops->count_archives(&peer->disconnected, peer->db_connection);

	struct so_value * response = so_value_pack("{su}", "returned", nb_archives);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);
}

static void sodr_io_import_media_finish_import_media(struct sodr_peer * peer, struct so_value * request) {
	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;
	struct so_media * media = drive->slot->media;

	const char * media_name = NULL;
	if (media != NULL)
		media_name = media->name;

	char * uuid = NULL;
	so_value_unpack(request, "{s{s{ss}}}",
		"params",
			"pool",
				"uuid", &uuid
	);

	struct so_pool * pool = peer->db_connection->ops->get_pool(peer->db_connection, uuid, NULL);

	free(uuid);

	so_log_write(so_log_level_notice,
		dgettext("libstoriqone-drive", "[%s %s #%u]: attach media '%s' to pool '%s'"),
		drive->vendor, drive->model, drive->index, media_name, pool->name);

	media->status = so_media_status_in_use;
	media->pool = pool;

	int failed = peer->db_connection->ops->sync_media(peer->db_connection, media, so_database_sync_default);

	struct so_value * response = so_value_pack("{si}", "returned", failed);
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);

	close(peer->fd_cmd);
	peer->fd_cmd = -1;
}

static void sodr_io_import_media_parse_archive(struct sodr_peer * peer, struct so_value * request) {
	struct so_drive_driver * driver = sodr_drive_get();
	struct so_drive * drive = driver->device;
	struct so_media * media = drive->slot->media;

	const char * media_name = NULL;
	if (media != NULL)
		media_name = media->name;

	int archive_position = -1;
	struct so_value * checksums = NULL;

	so_value_unpack(request, "{s{sisO}}",
		"params",
			"archive position", &archive_position,
			"checksums", &checksums
	);

	so_log_write(so_log_level_notice,
		dgettext("libstoriqone-drive", "[%s %s #%u]: Parsing archive from media '%s' at position #%u"),
		drive->vendor, drive->model, drive->index, media_name, archive_position);

	struct so_archive * archive = drive->ops->parse_archive(&peer->disconnected, archive_position, checksums, peer->db_connection);

	struct so_value * response = so_value_pack("{so}", "returned", so_archive_convert(archive));
	so_json_encode_to_fd(response, peer->fd_cmd, true);
	so_value_free(response);

	so_archive_free(archive);
}

