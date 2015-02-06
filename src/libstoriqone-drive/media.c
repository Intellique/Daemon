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
// snprintf
#include <stdio.h>
// free
#include <stdlib.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>

#include <libstoriqone/checksum.h>
#include <libstoriqone/database.h>
#include <libstoriqone/host.h>
#include <libstoriqone/log.h>

#include "media.h"

#include "storiqone.version"

static bool sodr_media_read_header_v1(struct so_media * media, const char * buffer, int nb_parsed, bool check, struct so_database_connection * db_connection);
static bool sodr_media_read_header_v2(struct so_media * media, const char * buffer, int nb_parsed, bool check, struct so_database_connection * db_connection);
static bool sodr_media_read_header_v3(struct so_media * media, const char * buffer, int nb_parsed, bool check, struct so_database_connection * db_connection);


bool sodr_media_check_header(struct so_media * media, const char * buffer, struct so_database_connection * db_connection) {
	char storiqone_version[65];
	int media_format_version = 0;
	int nb_parsed = 0;
	bool ok = false;

	int nb_params = sscanf(buffer, "Storiq One (%64[^)])\nMedia format: version=%d\n%n", storiqone_version, &media_format_version, &nb_parsed);
	if (nb_params < 2)
		nb_params = sscanf(buffer, "STone (%64[^)])\nTape format: version=%d\n%n", storiqone_version, &media_format_version, &nb_parsed);

	if (nb_params == 2) {
		switch (media_format_version) {
			case 1:
				ok = sodr_media_read_header_v1(media, buffer, nb_parsed, true, db_connection);
				break;

			case 2:
				ok = sodr_media_read_header_v2(media, buffer, nb_parsed, true, db_connection);
				break;

			case 3:
				ok = sodr_media_read_header_v3(media, buffer, nb_parsed, true, db_connection);
				break;
		}
	}

	return ok;
}

static bool sodr_media_read_header_v1(struct so_media * media, const char * buffer, int nb_parsed2, bool check, struct so_database_connection * db_connection __attribute__((unused))) {
	// M | STone (v0.1)
	// M | Tape format: version=1
	// O | Label: A0000002
	// M | Tape id: uuid=f680dd48-dd3e-4715-8ddc-a90d3e708914
	// M | Pool: name=Foo, uuid=07117f1a-2b13-11e1-8bcb-80ee73001df6
	// M | Block size: 32768
	// M | Checksum: crc32=1eb6931d

	char uuid[37];
	char name[65];
	char pool_id[37];
	char pool_name[65];
	ssize_t block_size;
	char checksum_name[12];
	char checksum_value[64];

	int nb_parsed = nb_parsed2;
	bool ok = true;
	bool has_label = false;

	if (sscanf(buffer + nb_parsed, "Label: %36s\n%n", name, &nb_parsed2) == 1) {
		nb_parsed += nb_parsed2;
		has_label = true;
	}

	if (ok && sscanf(buffer + nb_parsed, "Tape id: uuid=%36s\n%n", uuid, &nb_parsed2) == 1)
		nb_parsed += nb_parsed2;
	else
		ok = false;

	if (ok && sscanf(buffer + nb_parsed, "Pool: name=%65[^,], uuid=%36s\n%n", pool_name, pool_id, &nb_parsed2) == 2)
		nb_parsed += nb_parsed2;
	else
		ok = false;

	if (ok && sscanf(buffer + nb_parsed, "Block size: %zd\n%n", &block_size, &nb_parsed2) == 1)
		nb_parsed += nb_parsed2;
	else
		ok = false;

	if (ok)
		ok = sscanf(buffer + nb_parsed, "Checksum: %11[^=]=%63s\n", checksum_name, checksum_value) == 2;

	if (ok) {
		char * digest = so_checksum_compute(checksum_name, buffer, nb_parsed);
		ok = digest != NULL && !strcmp(checksum_value, digest);
		free(digest);

		media->pool = db_connection->ops->get_pool(db_connection, pool_id, NULL);
		// TODO: create pool
		// if (media->pool == NULL)
		//	media->pool = so_pool_create(pool_id, pool_name, media->format);
	}

	if (ok) {
		if (check) {
			ok = !strcmp(media->uuid, uuid) && !strcmp(media->pool->uuid, pool_id);

			so_log_write(so_log_level_info, dgettext("libstoriqone-drive", "Checking Storiq One header in media: %s"), ok ? "OK" : "Failed");
		} else {
			so_log_write(so_log_level_debug, dgettext("libstoriqone-drive", "Found Storiq One header in media with (uuid: %s, label: %s, blocksize: %zd)"), uuid, name, block_size);

			strcpy(media->uuid, uuid);
			if (has_label) {
				free(media->label);
				media->label = strdup(name);
			}
			free(media->name);
			media->name = strdup(name);
			media->status = so_media_status_in_use;
			media->block_size = block_size;
		}
	} else if (check)
		so_log_write(so_log_level_info, dgettext("libstoriqone-drive", "Checking Storiq One header in media: Failed"));
	else
		media->status = so_media_status_foreign;

	return ok;
}

static bool sodr_media_read_header_v2(struct so_media * media, const char * buffer, int nb_parsed2, bool check, struct so_database_connection * db_connection) {
	// M | STone (v0.2)
	// M | Tape format: version=2
	// M | Host: name=kazoo, uuid=40e576d7-cb14-42c2-95c5-edd14fbb638d
	// O | Label: A0000002
	// M | Tape id: uuid=f680dd48-dd3e-4715-8ddc-a90d3e708914
	// M | Pool: name=Foo, uuid=07117f1a-2b13-11e1-8bcb-80ee73001df6
	// M | Block size: 32768
	// M | Checksum: crc32=1eb6931d

	char uuid[37];
	char name[65];
	char pool_id[37];
	char pool_name[65];
	ssize_t block_size;
	char checksum_name[12];
	char checksum_value[64];

	int nb_parsed = nb_parsed2;
	bool ok = true;
	bool has_label = false;

	if (ok && sscanf(buffer + nb_parsed, "Host: name=%64[^,], uuid=%36s\n%n", name, uuid, &nb_parsed2) == 2)
		nb_parsed += nb_parsed2;
	else
		ok = false;

	if (sscanf(buffer + nb_parsed, "Label: %36s\n%n", name, &nb_parsed2) == 1) {
		nb_parsed += nb_parsed2;
		has_label = true;
	}

	if (ok && sscanf(buffer + nb_parsed, "Tape id: uuid=%36s\n%n", uuid, &nb_parsed2) == 1)
		nb_parsed += nb_parsed2;
	else
		ok = false;

	if (ok && sscanf(buffer + nb_parsed, "Pool: name=%65[^,], uuid=%36s\n%n", pool_name, pool_id, &nb_parsed2) == 2)
		nb_parsed += nb_parsed2;
	else
		ok = false;

	if (ok && sscanf(buffer + nb_parsed, "Block size: %zd\n%n", &block_size, &nb_parsed2) == 1)
		nb_parsed += nb_parsed2;
	else
		ok = false;

	if (ok)
		ok = sscanf(buffer + nb_parsed, "Checksum: %11[^=]=%63s\n", checksum_name, checksum_value) == 2;

	if (ok) {
		char * digest = so_checksum_compute(checksum_name, buffer, nb_parsed);
		ok = digest != NULL && !strcmp(checksum_value, digest);
		free(digest);

		media->pool = db_connection->ops->get_pool(db_connection, pool_id, NULL);
		// TODO: create pool
		// if (media->pool == NULL)
		//	media->pool = so_pool_create(pool_id, pool_name, media->format);
	}

	if (ok) {
		if (check) {
			ok = !strcmp(media->uuid, uuid) && !strcmp(media->pool->uuid, pool_id);

			so_log_write(so_log_level_info, dgettext("libstoriqone-drive", "Checking Storiq One header in media: %s"), ok ? "OK" : "Failed");
		} else {
			so_log_write(so_log_level_debug, dgettext("libstoriqone-drive", "Found Storiq One header in media with (uuid: %s, label: %s, blocksize: %zd)"), uuid, name, block_size);

			strcpy(media->uuid, uuid);
			if (has_label) {
				free(media->label);
				media->label = strdup(name);
			}
			free(media->name);
			media->name = strdup(name);
			media->status = so_media_status_in_use;
			media->block_size = block_size;
		}
	} else if (check)
		so_log_write(so_log_level_info, dgettext("libstoriqone-drive", "Checking Storiq One header in media: Failed"));
	else
		media->status = so_media_status_foreign;

	return ok;
}

static bool sodr_media_read_header_v3(struct so_media * media, const char * buffer, int nb_parsed2, bool check, struct so_database_connection * db_connection) {
	// M | STone (v1.2)
	// M | Media format: version=3
	// M | Host: name="kazoo", uuid="40e576d7-cb14-42c2-95c5-edd14fbb638d"
	// O | Label: "A0000002"
	// M | Media id: uuid="f680dd48-dd3e-4715-8ddc-a90d3e708914"
	// M | Pool: name="Foo", uuid="07117f1a-2b13-11e1-8bcb-80ee73001df6"
	// M | Block size: 32768
	// M | Checksum: sha1="ff3ac5ae13653067030b56c5f1899eefd4ff2584"

	char uuid[37];
	char name[65];
	char pool_id[37];
	char pool_name[65];
	ssize_t block_size;
	char checksum_name[12];
	char checksum_value[64];

	int nb_parsed = nb_parsed2;
	bool ok = true;
	bool has_label = false;

	if (ok && sscanf(buffer + nb_parsed, "Host: name=\"%64[^\"]\", uuid=\"%36s\"\n%n", name, uuid, &nb_parsed2) == 2)
		nb_parsed += nb_parsed2;
	else
		ok = false;

	if (sscanf(buffer + nb_parsed, "Label: \"%64[^\"]\"\n%n", name, &nb_parsed2) == 1) {
		nb_parsed += nb_parsed2;
		has_label = true;
	}

	if (ok && sscanf(buffer + nb_parsed, "Media id: uuid=\"%36[^\"]\"\n%n", uuid, &nb_parsed2) == 1)
		nb_parsed += nb_parsed2;
	else
		ok = false;

	if (ok && sscanf(buffer + nb_parsed, "Pool: name=\"%65[^\"]\", uuid=\"%36[^\"]\"\n%n", pool_name, pool_id, &nb_parsed2) == 2)
		nb_parsed += nb_parsed2;
	else
		ok = false;

	if (ok && sscanf(buffer + nb_parsed, "Block size: %zd\n%n", &block_size, &nb_parsed2) == 1)
		nb_parsed += nb_parsed2;
	else
		ok = false;

	if (ok)
		ok = sscanf(buffer + nb_parsed, "Checksum: %11[^=]=\"%63[^\"]\"\n", checksum_name, checksum_value) == 2;

	if (ok) {
		char * digest = so_checksum_compute(checksum_name, buffer, nb_parsed);
		ok = digest != NULL && !strcmp(checksum_value, digest);
		free(digest);

		media->pool = db_connection->ops->get_pool(db_connection, pool_id, NULL);
		// TODO: create pool
		// if (media->pool == NULL)
		//	media->pool = so_pool_create(pool_id, pool_name, media->format);
	}

	if (ok) {
		if (check) {
			ok = !strcmp(media->uuid, uuid) && !strcmp(media->pool->uuid, pool_id);

			so_log_write(so_log_level_info, dgettext("libstoriqone-drive", "Checking Storiq One header in media: %s"), ok ? "OK" : "Failed");
		} else {
			so_log_write(so_log_level_debug, dgettext("libstoriqone-drive", "Found Storiq One header in media with (uuid: %s, label: %s, blocksize: %zd)"), uuid, name, block_size);

			strcpy(media->uuid, uuid);
			if (has_label) {
				free(media->label);
				media->label = strdup(name);
			}
			free(media->name);
			media->name = strdup(name);
			media->status = so_media_status_in_use;
			media->block_size = block_size;
		}
	} else if (check)
		so_log_write(so_log_level_info, dgettext("libstoriqone-drive", "Checking Storiq One header in media: Failed"));
	else
		media->status = so_media_status_foreign;

	return ok;
}

bool sodr_media_write_header(struct so_media * media, struct so_pool * pool, char * buffer, size_t length) {
	bzero(buffer, length);

	if (*media->uuid == '\0') {
		uuid_t id;
		uuid_generate(id);
		uuid_unparse_lower(id, media->uuid);
	}

	struct so_host * host = so_host_get_info();

	// M | Storiq One (v1.2)
	// M | Media format: version=3
	// M | Host: name="kazoo", uuid="40e576d7-cb14-42c2-95c5-edd14fbb638d"
	// O | Label: "A0000002"
	// M | Media id: uuid="f680dd48-dd3e-4715-8ddc-a90d3e708914"
	// M | Pool: name="Foo", uuid="07117f1a-2b13-11e1-8bcb-80ee73001df6"
	// M | Block size: 32768
	// M | Checksum: sha1="ff3ac5ae13653067030b56c5f1899eefd4ff2584"
	size_t nb_write = snprintf(buffer, length, "Storiq One (" STORIQONE_VERSION ")\nMedia format: version=3\nHost: name=\"%s\", uuid=\"%s\"\n", host->hostname, host->uuid);
	size_t position = nb_write;

	/**
	 * With stand-alone tape drive, we cannot retreive tape label.
	 * In this case, media's label is NULL.
	 * That why, Label field in header is optionnal
	 */
	if (media->label != NULL) {
		nb_write = snprintf(buffer + position, length - position, "Label: \"%s\"\n", media->label);
		position += nb_write;
	}

	nb_write = snprintf(buffer + position, length - position, "Media id: uuid=\"%s\"\nPool: name=\"%s\", uuid=\"%s\"\nBlock size: %zd\n", media->uuid, pool->name, pool->uuid, media->block_size);
	position += nb_write;

	// compute header's checksum
	char * digest = so_checksum_compute("sha1", buffer, position);
	if (digest == NULL) {
		so_log_write(so_log_level_info, dgettext("libstoriqone-drive", "Failed to compute digest before write media header"));
		return false;
	}

	snprintf(buffer + position, length - position, "Checksum: sha1=\"%s\"\n", digest);

	free(digest);

	return true;
}

