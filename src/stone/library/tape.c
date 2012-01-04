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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Wed, 04 Jan 2012 17:52:56 +0100                         *
\*************************************************************************/

// pthread_mutex_lock, pthread_mutex_unlock
#include <pthread.h>
// sscanf
#include <stdio.h>
// malloc, realloc
#include <stdlib.h>
// strcmp, strcpy
#include <string.h>
// time
#include <time.h>

#include <stone/checksum.h>
#include <stone/database.h>
#include <stone/io.h>
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/tape.h>

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

static struct st_tape ** st_tape_tapes = 0;
static unsigned int st_tape_nb_tapes = 0;
static struct st_tape_format ** st_tape_formats = 0;
static unsigned int st_tape_format_nb_formats = 0;
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


struct st_tape * st_tape_new(struct st_drive * dr) {
	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&lock);

	struct st_tape * tape = dr->slot->tape = malloc(sizeof(struct st_tape));
	tape->id = -1;
	*tape->uuid = '\0';
	strcpy(tape->label, dr->slot->volume_name);
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

	dr->ops->rewind_tape(dr);
	ssize_t block_size = dr->ops->get_block_size(dr);
	tape->block_size = block_size;

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

		pthread_mutex_unlock(&lock);
		pthread_setcancelstate(old_state, 0);

		return tape;
	}

	// STone (v0.1)
	// Tape format: version=1
	// Label: A0000002
	// Tape id: uuid=f680dd48-dd3e-4715-8ddc-a90d3e708914
	// Pool: name=Foo, uuid=07117f1a-2b13-11e1-8bcb-80ee73001df6
	// Block size: 32768
	// Checksum: crc32=1eb6931d
	char stone_version[9];
	int tape_format_version = 0;
	int nb_parsed = 0;
	if (sscanf(buffer, "STone (v%8[^)])\nTape format: version=%d\n%n", stone_version, &tape_format_version, &nb_parsed) == 2) {
		char uuid[37];
		char name[65];
		char pool_id[37];
		char pool_name[65];
		ssize_t block_size;
		char checksum_name[12];
		char checksum_value[64];

		int nb_parsed2 = 0;
		int ok = 1;

		if (sscanf(buffer + nb_parsed, "Label: %36s\n%n", name, &nb_parsed2) == 1)
			nb_parsed += nb_parsed2;
		else
			ok = 0;

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

			tape->pool = st_pool_get_pool_by_uuid(pool_id, pool_name, dr->density_code);
		}

		if (ok) {
			strcpy(tape->uuid, uuid);
			strcpy(tape->label, name);
			strcpy(tape->name, name);
			tape->status = ST_TAPE_STATUS_IN_USE;
			tape->block_size = block_size;
		} else
			tape->status = ST_TAPE_STATUS_FOREIGN;
	} else {
		 tape->status = ST_TAPE_STATUS_FOREIGN;
	}

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

	pthread_mutex_unlock(&lock);
	pthread_setcancelstate(old_state, 0);

	return tape;
}


struct st_tape_format * st_tape_format_get_by_density_code(unsigned char density_code) {
	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&lock);

	unsigned int i;
	struct st_tape_format * format = 0;
	for (i = 0; i < st_tape_format_nb_formats; i++)
		if (st_tape_formats[i]->density_code == density_code)
			format = st_tape_formats[i];

	if (!format) {
		struct st_database * db = st_db_get_default_db();
		struct st_database_connection * con = db->ops->connect(db, 0);
		if (con) {
			st_tape_formats = realloc(st_tape_formats, (st_tape_format_nb_formats + 1) * sizeof(struct st_tape_format *));
			st_tape_formats[st_tape_format_nb_formats] = malloc(sizeof(struct st_tape_format));

			if (con->ops->get_tape_format(con, st_tape_formats[st_tape_format_nb_formats], density_code)) {
				free(st_tape_formats[st_tape_format_nb_formats]);
				st_tape_formats = realloc(st_tape_formats, st_tape_format_nb_formats * sizeof(struct st_tape_format));
			} else {
				format = st_tape_formats[st_tape_format_nb_formats];
				st_tape_format_nb_formats++;
			}

			con->ops->close(con);
			con->ops->free(con);
			free(con);
		}
	}

	pthread_mutex_unlock(&lock);
	pthread_setcancelstate(old_state, 0);

	return format;
}


struct st_pool * st_pool_get_pool_by_uuid(char * uuid, char * name, unsigned char density_code) {
	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&lock);

	unsigned int i;
	struct st_pool * pool = 0;
	for (i = 0; i < st_pool_nb_pools; i++)
		if (!strcmp(uuid, st_pools[i]->uuid))
			pool = st_pools[i];

	if (!pool) {
		struct st_database * db = st_db_get_default_db();
		struct st_database_connection * con = db->ops->connect(db, 0);
		if (con) {
			st_pools = realloc(st_pools, (st_pool_nb_pools + 1) * sizeof(struct st_pool *));
			st_pools[st_pool_nb_pools] = malloc(sizeof(struct st_pool));

			struct st_tape_format * format = st_tape_format_get_by_density_code(density_code);

			if (!con->ops->get_pool(con, st_pools[st_pool_nb_pools], uuid)) {
				pool = st_pools[st_pool_nb_pools];
				st_pool_nb_pools++;
				pool->format = format;
			} else {
				struct st_pool * tmp = st_pools[st_pool_nb_pools];
				tmp->id = -1;
				strcpy(tmp->uuid, uuid);
				strcpy(tmp->name, name);
				tmp->format = format;

				if (!con->ops->create_pool(con, tmp)) {
					pool = st_pools[st_pool_nb_pools];
					pool->format = format;
					st_pool_nb_pools++;
				}
			}

			if (!pool) {
				free(st_pools[st_pool_nb_pools]);
				st_pools = realloc(st_pools, st_pool_nb_pools * sizeof(struct st_pool *));
			}

			con->ops->close(con);
			con->ops->free(con);
			free(con);
		}
	}

	pthread_mutex_unlock(&lock);
	pthread_setcancelstate(old_state, 0);

	return pool;
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

