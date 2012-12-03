/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Thu, 29 Nov 2012 18:36:53 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// pthread_mutex_lock, pthread_mutex_unlock
#include <pthread.h>
// malloc, realloc
#include <stdlib.h>
// sscanf
#include <stdio.h>
// memcpy, strcasecmp, strcmp
#include <string.h>
// bzero
#include <strings.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>

#include <libstone/checksum.h>
#include <libstone/database.h>
#include <libstone/io.h>
#include <libstone/library/changer.h>
#include <libstone/library/drive.h>
#include <libstone/library/media.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>

#include "stone.version"


static pthread_mutex_t st_media_lock = PTHREAD_MUTEX_INITIALIZER;
static struct st_media ** st_medias = NULL;
static unsigned int st_media_nb_medias = 0;

static pthread_mutex_t st_media_format_lock = PTHREAD_MUTEX_INITIALIZER;
static struct st_media_format ** st_media_formats = NULL;
static unsigned int st_media_format_nb_formats = 0;

static pthread_mutex_t st_pool_lock = PTHREAD_MUTEX_INITIALIZER;
static struct st_pool ** st_pools = NULL;
static unsigned int st_pool_nb_pools = 0;

static void st_media_add(struct st_media * media);
static struct st_media * st_media_retrieve(struct st_database_connection * connection, struct st_job * job, const char * uuid, const char * medium_serial_number, const char * label);

static int st_media_format_retrieve(struct st_media_format ** media_format, unsigned char density_code, enum st_media_format_mode mode);

static struct st_pool * st_pool_retreive(struct st_database_connection * connection, struct st_job * job, const char * uuid);


static const struct st_media_format_data_type2 {
	char * name;
	enum st_media_format_data_type type;
} st_media_format_data_types[] = {
	{ "audio",    st_media_format_data_audio },
	{ "cleaning", st_media_format_data_cleaning },
	{ "data",     st_media_format_data_data },
	{ "video",    st_media_format_data_video },

	{ "unknown", st_media_format_data_unknown },
};

static const struct st_media_format_mode2 {
	char * name;
	enum st_media_format_mode mode;
} st_media_format_modes[] = {
	{ "disk",    st_media_format_mode_disk },
	{ "linear",  st_media_format_mode_linear },
	{ "optical", st_media_format_mode_optical },

	{ "unknown", st_media_format_mode_unknown },
};

static const struct st_media_location2 {
	char * name;
	enum st_media_location location;
} st_media_locations[] = {
	{ "in drive", st_media_location_indrive },
	{ "offline",  st_media_location_offline },
	{ "online",   st_media_location_online },

	{ "unknown", st_media_location_unknown },
};

static const struct st_media_status2 {
	char * name;
	enum st_media_status status;
} st_media_status[] = {
	{ "erasable",          st_media_status_erasable },
	{ "error",             st_media_status_error },
	{ "foreign",           st_media_status_foreign },
	{ "in use",            st_media_status_in_use },
	{ "locked",            st_media_status_locked },
	{ "needs replacement", st_media_status_needs_replacement },
	{ "new",               st_media_status_new },
	{ "pooled",            st_media_status_pooled },

	{ "unknown", st_media_status_unknown },
};

static const struct st_media_type2 {
	char * name;
	enum st_media_type type;
} st_media_types[] = {
	{ "cleaning",   st_media_type_cleanning },
	{ "read only",  st_media_type_readonly },
	{ "rewritable", st_media_type_rewritable },

	{ "unknown", st_media_type_unknown },
};


const char * st_media_format_data_to_string(enum st_media_format_data_type type) {
	unsigned int i;
	for (i = 0; st_media_format_data_types[i].type != st_media_format_data_unknown; i++)
		if (st_media_format_data_types[i].type == type)
			return st_media_format_data_types[i].name;

	return st_media_format_data_types[i].name;
}

const char * st_media_format_mode_to_string(enum st_media_format_mode mode) {
	unsigned int i;
	for (i = 0; st_media_format_modes[i].mode != st_media_format_mode_unknown; i++)
		if (st_media_format_modes[i].mode == mode)
			return st_media_format_modes[i].name;

	return st_media_format_modes[i].name;
}

const char * st_media_location_to_string(enum st_media_location location) {
	unsigned int i;
	for (i = 0; st_media_locations[i].location != st_media_location_unknown; i++)
		if (st_media_locations[i].location == location)
			return st_media_locations[i].name;

	return st_media_locations[i].name;
}

const char * st_media_status_to_string(enum st_media_status status) {
	unsigned int i;
	for (i = 0; st_media_status[i].status != st_media_status_unknown; i++)
		if (st_media_status[i].status == status)
			return st_media_status[i].name;

	return st_media_status[i].name;
}

const char * st_media_type_to_string(enum st_media_type type) {
	unsigned int i;
	for (i = 0; st_media_types[i].type != st_media_type_unknown; i++)
		if (st_media_types[i].type == type)
			return st_media_types[i].name;

	return st_media_types[i].name;
}

enum st_media_location st_media_string_to_location(const char * location) {
	if (location == NULL)
		return st_media_location_unknown;

	unsigned int i;
	for (i = 0; st_media_locations[i].location != st_media_location_unknown; i++)
		if (!strcasecmp(location, st_media_locations[i].name))
			return st_media_locations[i].location;

	return st_media_locations[i].location;
}

enum st_media_status st_media_string_to_status(const char * status) {
	if (status == NULL)
		return st_media_status_unknown;

	unsigned int i;
	for (i = 0; st_media_status[i].status != st_media_status_unknown; i++)
		if (!strcasecmp(status, st_media_status[i].name))
			return st_media_status[i].status;

	return st_media_status[i].status;
}

enum st_media_format_data_type st_media_string_to_format_data(const char * type) {
	if (type == NULL)
		return st_media_format_data_unknown;

	unsigned int i;
	for (i = 0; st_media_format_data_types[i].type != st_media_format_data_unknown; i++)
		if (st_media_format_data_types[i].name == type)
			return st_media_format_data_types[i].type;

	return st_media_format_data_types[i].type;
}

enum st_media_format_mode st_media_string_to_format_mode(const char * mode) {
	if (mode == NULL)
		return st_media_format_mode_unknown;

	unsigned int i;
	for (i = 0; st_media_format_modes[i].mode != st_media_format_mode_unknown; i++)
		if (!strcasecmp(mode, st_media_format_modes[i].name))
			return st_media_format_modes[i].mode;

	return st_media_format_modes[i].mode;
}

enum st_media_type st_media_string_to_type(const char * type) {
	if (type == NULL)
		return st_media_type_unknown;

	unsigned int i;
	for (i = 0; st_media_types[i].type != st_media_type_unknown; i++)
		if (!strcasecmp(type, st_media_types[i].name))
			return st_media_types[i].type;

	return st_media_types[i].type;
}


static void st_media_add(struct st_media * media) {
	void * new_addr = realloc(st_medias, (st_media_nb_medias + 1) * sizeof(struct st_media *));
	if (new_addr != NULL) {
		st_medias = new_addr;
		st_medias[st_media_nb_medias] = media;
		st_media_nb_medias++;
	}
}

ssize_t st_media_get_available_size(struct st_media * media) {
	if (media == NULL || media->format == NULL)
		return 0;

	return media->format->capacity - media->end_position * media->block_size;
}

struct st_media * st_media_get_by_job(struct st_job * job, struct st_database_connection * connection) {
	if (job == NULL)
		return NULL;

	pthread_mutex_lock(&st_media_lock);

	struct st_media * media = st_media_retrieve(connection, job, NULL, NULL, NULL);

	unsigned int i;
	for (i = 0; i < st_media_nb_medias && media != NULL; i++)
		if (!strcmp(media->medium_serial_number, st_medias[i]->medium_serial_number)) {
			free(media->label);
			free(media->medium_serial_number);
			free(media->name);
			free(media->db_data);
			free(media);

			media = st_medias[i];

			pthread_mutex_unlock(&st_media_lock);
			return media;
		}

	if (media != NULL)
		st_media_add(media);

	pthread_mutex_unlock(&st_media_lock);

	return media;
}

struct st_media * st_media_get_by_label(const char * label) {
	if (label == NULL)
		return NULL;

	pthread_mutex_lock(&st_media_lock);

	struct st_media * media = NULL;
	unsigned int i;
	for (i = 0; i < st_media_nb_medias && media == NULL; i++)
		if (!strcasecmp(label, st_medias[i]->label))
			media = st_medias[i];

	if (media == NULL) {
		media = st_media_retrieve(NULL, NULL, NULL, NULL, label);

		if (media != NULL)
			st_media_add(media);
	}

	pthread_mutex_unlock(&st_media_lock);

	return media;
}

struct st_media * st_media_get_by_medium_serial_number(const char * medium_serial_number) {
	if (medium_serial_number == NULL)
		return NULL;

	pthread_mutex_lock(&st_media_lock);

	struct st_media * media = NULL;
	unsigned int i;
	for (i = 0; i < st_media_nb_medias && media == NULL; i++)
		if (!strcmp(medium_serial_number, st_medias[i]->medium_serial_number))
			media = st_medias[i];

	if (media == NULL) {
		media = st_media_retrieve(NULL, NULL, NULL, medium_serial_number, NULL);

		if (media != NULL)
			st_media_add(media);
	}

	pthread_mutex_unlock(&st_media_lock);

	return media;
}

struct st_media * st_media_get_by_uuid(const char * uuid) {
	if (uuid == NULL)
		return NULL;

	pthread_mutex_lock(&st_media_lock);

	struct st_media * media = NULL;
	unsigned int i;
	for (i = 0; i < st_media_nb_medias && media == NULL; i++)
		if (!strcmp(uuid, st_medias[i]->uuid))
			media = st_medias[i];

	if (media == NULL) {
		media = st_media_retrieve(NULL, NULL, uuid, NULL, NULL);

		if (media != NULL)
			st_media_add(media);
	}

	pthread_mutex_unlock(&st_media_lock);

	return media;
}

int st_media_read_header(struct st_drive * drive) {
	if (drive == NULL)
		return 1;

	struct st_media * media = drive->slot->media;
	if (media == NULL)
		return 1;

	struct st_stream_reader * reader = drive->ops->get_raw_reader(drive, 0);
	if (reader == NULL) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "[%s | %s | #%td]: failed to read media", drive->vendor, drive->model, drive - drive->changer->drives);
		return 1;
	}

	char buffer[512];
	ssize_t nb_read = reader->ops->read(reader, buffer, 512);
	reader->ops->close(reader);
	reader->ops->free(reader);

	if (nb_read <= 0) {
		media->status = st_media_status_new;
		media->nb_volumes = 0;
		return 0;
	}

	// M | STone (v0.1)
	// M | Tape format: version=1
	// O | Label: A0000002
	// M | Tape id: uuid=f680dd48-dd3e-4715-8ddc-a90d3e708914
	// M | Pool: name=Foo, uuid=07117f1a-2b13-11e1-8bcb-80ee73001df6
	// M | Block size: 32768
	// M | Checksum: crc32=1eb6931d
	char stone_version[65];
	int tape_format_version = 0;
	int nb_parsed = 0;
	if (sscanf(buffer, "STone (%64[^)])\nTape format: version=%d\n%n", stone_version, &tape_format_version, &nb_parsed) == 2) {
		char uuid[37];
		char name[65];
		char pool_id[37];
		char pool_name[65];
		ssize_t block_size;
		char checksum_name[12];
		char checksum_value[64];

		int nb_parsed2 = 0;
		int ok = 1;
		bool has_label = false;

		if (sscanf(buffer + nb_parsed, "Label: %36s\n%n", name, &nb_parsed2) == 1) {
			nb_parsed += nb_parsed2;
			has_label = true;
		}

		if (ok && sscanf(buffer + nb_parsed, "Tape id: uuid=%37s\n%n", uuid, &nb_parsed2) == 1)
			nb_parsed += nb_parsed2;
		else
			ok = 0;

		if (ok && sscanf(buffer + nb_parsed, "Pool: name=%65[^,], uuid=%36s\n%n", pool_name, pool_id, &nb_parsed2) == 2)
			nb_parsed += nb_parsed2;
		else
			ok = 0;

		if (ok && sscanf(buffer + nb_parsed, "Block size: %zd\n%n", &block_size, &nb_parsed2) == 1)
			nb_parsed += nb_parsed2;
		else
			ok = 0;

		if (ok)
			ok = sscanf(buffer + nb_parsed, "Checksum: %11[^=]=%64s\n", checksum_name, checksum_value) == 2;

		if (ok) {
			char * digest = st_checksum_compute(checksum_name, buffer, nb_parsed);
			ok = digest != NULL && !strcmp(checksum_value, digest);
			free(digest);

			media->pool = st_pool_get_by_uuid(pool_id);
			// TODO: create pool
			// if (media->pool == NULL)
			//	media->pool = st_pool_create(pool_id, pool_name, media->format);
		}

		if (ok) {
			st_log_write_all(st_log_level_debug, st_log_type_drive, "Found STone header in media with (uuid=%s, label=%s, blocksize=%zd)", uuid, name, block_size);

			strcpy(media->uuid, uuid);
			if (has_label)
				media->label = strdup(name);
			media->name = strdup(name);
			media->status = st_media_status_in_use;
			media->block_size = block_size;
		} else
			media->status = st_media_status_foreign;
	} else {
		media->status = st_media_status_foreign;
	}

	return 0;
}

static struct st_media * st_media_retrieve(struct st_database_connection * connection, struct st_job * job, const char * uuid, const char * medium_serial_number, const char * label) {
	struct st_database * db = NULL;
	struct st_database_config * config = NULL;

	if (connection == NULL)
		db = st_database_get_default_driver();
	if (db != NULL)
		config = db->ops->get_default_config();
	if (config != NULL)
		connection = config->ops->connect(config);

	struct st_media * media = NULL;
	if (connection != NULL)
		media = connection->ops->get_media(connection, job, uuid, medium_serial_number, label);

	if (db != NULL) {
		connection->ops->close(connection);
		connection->ops->free(connection);
	}

	if (media != NULL)
		media->lock = st_ressource_new();

	return media;
}

int st_media_write_header(struct st_drive * drive, struct st_pool * pool) {
	if (drive == NULL)
		return 1;

	struct st_media * media = drive->slot->media;
	if (media == NULL) {
		st_log_write_all(st_log_level_warning, st_log_type_drive, "Try to write a tape header to a drive without tape");
		return 1;
	}

	if (*media->uuid == '\0') {
		uuid_t id;
		uuid_generate(id);
		uuid_unparse_lower(id, media->uuid);
	}

	// check for block size
	if (media->block_size == 0)
		media->block_size = media->format->block_size;

	// M | STone (v0.1)
	// M | Tape format: version=1
	// O | Label: A0000002
	// M | Tape id: uuid=f680dd48-dd3e-4715-8ddc-a90d3e708914
	// M | Pool: name=Foo, uuid=07117f1a-2b13-11e1-8bcb-80ee73001df6
	// M | Block size: 32768
	// M | Checksum: crc32=1eb6931d
	char * hdr = strdup("STone (" STONE_VERSION ")\nTape format: version=1\n"), * hdr2;
	ssize_t shdr = strlen(hdr), shdr2;

	/**
	 * With stand-alone tape drive, we cannot retreive tape label.
	 * In this case, media's label is NULL.
	 * That why, Label field in header is optionnal
	 */
	if (media->label != NULL) {
		shdr2 = asprintf(&hdr2, "Label: %s\n", media->label);
		void * new_addr = realloc(hdr, shdr + shdr2 + 1);
		if (new_addr == NULL) {
			free(hdr);
			return 1;
		}
		hdr = new_addr;
		strcpy(hdr + shdr, hdr2);
		shdr += shdr2;
		free(hdr2);
		hdr2 = NULL;
	}

	shdr2 = asprintf(&hdr2, "Tape id: uuid=%s\nPool: name=%s, uuid=%s\nBlock size: %zd\n", media->uuid, pool->name, pool->uuid, media->block_size);
	void * new_addr = realloc(hdr, shdr + shdr2 + 1);
	if (new_addr == NULL) {
		free(hdr);
		return 1;
	}
	hdr = new_addr;
	strcpy(hdr + shdr, hdr2);
	shdr += shdr2;
	free(hdr2);
	hdr2 = NULL;

	// compute header's checksum
	char * digest = st_checksum_compute("crc32", hdr, shdr);
	if (digest == NULL) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "Failed to compute digest before write media header");
		free(hdr);
		return 3;
	}

	ssize_t sdigest = strlen(digest);
	new_addr = realloc(hdr, shdr + sdigest + 18);
	if (new_addr == NULL) {
		free(hdr);
		return 1;
	}
	hdr = new_addr;
	shdr += snprintf(hdr + shdr, sdigest + 18, "Checksum: crc32=%s\n", digest);

	struct st_stream_writer * writer = drive->ops->get_raw_writer(drive, 0);
	ssize_t nb_write = writer->ops->write(writer, hdr, shdr);
	int failed = writer->ops->close(writer);
	writer->ops->free(writer);

	free(hdr);

	if (nb_write == shdr && !failed) {
		media->status = st_media_status_in_use;
		media->pool = pool;

		st_log_write_all(st_log_level_info, st_log_type_drive, "Write a media header: succed");
	} else {
		media->status = st_media_status_error;

		st_log_write_all(st_log_level_info, st_log_type_drive, "Write a media header: failed");
	}

	return failed;
}


struct st_media_format * st_media_format_get_by_density_code(unsigned char density_code, enum st_media_format_mode mode) {
	pthread_mutex_lock(&st_media_format_lock);

	struct st_media_format * media_format = NULL;
	unsigned int i;
	for (i = 0; i < st_media_format_nb_formats && media_format == NULL; i++)
		if (st_media_formats[i]->density_code == density_code && st_media_formats[i]->mode == mode)
			media_format = st_media_formats[i];

	if (media_format == NULL && st_media_format_retrieve(&media_format, density_code, mode)) {
		void * new_addr = realloc(st_media_formats, (st_media_format_nb_formats + 1) * sizeof(struct st_media_format *));
		if (new_addr != NULL) {
			st_media_formats = new_addr;
			st_media_formats[st_media_format_nb_formats] = media_format;
			st_media_format_nb_formats++;
		}
	}

	pthread_mutex_unlock(&st_media_format_lock);

	return media_format;
}

static int st_media_format_retrieve(struct st_media_format ** media_format, unsigned char density_code, enum st_media_format_mode mode) {
	struct st_database * db = st_database_get_default_driver();
	struct st_database_config * config = NULL;
	struct st_database_connection * connect = NULL;

	if (db != NULL)
		config = db->ops->get_default_config();

	if (config != NULL)
		connect = config->ops->connect(config);

	if (connect != NULL) {
		struct st_media_format format;
		bzero(&format, sizeof(struct st_media_format));

		short ok = 0;
		if (!connect->ops->get_media_format(connect, &format, density_code, mode)) {
			if (!*media_format)
				*media_format = malloc(sizeof(struct st_media_format));
			memcpy(*media_format, &format, sizeof(struct st_media_format));
			ok = 1;
		}

		connect->ops->close(connect);
		connect->ops->free(connect);

		return ok;
	}

	return 0;
}


struct st_pool * st_pool_get_by_job(struct st_job * job, struct st_database_connection * connection) {
	if (job == NULL)
		return NULL;

	pthread_mutex_lock(&st_pool_lock);

	struct st_pool * pool = st_pool_retreive(connection, job, NULL);

	unsigned int i;
	for (i = 0; i < st_pool_nb_pools; i++)
		if (!strcmp(pool->uuid, st_pools[i]->uuid)) {
			free(pool->name);
			free(pool);

			pool = st_pools[i];

			pthread_mutex_unlock(&st_pool_lock);
			return pool;
		}

	if (pool != NULL) {
		void * new_addr = realloc(st_pools, (st_pool_nb_pools + 1) * sizeof(struct st_pool *));
		if (new_addr != NULL) {
			st_pools = new_addr;
			st_pools[st_pool_nb_pools] = pool;
			st_pool_nb_pools++;
		}
	}

	pthread_mutex_unlock(&st_pool_lock);

	return pool;
}

struct st_pool * st_pool_get_by_uuid(const char * uuid) {
	if (uuid == NULL)
		return NULL;

	pthread_mutex_lock(&st_pool_lock);

	struct st_pool * pool = NULL;
	unsigned int i;
	for (i = 0; i < st_pool_nb_pools && pool == NULL; i++)
		if (!strcmp(uuid, st_pools[i]->uuid))
			pool = st_pools[i];

	if (pool == NULL) {
		pool = st_pool_retreive(NULL, NULL, uuid);

		if (pool != NULL) {
			void * new_addr = realloc(st_pools, (st_pool_nb_pools + 1) * sizeof(struct st_pool *));
			if (new_addr != NULL) {
				st_pools = new_addr;
				st_pools[st_pool_nb_pools] = pool;
				st_pool_nb_pools++;
			}
		}
	}

	pthread_mutex_unlock(&st_pool_lock);

	return pool;
}

static struct st_pool * st_pool_retreive(struct st_database_connection * connection, struct st_job * job, const char * uuid) {
	struct st_database * db = NULL;
	struct st_database_config * config = NULL;

	if (connection == NULL)
		db = st_database_get_default_driver();
	if (db != NULL)
		config = db->ops->get_default_config();
	if (config != NULL)
		connection = config->ops->connect(config);

	struct st_pool * pool = NULL;
	if (connection != NULL)
		pool = connection->ops->get_pool(connection, job, uuid);

	if (db != NULL) {
		connection->ops->close(connection);
		connection->ops->free(connection);
	}

	return pool;
}

