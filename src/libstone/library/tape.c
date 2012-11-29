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
*  Last modified: Wed, 28 Nov 2012 19:22:10 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// pthread_mutex_lock, pthread_mutex_unlock
#include <pthread.h>
// bool
#include <stdbool.h>
// sscanf
#include <stdio.h>
// malloc, realloc
#include <stdlib.h>
// sscanf, snprintf
#include <stdio.h>
// strcmp, strcpy
#include <string.h>
// time
#include <time.h>
// uuid
#include <uuid/uuid.h>

#include <stone/checksum.h>
#include <stone/database.h>
#include <stone/io.h>
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/tape.h>
#include <stone/log.h>

static const struct st_tape_format_data_type2 {
	char * name;
	enum st_tape_format_data_type type;
} st_tape_format_data_types[] = {
	{ "audio",    ST_TAPE_FORMAT_DATA_AUDIO },
	{ "cleaning", ST_TAPE_FORMAT_DATA_CLEANING },
	{ "data",     ST_TAPE_FORMAT_DATA_DATA },
	{ "video",    ST_TAPE_FORMAT_DATA_VIDEO },

	{ 0, ST_TAPE_FORMAT_DATA_UNKNOWN },
};

static const struct st_tape_format_mode2 {
	char * name;
	enum st_tape_format_mode mode;
} st_tape_format_modes[] = {
	{ "disk",    ST_TAPE_FORMAT_MODE_DISK },
	{ "linear",  ST_TAPE_FORMAT_MODE_LINEAR },
	{ "optical", ST_TAPE_FORMAT_MODE_OPTICAL },

	{ 0, ST_TAPE_FORMAT_MODE_UNKNOWN },
};

static const struct st_tape_location2 {
	char * name;
	enum st_tape_location location;
} st_tape_locations[] = {
	{ "in drive", ST_TAPE_LOCATION_INDRIVE },
	{ "offline",  ST_TAPE_LOCATION_OFFLINE },
	{ "online",   ST_TAPE_LOCATION_ONLINE },

	{ 0, ST_TAPE_LOCATION_UNKNOWN },
};

static const struct st_tape_status2 {
	char * name;
	enum st_tape_status status;
} st_tape_status[] = {
	{ "erasable",          ST_TAPE_STATUS_ERASABLE },
	{ "error",             ST_TAPE_STATUS_ERROR },
	{ "foreign",           ST_TAPE_STATUS_FOREIGN },
	{ "in use",            ST_TAPE_STATUS_IN_USE },
	{ "locked",            ST_TAPE_STATUS_LOCKED },
	{ "needs replacement", ST_TAPE_STATUS_NEEDS_REPLACEMENT },
	{ "new",               ST_TAPE_STATUS_NEW },
	{ "pooled",            ST_TAPE_STATUS_POOLED },

	{ 0, ST_TAPE_STATUS_UNKNOWN },
};


static void st_tape_retrieve(struct st_tape ** tape, long id, const char * medium_serial_number, const char * uuid, const char * label);
static void st_tape_format_retrieve(struct st_tape_format ** format, long id, unsigned char density_code);
static struct st_pool * st_pool_create(const char * uuid, const char * name, struct st_tape_format * format);
static void st_pool_retrieve(struct st_pool ** pool, long id, const char * uuid);

static pthread_mutex_t st_tape_lock = PTHREAD_MUTEX_INITIALIZER;
static struct st_tape ** st_tape_tapes = 0;
static unsigned int st_tape_nb_tapes = 0;

static pthread_mutex_t st_tape_format_lock = PTHREAD_MUTEX_INITIALIZER;
static struct st_tape_format ** st_tape_formats = 0;
static unsigned int st_tape_format_nb_formats = 0;

static pthread_mutex_t st_pool_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static struct st_pool ** st_pools = 0;
static unsigned int st_pool_nb_pools = 0;


const char * st_tape_format_data_to_string(enum st_tape_format_data_type type) {
	const struct st_tape_format_data_type2 * ptr;
	for (ptr = st_tape_format_data_types; ptr->name; ptr++)
		if (ptr->type == type)
			return ptr->name;
	return "unknown";
}

const char * st_tape_format_mode_to_string(enum st_tape_format_mode mode) {
	const struct st_tape_format_mode2 * ptr;
	for (ptr = st_tape_format_modes; ptr->name; ptr++)
		if (ptr->mode == mode)
			return ptr->name;
	return "unknown";
}

const char * st_tape_location_to_string(enum st_tape_location location) {
	const struct st_tape_location2 * ptr;
	for (ptr = st_tape_locations; ptr->name; ptr++)
		if (ptr->location == location)
			return ptr->name;
	return "unknown";
}

const char * st_tape_status_to_string(enum st_tape_status status) {
	const struct st_tape_status2 * ptr;
	for (ptr = st_tape_status; ptr->name; ptr++)
		if (ptr->status == status)
			return ptr->name;
	return "unknown";
}

enum st_tape_location st_tape_string_to_location(const char * location) {
	const struct st_tape_location2 * ptr;
	for (ptr = st_tape_locations; ptr->name; ptr++)
		if (!strcmp(location, ptr->name))
			return ptr->location;
	return ptr->location;
}

enum st_tape_status st_tape_string_to_status(const char * status) {
	const struct st_tape_status2 * ptr;
	for (ptr = st_tape_status; ptr->name; ptr++)
		if (!strcmp(status, ptr->name))
			return ptr->status;
	return ptr->status;
}

enum st_tape_format_data_type st_tape_string_to_format_data(const char * type) {
	const struct st_tape_format_data_type2 * ptr;
	for (ptr = st_tape_format_data_types; ptr->name; ptr++)
		if (!strcmp(type, ptr->name))
			return ptr->type;
	return ptr->type;
}

enum st_tape_format_mode st_tape_string_to_format_mode(const char * mode) {
	const struct st_tape_format_mode2 * ptr;
	for (ptr = st_tape_format_modes; ptr->name; ptr++)
		if (!strcmp(mode, ptr->name))
			return ptr->mode;
	return ptr->mode;
}


struct st_tape * st_tape_get_by_id(long id) {
	pthread_mutex_lock(&st_tape_lock);

	struct st_tape * tape = 0;
	unsigned int i;
	for (i = 0; i < st_tape_nb_tapes; i++)
		if (id == st_tape_tapes[i]->id) {
			tape = st_tape_tapes[i];
			break;
		}

	if (!tape)
		st_tape_retrieve(&tape, id, NULL, NULL, NULL);

	pthread_mutex_unlock(&st_tape_lock);

	return tape;
}

struct st_tape * st_tape_get_by_label(const char * label) {
	if (!label)
		return 0;

	pthread_mutex_lock(&st_tape_lock);

	struct st_tape * tape = 0;
	unsigned int i;
	for (i = 0; i < st_tape_nb_tapes; i++)
		if (!strcmp(label, st_tape_tapes[i]->label)) {
			tape = st_tape_tapes[i];
			break;
		}

	if (!tape)
		st_tape_retrieve(&tape, -1, NULL, NULL, label);

	pthread_mutex_unlock(&st_tape_lock);

	return tape;
}

struct st_tape * st_tape_get_by_medium_serial_number(const char * medium_serial_number) {
	if (!medium_serial_number)
		return 0;

	pthread_mutex_lock(&st_tape_lock);

	struct st_tape * tape = 0;
	unsigned int i;
	for (i = 0; i < st_tape_nb_tapes; i++)
		if (!strcmp(medium_serial_number, st_tape_tapes[i]->medium_serial_number)) {
			tape = st_tape_tapes[i];
			break;
		}

	if (!tape)
		st_tape_retrieve(&tape, -1, medium_serial_number, NULL, NULL);

	pthread_mutex_unlock(&st_tape_lock);

	return tape;
}

struct st_tape * st_tape_get_by_uuid(const char * uuid) {
	pthread_mutex_lock(&st_tape_lock);

	struct st_tape * tape = 0;
	unsigned int i;
	for (i = 0; i < st_tape_nb_tapes; i++)
		if (!strcmp(uuid, st_tape_tapes[i]->uuid)) {
			tape = st_tape_tapes[i];
			break;
		}

	if (!tape)
		st_tape_retrieve(&tape, -1, NULL, uuid, NULL);

	pthread_mutex_unlock(&st_tape_lock);

	return tape;
}

struct st_tape * st_tape_new(struct st_drive * dr) {
	pthread_mutex_lock(&st_tape_lock);

	struct st_tape * tape = dr->slot->tape = malloc(sizeof(struct st_tape));
	bzero(tape, sizeof(struct st_tape));

	tape->id = -1;
	*tape->uuid = '\0';
	strcpy(tape->label, dr->slot->volume_name);
	*tape->medium_serial_number = '\0';
	strcpy(tape->name, dr->slot->volume_name);
	tape->status = ST_TAPE_STATUS_UNKNOWN;
	tape->location = ST_TAPE_LOCATION_INDRIVE;
	tape->first_used = time(0);
	tape->load_count = 1;
	tape->read_count = 0;
	tape->write_count = 0;
	tape->end_position = 0;
	tape->nb_files = 0;
	tape->has_partition = 0;
	tape->format = st_tape_format_get_by_density_code(dr->density_code);
	tape->block_size = 0;
	tape->use_before = tape->first_used + tape->format->life_span;
	tape->pool = 0;
	tape->mam_ok = 0;

	dr->ops->rewind_tape(dr);
	ssize_t block_size = dr->ops->get_block_size(dr);
	tape->block_size = block_size;

	if (tape->format->support_mam)
		dr->ops->read_mam(dr);

	char buffer[512];
	struct st_stream_reader * reader = dr->ops->get_reader(dr);
	ssize_t nb_read = reader->ops->read(reader, buffer, 512);
	reader->ops->close(reader);
	reader->ops->free(reader);

	if (nb_read <= 0) {
		tape->status = ST_TAPE_STATUS_NEW;
		tape->nb_files = 0;

		unsigned int i, found = 0;
		for (i = 0; i < st_tape_nb_tapes; i++)
			if (!strcmp(tape->label, st_tape_tapes[i]->label)) {
				free(tape);
				tape = st_tape_tapes[i];
				found = 1;
				break;
			}

		if (!found) {
			st_tape_tapes = realloc(st_tape_tapes, (st_tape_nb_tapes + 1) * sizeof(struct st_tape *));
			st_tape_tapes[st_tape_nb_tapes] = tape;
			st_tape_nb_tapes++;
		}

		pthread_mutex_unlock(&st_tape_lock);

		return tape;
	}

	// M | STone (v0.1)
	// M | Tape format: version=1
	// O | Label: A0000002
	// M | Tape id: uuid=f680dd48-dd3e-4715-8ddc-a90d3e708914
	// M | Pool: name=Foo, uuid=07117f1a-2b13-11e1-8bcb-80ee73001df6
	// M | Block size: 32768
	// M | Checksum: crc32=1eb6931d
	char stone_version[33];
	int tape_format_version = 0;
	int nb_parsed = 0;
	if (sscanf(buffer, "STone (v%32[^)])\nTape format: version=%d\n%n", stone_version, &tape_format_version, &nb_parsed) == 2) {
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
			ok = digest && !strcmp(checksum_value, digest);

			if (digest)
				free(digest);

			tape->pool = st_pool_get_by_uuid(pool_id);
			if (!tape->pool)
				tape->pool = st_pool_create(pool_id, pool_name, tape->format);
		}

		if (ok) {
			st_log_write_all(st_log_level_debug, st_log_type_drive, "Found STone header in tape with (uuid=%s, label=%s, blocksize=%zd)", uuid, name, block_size);

			strcpy(tape->uuid, uuid);
			if (has_label)
				strcpy(tape->label, name);
			strcpy(tape->name, name);
			tape->status = ST_TAPE_STATUS_IN_USE;
			tape->block_size = block_size;
		} else
			tape->status = ST_TAPE_STATUS_FOREIGN;
	} else {
		tape->status = ST_TAPE_STATUS_FOREIGN;
	}

	// compute how many files are on the tape
	dr->ops->eod(dr);
	tape->nb_files = dr->nb_files;

	unsigned int i, found = 0;
	for (i = 0; i < st_tape_nb_tapes; i++)
		if (!strcmp(tape->label, st_tape_tapes[i]->label)) {
			free(tape);
			tape = st_tape_tapes[i];
			found = 1;
			break;
		}

	if (!found) {
		st_tape_tapes = realloc(st_tape_tapes, (st_tape_nb_tapes + 1) * sizeof(struct st_tape *));
		st_tape_tapes[st_tape_nb_tapes] = tape;
		st_tape_nb_tapes++;
	}

	pthread_mutex_unlock(&st_tape_lock);

	return tape;
}

void st_tape_retrieve(struct st_tape ** tape, long id, const char * medium_serial_number, const char * uuid, const char * label) {
	struct st_database * db = st_db_get_default_db();
	struct st_database_connection * con = db->ops->connect(db, 0);
	if (con) {
		*tape = malloc(sizeof(struct st_tape));
		bzero(*tape, sizeof(struct st_tape));

		if (con->ops->get_tape(con, *tape, id, medium_serial_number, uuid, label)) {
			free(*tape);
			*tape = 0;
		} else {
			st_tape_tapes = realloc(st_tape_tapes, (st_tape_nb_tapes + 1) * sizeof(struct st_tape *));
			st_tape_tapes[st_tape_nb_tapes] = *tape;
			st_tape_nb_tapes++;
		}

		con->ops->close(con);
		con->ops->free(con);
		free(con);
	}

	if (*tape != NULL && (*tape)->available_block == 0 && (*tape)->format != NULL)
		(*tape)->available_block = (*tape)->format->capacity / (*tape)->block_size - (*tape)->end_position;
}

void st_tape_sync(struct st_tape * tape) {
	if (tape == NULL)
		return;

	struct st_database * db = st_db_get_default_db();
	struct st_database_connection * con = db->ops->connect(db, 0);
	if (con) {
		con->ops->sync_tape(con, tape);

		con->ops->close(con);
		con->ops->free(con);
		free(con);
	}
}

int st_tape_write_header(struct st_drive * dr, struct st_pool * pool) {
	if (!dr)
		return 1;

	struct st_tape * tape = dr->slot->tape;
	if (!tape) {
		st_log_write_all(st_log_level_warning, st_log_type_drive, "Try to write a tape header to a drive without tape");
		return 2;
	}

	dr->ops->rewind_tape(dr);

	// check for uuid
	char uuid[37];
	if (*tape->uuid) {
		strncpy(uuid, tape->uuid, 37);
	} else {
		uuid_t id;
		uuid_generate(id);
		uuid_unparse_lower(id, uuid);
	}

	// check for block size
	if (tape->block_size == 0)
		tape->block_size = tape->format->block_size;

	// M | STone (v0.1)
	// M | Tape format: version=1
	// O | Label: A0000002
	// M | Tape id: uuid=f680dd48-dd3e-4715-8ddc-a90d3e708914
	// M | Pool: name=Foo, uuid=07117f1a-2b13-11e1-8bcb-80ee73001df6
	// M | Block size: 32768
	// M | Checksum: crc32=1eb6931d

	char * hdr = 0, * hdr2 = 0;
	ssize_t shdr = asprintf(&hdr, "STone (%s)\nTape format: version=1\n", STONE_VERSION), shdr2;
	if (*tape->label) {
		shdr2 = asprintf(&hdr2, "Label: %s\n", tape->label);
		hdr = realloc(hdr, shdr + shdr2 + 1);
		strcpy(hdr + shdr, hdr2);
		shdr += shdr2;
		free(hdr2);
		hdr2 = 0;
	}
	shdr2 = asprintf(&hdr2, "Tape id: uuid=%s\nPool: name=%s, uuid=%s\nBlock size: %zd\n", uuid, pool->name, pool->uuid, tape->block_size);
	hdr = realloc(hdr, shdr + shdr2 + 1);
	strcpy(hdr + shdr, hdr2);
	shdr += shdr2;
	free(hdr2);
	hdr2 = 0;

	char * digest = st_checksum_compute("crc32", hdr, shdr);

	if (!digest) {
		free(hdr);
		return 3;
	}

	ssize_t sdigest = strlen(digest);

	hdr = realloc(hdr, shdr + sdigest + 18);
	shdr += snprintf(hdr + shdr, sdigest + 18, "Checksum: crc32=%s\n", digest);

	struct st_stream_writer * w = dr->ops->get_writer(dr);
	ssize_t nb_write = w->ops->write(w, hdr, shdr);
	int failed = w->ops->close(w);
	w->ops->free(w);

	free(hdr);

	if (nb_write == shdr && !failed) {
		strncpy(tape->uuid, uuid, 37);
		tape->status = ST_TAPE_STATUS_IN_USE;
		tape->pool = pool;

		st_log_write_all(st_log_level_info, st_log_type_drive, "Write a tape header: succed");
	} else {
		tape->status = ST_TAPE_STATUS_ERROR;

		st_log_write_all(st_log_level_info, st_log_type_drive, "Write a tape header: failed");
	}

	return failed;
}


struct st_tape_format * st_tape_format_get_by_id(long id) {
	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	pthread_mutex_lock(&st_tape_format_lock);

	unsigned int i;
	struct st_tape_format * format = 0;
	for (i = 0; i < st_tape_format_nb_formats; i++)
		if (id == st_tape_formats[i]->id)
			format = st_tape_formats[i];

	if (!format)
		st_tape_format_retrieve(&format, id, 0);

	pthread_mutex_unlock(&st_tape_format_lock);
	pthread_setcancelstate(old_state, 0);

	return format;
}

struct st_tape_format * st_tape_format_get_by_density_code(unsigned char density_code) {
	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	pthread_mutex_lock(&st_tape_format_lock);

	unsigned int i;
	struct st_tape_format * format = 0;
	for (i = 0; i < st_tape_format_nb_formats; i++)
		if (st_tape_formats[i]->density_code == density_code)
			format = st_tape_formats[i];

	if (!format)
		st_tape_format_retrieve(&format, -1, density_code);

	pthread_mutex_unlock(&st_tape_format_lock);
	pthread_setcancelstate(old_state, 0);

	return format;
}

void st_tape_format_retrieve(struct st_tape_format ** format, long id, unsigned char density_code) {
	struct st_database * db = st_db_get_default_db();
	struct st_database_connection * con = db->ops->connect(db, 0);
	if (con) {
		st_tape_formats = realloc(st_tape_formats, (st_tape_format_nb_formats + 1) * sizeof(struct st_tape_format *));
		st_tape_formats[st_tape_format_nb_formats] = malloc(sizeof(struct st_tape_format));
		bzero(st_tape_formats[st_tape_format_nb_formats], sizeof(struct st_tape_format));

		if (con->ops->get_tape_format(con, st_tape_formats[st_tape_format_nb_formats], id, density_code)) {
			free(st_tape_formats[st_tape_format_nb_formats]);
			st_tape_formats = realloc(st_tape_formats, st_tape_format_nb_formats * sizeof(struct st_tape_format));
		} else {
			*format = st_tape_formats[st_tape_format_nb_formats];
			st_tape_format_nb_formats++;
		}

		con->ops->close(con);
		con->ops->free(con);
		free(con);
	}
}


struct st_pool * st_pool_create(const char * uuid, const char * name, struct st_tape_format * format) {
	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	pthread_mutex_lock(&st_pool_lock);

	struct st_pool * pool = st_pool_get_by_uuid(uuid);
	if (!pool) {
		struct st_database * db = st_db_get_default_db();
		struct st_database_connection * con = db->ops->connect(db, 0);
		if (con) {
			pool = malloc(sizeof(struct st_pool));
			bzero(pool, sizeof(struct st_pool));
			strncpy(pool->uuid, uuid, 37);
			strncpy(pool->name, name, 64);
			pool->format = format;

			if (con->ops->create_pool(con, pool)) {
				free(pool);
				pool = 0;
			}

			con->ops->close(con);
			con->ops->free(con);
			free(con);
		}
	}

	if (pool) {
		st_pools = realloc(st_pools, (st_pool_nb_pools + 1) * sizeof(struct st_pool *));
		st_pools[st_pool_nb_pools] = pool;
		st_pool_nb_pools++;
	}

	pthread_mutex_unlock(&st_pool_lock);
	pthread_setcancelstate(old_state, 0);

	return pool;
}

struct st_pool * st_pool_get_by_id(long id) {
	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	pthread_mutex_lock(&st_pool_lock);

	unsigned int i;
	struct st_pool * pool = 0;
	for (i = 0; i < st_pool_nb_pools; i++)
		if (id == st_pools[i]->id) {
			pool = st_pools[i];
			break;
		}

	if (!pool)
		st_pool_retrieve(&pool, id, NULL);

	pthread_mutex_unlock(&st_pool_lock);
	pthread_setcancelstate(old_state, 0);

	return pool;
}

struct st_pool * st_pool_get_by_uuid(const char * uuid) {
	pthread_mutex_lock(&st_pool_lock);

	unsigned int i;
	struct st_pool * pool = 0;
	for (i = 0; i < st_pool_nb_pools; i++)
		if (!strcmp(uuid, st_pools[i]->uuid)) {
			pool = st_pools[i];
			break;
		}

	if (!pool)
		st_pool_retrieve(&pool, -1, uuid);

	pthread_mutex_unlock(&st_pool_lock);

	return pool;
}

void st_pool_retrieve(struct st_pool ** pool, long id, const char * uuid) {
	struct st_database * db = st_db_get_default_db();
	struct st_database_connection * con = db->ops->connect(db, 0);
	if (con) {
		st_pools = realloc(st_pools, (st_pool_nb_pools + 1) * sizeof(struct st_pool *));
		st_pools[st_pool_nb_pools] = malloc(sizeof(struct st_pool));
		bzero(st_pools[st_pool_nb_pools], sizeof(struct st_pool));

		if (con->ops->get_pool(con, st_pools[st_pool_nb_pools], id, uuid)) {
			free(st_pools[st_pool_nb_pools]);
			st_pools = realloc(st_pools, st_pool_nb_pools * sizeof(struct st_pool *));
		} else {
			*pool = st_pools[st_pool_nb_pools];
			st_pool_nb_pools++;
		}

		con->ops->close(con);
		con->ops->free(con);
		free(con);
	}
}

int st_pool_sync(struct st_pool * pool) {
	if (!pool)
		return 0;

	struct st_database * db = st_db_get_default_db();
	struct st_database_connection * con = db->ops->connect(db, 0);

	int ok = con->ops->sync_pool(con, pool);

	con->ops->close(con);
	con->ops->free(con);
	free(con);

	return ok;
}

