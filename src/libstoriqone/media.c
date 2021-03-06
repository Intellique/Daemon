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

// dgettext, gettext
#include <libintl.h>
// free, malloc
#include <stdlib.h>
// strncpy
#include <string.h>
// bzero
#include <strings.h>

#define gettext_noop(String) String

#include <libstoriqone/archive.h>
#include <libstoriqone/media.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

static struct so_media_format_data_type2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_media_format_data_type type;
} so_media_format_data_types[] = {
	[so_media_format_data_audio]    = { 0, 0, gettext_noop("audio"),    NULL, so_media_format_data_audio },
	[so_media_format_data_cleaning] = { 0, 0, gettext_noop("cleaning"), NULL, so_media_format_data_cleaning },
	[so_media_format_data_data]     = { 0, 0, gettext_noop("data"),     NULL, so_media_format_data_data },
	[so_media_format_data_video]    = { 0, 0, gettext_noop("video"),    NULL, so_media_format_data_video },

	[so_media_format_data_unknown]  = { 0, 0, gettext_noop("unknown"),  NULL, so_media_format_data_unknown },
};
static const unsigned int so_media_format_nb_data_types = sizeof(so_media_format_data_types) / sizeof(*so_media_format_data_types);

static struct so_media_format_mode2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_media_format_mode mode;
} so_media_format_modes[] = {
	[so_media_format_mode_disk]    = { 0, 0, gettext_noop("disk"),    NULL, so_media_format_mode_disk },
	[so_media_format_mode_linear]  = { 0, 0, gettext_noop("linear"),  NULL, so_media_format_mode_linear },
	[so_media_format_mode_optical] = { 0, 0, gettext_noop("optical"), NULL, so_media_format_mode_optical },

	[so_media_format_mode_unknown] = { 0, 0, gettext_noop("unknown"), NULL, so_media_format_mode_unknown },
};
static const unsigned int so_media_format_nb_modes = sizeof(so_media_format_modes) / sizeof(*so_media_format_modes);

static struct so_media_status2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_media_status status;
} so_media_status[] = {
	[so_media_status_erasable]          = { 0, 0, gettext_noop("erasable"),          NULL, so_media_status_erasable },
	[so_media_status_error]             = { 0, 0, gettext_noop("error"),             NULL, so_media_status_error },
	[so_media_status_foreign]           = { 0, 0, gettext_noop("foreign"),           NULL, so_media_status_foreign },
	[so_media_status_in_use]            = { 0, 0, gettext_noop("in use"),            NULL, so_media_status_in_use },
	[so_media_status_locked]            = { 0, 0, gettext_noop("locked"),            NULL, so_media_status_locked },
	[so_media_status_needs_replacement] = { 0, 0, gettext_noop("needs replacement"), NULL, so_media_status_needs_replacement },
	[so_media_status_new]               = { 0, 0, gettext_noop("new"),               NULL, so_media_status_new },
	[so_media_status_pooled]            = { 0, 0, gettext_noop("pooled"),            NULL, so_media_status_pooled },

	[so_media_status_unknown]           = { 0, 0, gettext_noop("unknown"),           NULL, so_media_status_unknown },
};
static const unsigned int so_media_nb_status = sizeof(so_media_status) / sizeof(*so_media_status);

static struct so_media_type2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_media_type type;
} so_media_types[] = {
	[so_media_type_cleaning]   = { 0, 0, gettext_noop("cleaning"),   NULL, so_media_type_cleaning },
	[so_media_type_rewritable] = { 0, 0, gettext_noop("rewritable"), NULL, so_media_type_rewritable },
	[so_media_type_worm]       = { 0, 0, gettext_noop("WORM"),       NULL, so_media_type_worm },

	[so_media_type_unknown]    = { 0, 0, gettext_noop("unknown"),    NULL, so_media_type_unknown },
};
static const unsigned int so_media_nb_types = sizeof(so_media_types) / sizeof(*so_media_types);

static struct so_pool_autocheck_mode2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_pool_autocheck_mode mode;
} so_pool_autocheck_modes[] = {
	[so_pool_autocheck_quick_mode]    = { 0, 0, gettext_noop("quick mode"),    NULL, so_pool_autocheck_quick_mode },
	[so_pool_autocheck_thorough_mode] = { 0, 0, gettext_noop("thorough mode"), NULL, so_pool_autocheck_thorough_mode },
	[so_pool_autocheck_mode_none]     = { 0, 0, gettext_noop("none"),          NULL, so_pool_autocheck_mode_none },

	[so_pool_autocheck_mode_unknown]  = { 0, 0, gettext_noop("unknown"),       NULL, so_pool_autocheck_mode_unknown },
};
static const unsigned int so_pool_nb_autocheck_modes = sizeof(so_pool_autocheck_modes) / sizeof(*so_pool_autocheck_modes);

static struct so_pool_unbreakable_level2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_pool_unbreakable_level level;
} so_pool_unbreakable_levels[] = {
	[so_pool_unbreakable_level_archive] = { 0, 0, gettext_noop("archive"), NULL, so_pool_unbreakable_level_archive },
	[so_pool_unbreakable_level_file]    = { 0, 0, gettext_noop("file"),    NULL, so_pool_unbreakable_level_file },
	[so_pool_unbreakable_level_none]    = { 0, 0, gettext_noop("none"),    NULL, so_pool_unbreakable_level_none },

	[so_pool_unbreakable_level_unknown] = { 0, 0, gettext_noop("unknown"), NULL, so_pool_unbreakable_level_unknown },
};
static const unsigned int so_pool_nb_unbreakable_levels = sizeof(so_pool_unbreakable_levels) / sizeof(*so_pool_unbreakable_levels);

static void so_media_init(void) __attribute__((constructor));


static void so_media_init(void) {
	unsigned int i;
	for (i = 0; i < so_media_format_nb_data_types; i++) {
		so_media_format_data_types[i].hash = so_string_compute_hash2(so_media_format_data_types[i].name);
		so_media_format_data_types[i].translation = dgettext("libstoriqone", so_media_format_data_types[i].name);
		so_media_format_data_types[i].hash_translated = so_string_compute_hash2(so_media_format_data_types[i].translation);
	}

	for (i = 0; i < so_media_format_nb_modes; i++) {
		so_media_format_modes[i].hash = so_string_compute_hash2(so_media_format_modes[i].name);
		so_media_format_modes[i].translation = dgettext("libstoriqone", so_media_format_modes[i].name);
		so_media_format_modes[i].hash_translated = so_string_compute_hash2(so_media_format_modes[i].translation);
	}

	for (i = 0; i < so_media_nb_status; i++) {
		so_media_status[i].hash = so_string_compute_hash2(so_media_status[i].name);
		so_media_status[i].translation = dgettext("libstoriqone", so_media_status[i].name);
		so_media_status[i].hash_translated = so_string_compute_hash2(so_media_status[i].translation);
	}

	for (i = 0; i < so_media_nb_types; i++) {
		so_media_types[i].hash = so_string_compute_hash2(so_media_types[i].name);
		so_media_types[i].translation = dgettext("libstoriqone", so_media_types[i].name);
		so_media_types[i].hash_translated = so_string_compute_hash2(so_media_types[i].translation);
	}

	for (i = 0; i < so_pool_nb_autocheck_modes; i++) {
		so_pool_autocheck_modes[i].hash = so_string_compute_hash2(so_pool_autocheck_modes[i].name);
		so_pool_autocheck_modes[i].translation = dgettext("libstoriqone", so_pool_autocheck_modes[i].name);
		so_pool_autocheck_modes[i].hash_translated = so_string_compute_hash2(so_pool_autocheck_modes[i].translation);
	}

	for (i = 0; i < so_pool_nb_unbreakable_levels; i++) {
		so_pool_unbreakable_levels[i].hash = so_string_compute_hash2(so_pool_unbreakable_levels[i].name);
		so_pool_unbreakable_levels[i].translation = dgettext("libstoriqone", so_pool_unbreakable_levels[i].name);
		so_pool_unbreakable_levels[i].hash_translated = so_string_compute_hash2(so_pool_unbreakable_levels[i].translation);
	}
}


struct so_value * so_media_convert(struct so_media * media) {
	struct so_value * pool = so_value_new_null();
	if (media == NULL)
		return pool;

	if (media->pool != NULL)
		pool = so_pool_convert(media->pool);

	struct so_value * md = so_value_pack("{sssssssssssIsIsIsIsIsIszszsususzszszsusbsssbsososo}",
		"uuid", media->uuid[0] != '\0' ? media->uuid : NULL,
		"label", media->label,
		"medium serial number", media->medium_serial_number,
		"name", media->name,

		"status", so_media_status_to_string(media->status, false),

		"first used", (long) media->first_used,
		"use before", (long) media->use_before,

		"load count", media->load_count,
		"read count", media->read_count,
		"write count", media->write_count,
		"operation count", media->operation_count,

		"nb total read", media->nb_total_read,
		"nb total write", media->nb_total_write,

		"nb read errors", media->nb_read_errors,
		"nb write errors", media->nb_write_errors,

		"block size", media->block_size,
		"free block", media->free_block,
		"total block", media->total_block,

		"nb volumes", media->nb_volumes,
		"append", media->append,
		"type", so_media_type_to_string(media->type, false),
		"write lock", media->write_lock,

		"archive format", so_archive_format_convert(media->archive_format),
		"media format", so_media_format_convert(media->media_format),
		"pool", pool
	);

	if (media->last_read > 0)
		so_value_hashtable_put2(md, "last read", so_value_new_integer(media->last_read), true);
	else
		so_value_hashtable_put2(md, "last read", so_value_new_null(), true);

	if (media->last_write > 0)
		so_value_hashtable_put2(md, "last write", so_value_new_integer(media->last_write), true);
	else
		so_value_hashtable_put2(md, "last write", so_value_new_null(), true);

	if (media->pool != NULL)
		so_value_hashtable_put2(md, "pool", so_pool_convert(media->pool), true);
	else
		so_value_hashtable_put2(md, "pool", so_value_new_null(), true);

	return md;
}

struct so_media * so_media_dup(struct so_media * media) {
	struct so_media * new_media = malloc(sizeof(struct so_media));
	bzero(new_media, sizeof(struct so_media));

	strncpy(new_media->uuid, media->uuid, 37);
	if (media->label != NULL)
		new_media->label = strdup(media->label);

	if (media->medium_serial_number != NULL)
		new_media->medium_serial_number = strdup(media->medium_serial_number);

	if (media->name != NULL)
		new_media->name = strdup(media->name);

	new_media->status = media->status;

	new_media->first_used = media->first_used;
	new_media->use_before = media->use_before;
	new_media->last_read = media->last_read;
	new_media->last_write = media->last_write;

	new_media->load_count = media->load_count;
	new_media->read_count = media->read_count;
	new_media->write_count = media->write_count;
	new_media->operation_count = media->operation_count;

	new_media->nb_total_read = media->nb_total_read;
	new_media->nb_total_write = media->nb_total_write;

	new_media->nb_read_errors = media->nb_read_errors;
	new_media->nb_write_errors = media->nb_write_errors;

	new_media->block_size = media->block_size;
	new_media->free_block = media->free_block;
	new_media->total_block = media->total_block;

	new_media->nb_volumes = media->nb_volumes;
	new_media->append = media->append;
	new_media->type = media->type;
	new_media->write_lock = media->write_lock;

	if (media->archive_format != NULL)
		new_media->archive_format = so_archive_format_dup(media->archive_format);
	new_media->media_format = so_media_format_dup(media->media_format);
	if (media->pool != NULL)
		new_media->pool = so_pool_dup(media->pool);

	new_media->private_data = NULL;
	new_media->free_private_data = NULL;
	new_media->db_data = so_value_share(media->db_data);

	return new_media;
}

struct so_value * so_media_format_convert(struct so_media_format * format) {
	return so_value_pack("{sssusssssIsIsIsIsIszszsbsb}",
		"name", format->name,

		"density code", format->density_code,
		"type", so_media_format_data_type_to_string(format->type, false),
		"mode", so_media_format_mode_to_string(format->mode, false),

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

const char * so_media_get_name(const struct so_media * media) {
	if (media == NULL)
		return NULL;

	if (media->name != NULL)
		return media->name;

	if (media->label != NULL)
		return media->label;

	if (media->medium_serial_number != NULL)
		return media->medium_serial_number;

	return NULL;
}

void so_media_format_sync(struct so_media_format * format, struct so_value * new_format) {
	const char * name = NULL;

	long int density_code = 0;
	const char * type = NULL, * mode = NULL;

	so_value_unpack(new_format, "{sSsusSsSsIsIsIsIsIszszsbsb}",
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
		strncpy(format->name, name, 63);

	format->density_code = density_code;
	format->type = so_media_string_to_format_data_type(type, false);
	format->mode = so_media_string_to_format_mode(mode, false);
}

struct so_media * so_media_new(struct so_value * media) {
	struct so_media * new_media = malloc(sizeof(struct so_media));
	bzero(new_media, sizeof(struct so_media));

	so_media_sync(new_media, media);

	return new_media;
}

void so_media_sync(struct so_media * media, struct so_value * new_media) {
	free(media->label);
	free(media->medium_serial_number);
	free(media->name);
	media->label = media->medium_serial_number = media->name = NULL;

	struct so_value * uuid = NULL;
	struct so_value * archive_format = NULL;
	struct so_value * media_format = NULL;
	struct so_value * pool = NULL;
	struct so_value * last_read = NULL;
	struct so_value * last_write = NULL;

	const char * status = NULL, * type = NULL;
	long int nb_read_errors = 0, nb_write_errors = 0;
	long int nb_volumes = 0;

	so_value_unpack(new_media, "{sosssssssSsisisososIsIsIsIszszsususzszszsusbsSsbsososo}",
		"uuid", &uuid,
		"label", &media->label,
		"medium serial number", &media->medium_serial_number,
		"name", &media->name,

		"status", &status,

		"first used", &media->first_used,
		"use before", &media->use_before,
		"last read", &last_read,
		"last write", &last_write,

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

		"archive format", &archive_format,
		"media format", &media_format,
		"pool", &pool
	);

	if (uuid->type != so_value_null) {
		strncpy(media->uuid, so_value_string_get(uuid), 37);
		media->uuid[36] = '\0';
	} else
		media->uuid[0] = '\0';

	media->status = so_media_string_to_status(status, false);

	if (last_read->type != so_value_integer)
		media->last_read = 0;
	else
		media->last_read = so_value_integer_get(last_read);

	if (last_write->type != so_value_integer)
		media->last_write = 0;
	else
		media->last_write = so_value_integer_get(last_write);

	media->nb_read_errors = nb_read_errors;
	media->nb_write_errors = nb_write_errors;

	media->nb_volumes = nb_volumes;
	media->type = so_media_string_to_type(type, false);

	if (archive_format->type != so_value_null) {
		if (media->archive_format == NULL) {
			media->archive_format = malloc(sizeof(struct so_archive_format));
			bzero(media->archive_format, sizeof(struct so_archive_format));
		}

		so_archive_format_sync(media->archive_format, archive_format);
	} else if (media->archive_format != NULL) {
		so_archive_format_free(media->archive_format);
		media->archive_format = NULL;
	}

	if (media->media_format == NULL) {
		media->media_format = malloc(sizeof(struct so_media_format));
		bzero(media->media_format, sizeof(struct so_media_format));
	}
	so_media_format_sync(media->media_format, media_format);

	if (media->pool != NULL && pool->type == so_value_hashtable) {
		so_pool_sync(media->pool, pool);
	} else if (media->pool == NULL && pool->type == so_value_hashtable) {
		media->pool = malloc(sizeof(struct so_pool));
		bzero(media->pool, sizeof(struct so_pool));
		so_pool_sync(media->pool, pool);
	} else if (media->pool != NULL && pool->type == so_value_null) {
		so_pool_free(media->pool);
		media->pool = NULL;
	}
}


struct so_value * so_pool_convert(struct so_pool * pool) {
	if (pool == NULL)
		return so_value_new_null();

	return so_value_pack("{sssssssbsssbsbsbsoso}",
		"uuid", pool->uuid,
		"name", pool->name,

		"auto check", so_pool_autocheck_mode_to_string(pool->auto_check, false),
		"growable", pool->growable,
		"unbreakable level", so_pool_unbreakable_level_to_string(pool->unbreakable_level, false),
		"rewritable", pool->rewritable,
		"backup", pool->backup,
		"deleted", pool->deleted,

		"archive format", so_archive_format_convert(pool->archive_format),
		"media format", so_media_format_convert(pool->media_format)
	);
}

struct so_pool * so_pool_dup(struct so_pool * pool) {
	struct so_pool * new_pool = malloc(sizeof(struct so_pool));
	bzero(new_pool, sizeof(struct so_pool));

	strncpy(new_pool->uuid, pool->uuid, 37);
	new_pool->name = strdup(pool->name);

	new_pool->auto_check = pool->auto_check;
	new_pool->growable = pool->growable;
	new_pool->unbreakable_level = pool->unbreakable_level;
	new_pool->rewritable = pool->rewritable;
	new_pool->backup = pool->backup;
	new_pool->deleted = pool->deleted;

	new_pool->archive_format = so_archive_format_dup(pool->archive_format);
	new_pool->media_format = so_media_format_dup(pool->media_format);

	new_pool->db_data = so_value_share(pool->db_data);

	return new_pool;
}

void so_pool_sync(struct so_pool * pool, struct so_value * new_pool) {
	const char * uuid = NULL;
	free(pool->name);
	pool->name = NULL;

	const char * auto_check = NULL, * unbreakable_level = NULL;

	struct so_value * archive_format = NULL;
	struct so_value * media_format = NULL;

	so_value_unpack(new_pool, "{sSsssSsbsSsbsbsbsoso}",
		"uuid", &uuid,
		"name", &pool->name,

		"auto check", &auto_check,
		"growable", &pool->growable,
		"unbreakable level", &unbreakable_level,
		"rewritable", &pool->rewritable,
		"backup", &pool->backup,
		"deleted", &pool->deleted,

		"archive format", &archive_format,
		"media format", &media_format
	);

	if (uuid != NULL)
		strncpy(pool->uuid, uuid, 37);

	pool->auto_check = so_pool_string_to_autocheck_mode(auto_check, false);
	pool->unbreakable_level = so_pool_string_to_unbreakable_level(unbreakable_level, false);

	if (pool->archive_format == NULL) {
		pool->archive_format = malloc(sizeof(struct so_archive_format));
		bzero(pool->archive_format, sizeof(struct so_archive_format));
	}
	so_archive_format_sync(pool->archive_format, archive_format);

	if (pool->media_format == NULL) {
		pool->media_format = malloc(sizeof(struct so_media_format));
		bzero(pool->media_format, sizeof(struct so_media_format));
	}
	so_media_format_sync(pool->media_format, media_format);
}


int so_media_format_cmp(struct so_media_format * f1, struct so_media_format * f2) {
	if (f1->mode != f2->mode)
		return f1->mode - f2->mode;

	return f1->density_code - f2->density_code;
}

struct so_media_format * so_media_format_dup(const struct so_media_format * format) {
	struct so_media_format * new_format = malloc(sizeof(struct so_media_format));
	bzero(new_format, sizeof(struct so_media_format));

	strncpy(new_format->name, format->name, 64);
	new_format->density_code = format->density_code;
	new_format->type = format->type;
	new_format->mode = format->mode;

	new_format->max_load_count = format->max_load_count;
	new_format->max_read_count = format->max_read_count;
	new_format->max_write_count = format->max_write_count;
	new_format->max_operation_count = format->max_operation_count;

	new_format->life_span = format->life_span;

	new_format->capacity = format->capacity;
	new_format->block_size = format->block_size;

	new_format->support_partition = format->support_partition;
	new_format->support_mam = format->support_mam;

	new_format->db_data = so_value_share(format->db_data);

	return new_format;
}


void so_media_free(struct so_media * media) {
	if (media == NULL)
		return;

	free(media->label);
	free(media->medium_serial_number);
	free(media->name);

	so_archive_format_free(media->archive_format);
	so_media_format_free(media->media_format);
	so_pool_free(media->pool);

	if (media->free_private_data != NULL)
		media->free_private_data(media->private_data);
	else if (media->private_data != NULL)
		free(media->private_data);
	so_value_free(media->db_data);

	free(media);
}

void so_media_free2(void * media) {
	so_media_free(media);
}

void so_media_format_free(struct so_media_format * format) {
	so_value_free(format->db_data);
	free(format);
}

void so_pool_free(struct so_pool * pool) {
	if (pool == NULL)
		return;

	free(pool->name);
	so_archive_format_free(pool->archive_format);
	so_media_format_free(pool->media_format);
	so_value_free(pool->db_data);
	free(pool);
}

void so_pool_free2(void * pool) {
	so_pool_free(pool);
}


const char * so_media_format_data_type_to_string(enum so_media_format_data_type type, bool translate) {
	if (translate)
		return so_media_format_data_types[type].translation;
	else
		return so_media_format_data_types[type].name;
}

enum so_media_format_data_type so_media_string_to_format_data_type(const char * type, bool translate) {
	if (type == NULL)
		return so_media_format_data_unknown;

	unsigned int i;
	unsigned long long hash = so_string_compute_hash2(type);

	if (translate) {
		for (i = 0; i < so_media_format_nb_data_types; i++)
			if (hash == so_media_format_data_types[i].hash_translated)
				return so_media_format_data_types[i].type;
	} else {
		for (i = 0; i < so_media_format_nb_data_types; i++)
			if (hash == so_media_format_data_types[i].hash)
				return so_media_format_data_types[i].type;
	}

	return so_media_format_data_unknown;
}


const char * so_media_format_mode_to_string(enum so_media_format_mode mode, bool translate) {
	if (translate)
		return so_media_format_modes[mode].translation;
	else
		return so_media_format_modes[mode].name;
}

enum so_media_format_mode so_media_string_to_format_mode(const char * mode, bool translate) {
	if (mode == NULL)
		return so_media_format_mode_unknown;

	unsigned int i;
	unsigned long long hash = so_string_compute_hash2(mode);

	if (translate) {
		for (i = 0; i < so_media_format_nb_modes; i++)
			if (hash == so_media_format_modes[i].hash_translated)
				return so_media_format_modes[i].mode;
	} else {
		for (i = 0; i < so_media_format_nb_modes; i++)
			if (hash == so_media_format_modes[i].hash)
				return so_media_format_modes[i].mode;
	}

	return so_media_format_mode_unknown;
}


const char * so_media_status_to_string(enum so_media_status status, bool translate) {
	if (translate)
		return so_media_status[status].translation;
	else
		return so_media_status[status].name;
}

enum so_media_status so_media_string_to_status(const char * status, bool translate) {
	if (status == NULL)
		return so_media_status_unknown;

	unsigned int i;
	unsigned long long hash = so_string_compute_hash2(status);

	if (translate) {
		for (i = 0; i < so_media_nb_status; i++)
			if (hash == so_media_status[i].hash_translated)
				return so_media_status[i].status;
	} else {
		for (i = 0; i < so_media_nb_status; i++)
			if (hash == so_media_status[i].hash)
				return so_media_status[i].status;
	}

	return so_media_status_unknown;
}


enum so_media_type so_media_string_to_type(const char * type, bool translate) {
	if (type == NULL)
		return so_media_type_unknown;

	unsigned int i;
	unsigned long long hash = so_string_compute_hash2(type);

	if (translate) {
		for (i = 0; i < so_media_nb_types; i++)
			if (hash == so_media_types[i].hash_translated)
				return so_media_types[i].type;
	} else {
		for (i = 0; i < so_media_nb_types; i++)
			if (hash == so_media_types[i].hash)
				return so_media_types[i].type;
	}

	return so_media_type_unknown;
}

const char * so_media_type_to_string(enum so_media_type type, bool translate) {
	if (translate)
		return so_media_types[type].translation;
	else
		return so_media_types[type].name;
}


const char * so_pool_autocheck_mode_to_string(enum so_pool_autocheck_mode mode, bool translate) {
	if (translate)
		return so_pool_autocheck_modes[mode].translation;
	else
		return so_pool_autocheck_modes[mode].name;
}

enum so_pool_autocheck_mode so_pool_string_to_autocheck_mode(const char * mode, bool translate) {
	if (mode == NULL)
		return so_pool_autocheck_mode_unknown;

	unsigned int i;
	unsigned long long hash = so_string_compute_hash2(mode);

	if (translate) {
		for (i = 0; i < so_pool_nb_autocheck_modes; i++)
			if (hash == so_pool_autocheck_modes[i].hash_translated)
				return so_pool_autocheck_modes[i].mode;
	} else {
		for (i = 0; i < so_pool_nb_autocheck_modes; i++)
			if (hash == so_pool_autocheck_modes[i].hash)
				return so_pool_autocheck_modes[i].mode;
	}

	return so_pool_autocheck_mode_unknown;
}


enum so_pool_unbreakable_level so_pool_string_to_unbreakable_level(const char * level, bool translate) {
	if (level == NULL)
		return so_pool_unbreakable_level_unknown;

	unsigned int i;
	unsigned long long hash = so_string_compute_hash2(level);

	if (translate) {
		for (i = 0; i < so_pool_nb_unbreakable_levels; i++)
			if (hash == so_pool_unbreakable_levels[i].hash_translated)
				return so_pool_unbreakable_levels[i].level;
	} else {
		for (i = 0; i < so_pool_nb_unbreakable_levels; i++)
			if (hash == so_pool_unbreakable_levels[i].hash)
				return so_pool_unbreakable_levels[i].level;
	}

	return so_pool_unbreakable_level_unknown;
}

const char * so_pool_unbreakable_level_to_string(enum so_pool_unbreakable_level level, bool translate) {
	if (translate)
		return so_pool_unbreakable_levels[level].translation;
	else
		return so_pool_unbreakable_levels[level].name;
}
