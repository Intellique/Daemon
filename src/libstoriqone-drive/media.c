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

// dgettext
#include <libintl.h>
// sscanf, snprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/checksum.h>
#include <libstoriqone/database.h>
#include <libstoriqone/format.h>
#include <libstoriqone/host.h>
#include <libstoriqone/io.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/value.h>
#include <libstoriqone-drive/drive.h>
#include <libstoriqone-drive/log.h>

#include "media.h"

#include "storiqone.version"

static bool sodr_media_read_header_v1(struct so_media * media, const char * buffer, int nb_parsed, bool restore_data, struct so_database_connection * db);
static bool sodr_media_read_header_v2(struct so_media * media, const char * buffer, int nb_parsed, bool restore_data, struct so_database_connection * db);
static bool sodr_media_read_header_v3(struct so_media * media, const char * buffer, int nb_parsed, bool restore_data, struct so_database_connection * db);


bool sodr_media_check_header(struct so_media * media, const char * buffer, bool restore_data, struct so_database_connection * db) {
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
				ok = sodr_media_read_header_v1(media, buffer, nb_parsed, restore_data, db);
				break;

			case 2:
				ok = sodr_media_read_header_v2(media, buffer, nb_parsed, restore_data, db);
				break;

			case 3:
				ok = sodr_media_read_header_v3(media, buffer, nb_parsed, restore_data, db);
				break;
		}
	}

	return ok;
}

static bool sodr_media_read_header_v1(struct so_media * media, const char * buffer, int nb_parsed2, bool restore_data, struct so_database_connection * db) {
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
	}

	if (ok) {
		if (!restore_data) {
			ok = !strcmp(media->uuid, uuid) && media->pool != NULL && !strcmp(media->pool->uuid, pool_id);

			sodr_log_add_record(so_job_status_running, db, ok ? so_log_level_info : so_log_level_warning, so_job_record_notif_normal,
				dgettext("libstoriqone-drive", "Checking Storiq One header in media: %s"),
				ok ? "OK" : "Failed");
		} else {
			sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
				dgettext("libstoriqone-drive", "Found Storiq One header in media with (uuid: %s, label: %s, blocksize: %zd)"),
				uuid, name, block_size);

			strcpy(media->uuid, uuid);
			if (has_label) {
				free(media->label);
				media->label = strdup(name);
			}
			free(media->name);
			media->name = strdup(name);
			media->block_size = block_size;
		}
	} else if (!restore_data)
		sodr_log_add_record(so_job_status_running, db, so_log_level_info, so_job_record_notif_normal,
			dgettext("libstoriqone-drive", "Checking Storiq One header in media: Failed"));
	else
		media->status = so_media_status_foreign;

	return ok;
}

static bool sodr_media_read_header_v2(struct so_media * media, const char * buffer, int nb_parsed2, bool restore_data, struct so_database_connection * db) {
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
	}

	if (ok) {
		if (!restore_data) {
			ok = !strcmp(media->uuid, uuid) && media->pool != NULL && !strcmp(media->pool->uuid, pool_id);

			sodr_log_add_record(so_job_status_running, db, ok ? so_log_level_info : so_log_level_warning, so_job_record_notif_normal,
				dgettext("libstoriqone-drive", "Checking Storiq One header in media: %s"),
				ok ? "OK" : "Failed");
		} else {
			sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
				dgettext("libstoriqone-drive", "Found Storiq One header in media with (uuid: %s, label: %s, blocksize: %zd)"),
				uuid, name, block_size);

			strcpy(media->uuid, uuid);
			if (has_label) {
				free(media->label);
				media->label = strdup(name);
			}
			free(media->name);
			media->name = strdup(name);
			media->block_size = block_size;
		}
	} else if (!restore_data)
		sodr_log_add_record(so_job_status_running, db, so_log_level_info, so_job_record_notif_normal,
			dgettext("libstoriqone-drive", "Checking Storiq One header in media: Failed"));
	else
		media->status = so_media_status_foreign;

	return ok;
}

static bool sodr_media_read_header_v3(struct so_media * media, const char * buffer, int nb_parsed2, bool restore_data, struct so_database_connection * db) {
	// M | Storiq One (v1.2)
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
	}

	if (ok) {
		if (!restore_data) {
			ok = !strcmp(media->uuid, uuid) && media->pool != NULL && !strcmp(media->pool->uuid, pool_id);

			sodr_log_add_record(so_job_status_running, db, ok ? so_log_level_info : so_log_level_warning, so_job_record_notif_normal,
				dgettext("libstoriqone-drive", "Checking Storiq One header in media: %s"),
				ok ? "OK" : "Failed");
		} else {
			sodr_log_add_record(so_job_status_running, db, so_log_level_info, so_job_record_notif_normal,
				dgettext("libstoriqone-drive", "Found Storiq One header in media with (uuid: %s, label: %s, blocksize: %zd)"),
				uuid, name, block_size);

			strcpy(media->uuid, uuid);
			if (has_label) {
				free(media->label);
				media->label = strdup(name);
			}
			free(media->name);
			media->name = strdup(name);
			media->block_size = block_size;
		}
	} else if (!restore_data)
		sodr_log_add_record(so_job_status_running, db, so_log_level_info, so_job_record_notif_normal,
			dgettext("libstoriqone-drive", "Checking Storiq One header in media: Failed"));
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
		so_log_write(so_log_level_info,
			dgettext("libstoriqone-drive", "Failed to compute digest before writing media header"));
		return false;
	}

	snprintf(buffer + position, length - position, "Checksum: sha1=\"%s\"\n", digest);

	free(digest);

	return true;
}


unsigned int sodr_media_storiqone_count_files(struct so_drive * drive, const bool * const disconnected, struct so_database_connection * db_connection) {
	struct so_format_reader * tar = drive->ops->get_reader(1, NULL, db_connection);
	if (tar == NULL)
		return 0;

	if (*disconnected) {
		tar->ops->free(tar);
		return 0;
	}

	struct so_format_file file;
	so_format_file_init(&file);
	enum so_format_reader_header_status status = tar->ops->get_header(tar, &file);

	if (status == so_format_reader_header_ok)
		so_format_file_free(&file);

	tar->ops->free(tar);

	if (status != so_format_reader_header_ok || *disconnected)
		return status != so_format_reader_header_ok ? 0 : 1;

	unsigned int nb_archives = 0;
	unsigned int i_file = 2;

	while (!*disconnected) {
		struct so_stream_reader * reader = drive->ops->get_raw_reader(i_file, db_connection);
		if (reader == NULL)
			break;

		if (*disconnected) {
			reader->ops->free(reader);
			break;
		}

		struct so_value * index = so_json_parse_stream(reader);
		if (index != NULL) {
			i_file++;
			nb_archives++;
			so_value_free(index);
		}

		reader->ops->free(reader);

		if (*disconnected)
			break;

		tar = drive->ops->get_reader(i_file, NULL, db_connection);
		if (tar == NULL)
			break;

		if (*disconnected) {
			tar->ops->free(tar);
			break;
		}

		so_format_file_init(&file);
		status = tar->ops->get_header(tar, &file);

		if (status == so_format_reader_header_ok) {
			so_format_file_free(&file);
			i_file++;
		}

		tar->ops->free(tar);

		if (status != so_format_reader_header_ok || *disconnected)
			break;
	}

	return nb_archives;
}

struct so_archive * sodr_media_storiqone_parse_archive(struct so_drive * drive, const bool * const disconnected, unsigned int archive_position, struct so_database_connection * db_connection) {
	struct so_format_reader * tar = drive->ops->get_reader(1, NULL, db_connection);
	if (tar == NULL)
		return NULL;

	if (*disconnected) {
		tar->ops->free(tar);
		return NULL;
	}

	struct so_format_file file;
	so_format_file_init(&file);
	enum so_format_reader_header_status status = tar->ops->get_header(tar, &file);

	if (status == so_format_reader_header_ok)
		so_format_file_free(&file);

	tar->ops->free(tar);

	if (status != so_format_reader_header_ok || *disconnected)
		return NULL;

	unsigned int nb_archives = 0;
	unsigned int i_file = 2;

	while (!*disconnected && nb_archives <= archive_position) {
		struct so_stream_reader * reader = drive->ops->get_raw_reader(i_file, db_connection);
		if (reader == NULL)
			break;

		if (*disconnected) {
			reader->ops->free(reader);
			break;
		}

		struct so_value * index = so_json_parse_stream(reader);
		if (index != NULL) {
			i_file++;
			nb_archives++;

			if (nb_archives == archive_position) {
				struct so_archive * archive = malloc(sizeof(struct so_archive));
				bzero(archive, sizeof(struct so_archive));

				so_archive_sync(archive, index);

				so_value_free(index);
				reader->ops->free(reader);

				return archive;
			}

			so_value_free(index);
		}

		reader->ops->free(reader);

		if (*disconnected)
			break;

		tar = drive->ops->get_reader(i_file, NULL, db_connection);
		if (tar == NULL)
			break;

		if (*disconnected) {
			tar->ops->free(tar);
			break;
		}

		so_format_file_init(&file);
		status = tar->ops->get_header(tar, &file);

		if (status == so_format_reader_header_ok) {
			so_format_file_free(&file);
			i_file++;
		}

		tar->ops->free(tar);

		if (status != so_format_reader_header_ok || *disconnected)
			break;
	}

	return NULL;
}

