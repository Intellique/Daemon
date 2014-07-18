/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
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

// free
#include <stdlib.h>
// strncpy
#include <string.h>

#include "media.h"
#include "string.h"

static struct st_media_format_data_type2 {
	const char * name;
	const enum st_media_format_data_type type;
	unsigned long long hash;
} st_media_format_data_types[] = {
	{ "audio",    st_media_format_data_audio,    0 },
	{ "cleaning", st_media_format_data_cleaning, 0 },
	{ "data",     st_media_format_data_data,     0 },
	{ "video",    st_media_format_data_video,    0 },

	{ "unknown", st_media_format_data_unknown, 0 },
};

static struct st_media_format_mode2 {
	const char * name;
	const enum st_media_format_mode mode;
	unsigned long long hash;
} st_media_format_modes[] = {
	{ "disk",    st_media_format_mode_disk,    0 },
	{ "linear",  st_media_format_mode_linear,  0 },
	{ "optical", st_media_format_mode_optical, 0 },

	{ "unknown", st_media_format_mode_unknown, 0 },
};

static struct st_media_status2 {
	const char * name;
	const enum st_media_status status;
	unsigned long long hash;
} st_media_status[] = {
	{ "erasable",          st_media_status_erasable,          0 },
	{ "error",             st_media_status_error,             0 },
	{ "foreign",           st_media_status_foreign,           0 },
	{ "in use",            st_media_status_in_use,            0 },
	{ "locked",            st_media_status_locked,            0 },
	{ "needs replacement", st_media_status_needs_replacement, 0 },
	{ "new",               st_media_status_new,               0 },
	{ "pooled",            st_media_status_pooled,            0 },

	{ "unknown", st_media_status_unknown, 0 },
};

static struct st_media_type2 {
	const char * name;
	const enum st_media_type type;
	unsigned long long hash;
} st_media_types[] = {
	{ "cleaning",   st_media_type_cleaning,   0 },
	{ "rewritable", st_media_type_rewritable, 0 },
	{ "worm",       st_media_type_worm,       0 },

	{ "unknown", st_media_type_unknown, 0 },
};

static struct st_pool_autocheck_mode2 {
	const char * name;
	const enum st_pool_autocheck_mode mode;
	unsigned long long hash;
} st_pool_autocheck_modes[] = {
	{ "quick mode",    st_pool_autocheck_quick_mode,    0 },
	{ "thorough mode", st_pool_autocheck_thorough_mode, 0 },
	{ "none",          st_pool_autocheck_mode_none,     0 },

	{ "unknown", st_pool_autocheck_mode_unknown, 0 },
};

static struct st_pool_unbreakable_level2 {
	const char * name;
	const enum st_pool_unbreakable_level level;
	unsigned long long hash;
} st_pool_unbreakable_levels[] = {
	{ "archive", st_pool_unbreakable_level_archive, 0 },
	{ "file",    st_pool_unbreakable_level_file,    0 },
	{ "none",    st_pool_unbreakable_level_none,    0 },

	{ "unknown", st_pool_unbreakable_level_unknown, 0 },
};

static void st_media_init(void) __attribute__((constructor));


static void st_media_init(void) {
	int i;
	for (i = 0; st_media_format_data_types[i].type != st_media_format_data_unknown; i++)
		st_media_format_data_types[i].hash = st_string_compute_hash2(st_media_format_data_types[i].name);
	st_media_format_data_types[i].hash = st_string_compute_hash2(st_media_format_data_types[i].name);

	for (i = 0; st_media_format_modes[i].mode != st_media_format_mode_unknown; i++)
		st_media_format_modes[i].hash = st_string_compute_hash2(st_media_format_modes[i].name);
	st_media_format_modes[i].hash = st_string_compute_hash2(st_media_format_modes[i].name);

	for (i = 0; st_media_status[i].status != st_media_status_unknown; i++)
		st_media_status[i].hash = st_string_compute_hash2(st_media_status[i].name);
	st_media_status[i].hash = st_string_compute_hash2(st_media_status[i].name);

	for (i = 0; st_media_types[i].type != st_media_type_unknown; i++)
		st_media_types[i].hash = st_string_compute_hash2(st_media_types[i].name);
	st_media_types[i].hash = st_string_compute_hash2(st_media_types[i].name);

	for (i = 0; st_pool_autocheck_modes[i].mode != st_pool_autocheck_mode_unknown; i++)
		st_pool_autocheck_modes[i].hash = st_string_compute_hash2(st_pool_autocheck_modes[i].name);
	st_pool_autocheck_modes[i].hash = st_string_compute_hash2(st_pool_autocheck_modes[i].name);

	for (i = 0; st_pool_unbreakable_levels[i].level != st_pool_unbreakable_level_unknown; i++)
		st_pool_unbreakable_levels[i].hash = st_string_compute_hash2(st_pool_unbreakable_levels[i].name);
	st_pool_unbreakable_levels[i].hash = st_string_compute_hash2(st_pool_unbreakable_levels[i].name);
}


__asm__(".symver st_media_convert_v1, st_media_convert@@LIBSTONE_1.2");
struct st_value * st_media_convert_v1(struct st_media * media) {
	struct st_value * md = st_value_pack("{sssssssssssisisisisisisisisisisisisisisbsssbso}",
		"uuid", media->uuid[0] != '\0' ? media->uuid : NULL,
		"label", media->label,
		"medium serial number", media->medium_serial_number,
		"name", media->name,

		"status", st_media_status_to_string_v1(media->status),

		"first used", media->first_used,
		"use before", media->use_before,

		"load count", media->load_count,
		"read count", media->read_count,
		"write count", media->write_count,
		"operation count", media->operation_count,

		"nb total read", media->nb_total_read,
		"nb total write", media->nb_total_write,

		"nb read errors", (unsigned long int) media->nb_read_errors,
		"nb write errors", (unsigned long int) media->nb_write_errors,

		"block size", media->block_size,
		"free block", media->free_block,
		"total block", media->total_block,

		"nb volumes", (unsigned long int) media->nb_volumes,
		"append", media->append,
		"type", st_media_type_to_string_v1(media->type),
		"write lock", media->write_lock,

		"format", st_media_format_convert_v1(media->format)
	);

	if (media->last_read > 0)
		st_value_hashtable_put2(md, "last read", st_value_new_integer_v1(media->last_read), true);
	else
		st_value_hashtable_put2(md, "last read", st_value_new_null_v1(), true);

	if (media->last_write > 0)
		st_value_hashtable_put2(md, "last write", st_value_new_integer_v1(media->last_write), true);
	else
		st_value_hashtable_put2(md, "last write", st_value_new_null_v1(), true);

	if (media->pool != NULL)
		st_value_hashtable_put2(md, "pool", st_pool_convert_v1(media->pool), true);
	else
		st_value_hashtable_put2(md, "pool", st_value_new_null_v1(), true);

	return md;
}

__asm__(".symver st_media_format_convert_v1, st_media_format_convert@@LIBSTONE_1.2");
struct st_value * st_media_format_convert_v1(struct st_media_format * format) {
	return st_value_pack_v1("{sssisssssisisisisisisisbsb}",
		"name", format->name,

		"density code", (long int) format->density_code,
		"type", st_media_format_data_type_to_string_v1(format->type),
		"mode", st_media_format_mode_to_string_v1(format->mode),

		"max load count", format->max_load_count,
		"max read count", format->max_read_count,
		"max write count", format->max_write_count,
		"max operation count", format->max_operation_count,

		"life span", format->life_span,

		"capacity", format->capacity,
		"block size", format->block_size,

		"support partition", format->support_partition,
		"support mam", format->support_mam
	);
}

__asm__(".symver st_media_format_sync_v1, st_media_format_sync@@LIBSTONE_1.2");
void st_media_format_sync_v1(struct st_media_format * format, struct st_value * new_format) {
	char * name = NULL;

	long int density_code = 0;
	char * type = NULL, * mode = NULL;

	st_value_unpack_v1(new_format, "{sssisssssisisisisisisisbsb}",
		"name", &name,

		"density code", &density_code,
		"type", &type,
		"mode", &mode,

		"max load count", &format->max_load_count,
		"max read count", &format->max_read_count,
		"max write count", &format->max_write_count,
		"max operation count", &format->max_operation_count,

		"life span", &format->life_span,

		"capacity", &format->capacity,
		"block size", &format->block_size,

		"support partition", &format->support_partition,
		"support mam", &format->support_mam
	);

	if (name != NULL)
		strncpy(format->name, name, 64);
	free(name);

	format->density_code = density_code;
	format->type = st_media_string_to_format_data_type_v1(type);
	free(type);
	format->mode = st_media_string_to_format_mode_v1(mode);
	free(mode);
}

__asm__(".symver st_media_sync_v1, st_media_sync@@LIBSTONE_1.2");
void st_media_sync_v1(struct st_media * media, struct st_value * new_media) {
	free(media->label);
	free(media->medium_serial_number);
	free(media->name);
	media->label = media->medium_serial_number = media->name = NULL;

	struct st_value * uuid = NULL;
	struct st_value * format = NULL;

	char * status = NULL, * type = NULL;
	long int nb_read_errors = 0, nb_write_errors = 0;
	long int nb_volumes = 0;

	st_value_unpack_v1(new_media, "{sosssssssssisisisisisisisisisisisisisisbsssbso}",
		"uuid", &uuid,
		"label", &media->label,
		"medium serial number", &media->medium_serial_number,
		"name", &media->name,

		"status", &status,

		"first used", &media->first_used,
		"use before", &media->use_before,

		"load count", &media->load_count,
		"read count", &media->read_count,
		"write count", &media->write_count,
		"operation count", &media->operation_count,

		"nb total read", &media->nb_total_read,
		"nb total write", &media->nb_total_write,

		"nb read errors", &nb_read_errors,
		"nb write errors", &nb_write_errors,

		"block size", &media->block_size,
		"free block", &media->free_block,
		"total block", &media->total_block,

		"nb volumes", &nb_volumes,
		"append", &media->append,
		"type", &type,
		"write lock", &media->write_lock,

		"format", &format
	);

	if (uuid->type != st_value_null) {
		strncpy(media->uuid, st_value_string_get_v1(uuid), 37);
		media->uuid[36] = '\0';
	} else
		media->uuid[0] = '\0';

	media->status = st_media_string_to_status_v1(status);
	free(status);

	media->nb_read_errors = nb_read_errors;
	media->nb_write_errors = nb_write_errors;

	media->nb_volumes = nb_volumes;
	media->type = st_media_string_to_type_v1(type);
	free(type);

	if (media->format == NULL) {
		media->format = malloc(sizeof(struct st_media_format));
		bzero(media->format, sizeof(struct st_media_format));
	}
	st_media_format_sync_v1(media->format, format);
}

__asm__(".symver st_pool_convert_v1, st_pool_convert@@LIBSTONE_1.2");
struct st_value * st_pool_convert_v1(struct st_pool * pool) {
	return st_value_pack_v1("{sssssssbsssbsbso}",
		"uuid", pool->uuid,
		"name", pool->name,

		"auto check", st_pool_autocheck_mode_to_string_v1(pool->auto_check),
		"growable", pool->growable,
		"unbreakable level", st_pool_unbreakable_level_to_string_v1(pool->unbreakable_level),
		"rewritable", pool->rewritable,
		"deleted", pool->deleted,

		"format", st_media_format_convert_v1(pool->format)
	);
}

__asm__(".symver st_pool_sync_v1, st_pool_sync@@LIBSTONE_1.2");
void st_pool_sync_v1(struct st_pool * pool, struct st_value * new_pool) {
	char * uuid = NULL;
	free(pool->name);
	pool->name = NULL;

	char * auto_check = NULL, * unbreakable_level = NULL;

	struct st_value * format = NULL;

	st_value_unpack_v1(new_pool, "{sssssssbsssbso}",
		"uuid", &uuid,
		"name", &pool->name,

		"auto check", &auto_check,
		"growable", &pool->growable,
		"unbreakable level", &unbreakable_level,
		"deleted", &pool->deleted,

		"format", &format
	);

	if (uuid != NULL)
		strncpy(pool->uuid, uuid, 37);
	free(uuid);

	pool->auto_check = st_pool_string_to_autocheck_mode_v1(auto_check);
	free(auto_check);
	pool->unbreakable_level = st_pool_string_to_unbreakable_level_v1(unbreakable_level);
	free(unbreakable_level);

	if (pool->format == NULL) {
		pool->format = malloc(sizeof(struct st_media_format));
		bzero(pool->format, sizeof(struct st_media_format));
	}
	st_media_format_sync_v1(pool->format, format);
}


__asm__(".symver st_media_free_v1, st_media_free@@LIBSTONE_1.2");
void st_media_free_v1(struct st_media * media) {
	if (media == NULL)
		return;

	free(media->label);
	free(media->medium_serial_number);
	free(media->name);

	st_media_format_free_v1(media->format);
	st_pool_free_v1(media->pool);

	st_value_free_v1(media->db_data);

	free(media);
}

__asm__(".symver st_media_format_free_v1, st_media_format_free@@LIBSTONE_1.2");
void st_media_format_free_v1(struct st_media_format * format) {
	st_value_free_v1(format->db_data);
	free(format);
}

__asm__(".symver st_pool_free_v1, st_pool_free@@LIBSTONE_1.2");
void st_pool_free_v1(struct st_pool * pool) {
	if (pool == NULL)
		return;

	free(pool->name);
	st_media_format_free_v1(pool->format);
	free(pool);
}


__asm__(".symver st_media_format_data_type_to_string_v1, st_media_format_data_type_to_string@@LIBSTONE_1.2");
const char * st_media_format_data_type_to_string_v1(enum st_media_format_data_type type) {
	unsigned int i;
	for (i = 0; st_media_format_data_types[i].type != st_media_format_data_unknown; i++)
		if (st_media_format_data_types[i].type == type)
			return st_media_format_data_types[i].name;

	return st_media_format_data_types[i].name;
}

__asm__(".symver st_media_string_to_format_data_type_v1, st_media_string_to_format_data_type@@LIBSTONE_1.2");
enum st_media_format_data_type st_media_string_to_format_data_type_v1(const char * type) {
	if (type == NULL)
		return st_media_format_data_unknown;

	unsigned int i;
	unsigned long long hash = st_string_compute_hash2(type);
	for (i = 0; st_media_format_data_types[i].type != st_media_format_data_unknown; i++)
		if (hash == st_media_format_data_types[i].hash)
			return st_media_format_data_types[i].type;

	return st_media_format_data_types[i].type;
}


__asm__(".symver st_media_format_mode_to_string_v1, st_media_format_mode_to_string@@LIBSTONE_1.2");
const char * st_media_format_mode_to_string_v1(enum st_media_format_mode mode) {
	unsigned int i;
	for (i = 0; st_media_format_modes[i].mode != st_media_format_mode_unknown; i++)
		if (st_media_format_modes[i].mode == mode)
			return st_media_format_modes[i].name;

	return st_media_format_modes[i].name;
}

__asm__(".symver st_media_string_to_format_mode_v1, st_media_string_to_format_mode@@LIBSTONE_1.2");
enum st_media_format_mode st_media_string_to_format_mode_v1(const char * mode) {
	if (mode == NULL)
		return st_media_format_mode_unknown;

	unsigned int i;
	unsigned long long hash = st_string_compute_hash2(mode);
	for (i = 0; st_media_format_modes[i].mode != st_media_format_mode_unknown; i++)
		if (hash == st_media_format_modes[i].hash)
			return st_media_format_modes[i].mode;

	return st_media_format_modes[i].mode;
}


__asm__(".symver st_media_status_to_string_v1, st_media_status_to_string@@LIBSTONE_1.2");
const char * st_media_status_to_string_v1(enum st_media_status status) {
	unsigned int i;
	for (i = 0; st_media_status[i].status != st_media_status_unknown; i++)
		if (st_media_status[i].status == status)
			return st_media_status[i].name;

	return st_media_status[i].name;
}

__asm__(".symver st_media_string_to_status_v1, st_media_string_to_status@@LIBSTONE_1.2");
enum st_media_status st_media_string_to_status_v1(const char * status) {
	if (status == NULL)
		return st_media_status_unknown;

	unsigned int i;
	unsigned long long hash = st_string_compute_hash2(status);
	for (i = 0; st_media_status[i].status != st_media_status_unknown; i++)
		if (hash == st_media_status[i].hash)
			return st_media_status[i].status;

	return st_media_status[i].status;
}


__asm__(".symver st_media_string_to_type_v1, st_media_string_to_type@@LIBSTONE_1.2");
enum st_media_type st_media_string_to_type_v1(const char * type) {
	if (type == NULL)
		return st_media_type_unknown;

	unsigned int i;
	unsigned long long hash = st_string_compute_hash2(type);
	for (i = 0; st_media_types[i].type != st_media_type_unknown; i++)
		if (hash == st_media_types[i].hash)
			return st_media_types[i].type;

	return st_media_types[i].type;
}

__asm__(".symver st_media_type_to_string_v1, st_media_type_to_string@@LIBSTONE_1.2");
const char * st_media_type_to_string_v1(enum st_media_type type) {
	unsigned int i;
	for (i = 0; st_media_types[i].type != st_media_type_unknown; i++)
		if (st_media_types[i].type == type)
			return st_media_types[i].name;

	return st_media_types[i].name;
}


__asm__(".symver st_pool_autocheck_mode_to_string_v1, st_pool_autocheck_mode_to_string@@LIBSTONE_1.2");
const char * st_pool_autocheck_mode_to_string_v1(enum st_pool_autocheck_mode mode) {
	unsigned int i;
	for (i = 0; st_pool_autocheck_modes[i].mode != st_pool_autocheck_mode_unknown; i++)
		if (st_pool_autocheck_modes[i].mode == mode)
			return st_pool_autocheck_modes[i].name;

	return st_pool_autocheck_modes[i].name;
}

__asm__(".symver st_pool_string_to_autocheck_mode_v1, st_pool_string_to_autocheck_mode@@LIBSTONE_1.2");
enum st_pool_autocheck_mode st_pool_string_to_autocheck_mode_v1(const char * mode) {
	if (mode == NULL)
		return st_pool_autocheck_mode_unknown;

	unsigned int i;
	unsigned long long hash = st_string_compute_hash2(mode);
	for (i = 0; st_pool_autocheck_modes[i].mode != st_pool_autocheck_mode_unknown; i++)
		if (hash == st_pool_autocheck_modes[i].hash)
			return st_pool_autocheck_modes[i].mode;

	return st_pool_autocheck_modes[i].mode;
}


__asm__(".symver st_pool_string_to_unbreakable_level_v1, st_pool_string_to_unbreakable_level@@LIBSTONE_1.2");
enum st_pool_unbreakable_level st_pool_string_to_unbreakable_level_v1(const char * level) {
	if (level == NULL)
		return st_pool_unbreakable_level_unknown;

	unsigned int i;
	unsigned long long hash = st_string_compute_hash2(level);
	for (i = 0; st_pool_unbreakable_levels[i].level != st_pool_unbreakable_level_unknown; i++)
		if (hash == st_pool_unbreakable_levels[i].hash)
			return st_pool_unbreakable_levels[i].level;

	return st_pool_unbreakable_levels[i].level;
}

__asm__(".symver st_pool_unbreakable_level_to_string_v1, st_pool_unbreakable_level_to_string@@LIBSTONE_1.2");
const char * st_pool_unbreakable_level_to_string_v1(enum st_pool_unbreakable_level level) {
	unsigned int i;
	for (i = 0; st_pool_unbreakable_levels[i].level != st_pool_unbreakable_level_unknown; i++)
		if (st_pool_unbreakable_levels[i].level == level)
			return st_pool_unbreakable_levels[i].name;

	return st_pool_unbreakable_levels[i].name;
}

