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
// strcasecmp
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

static struct st_media_location2 {
	const char * name;
	const enum st_media_location location;
	unsigned long long hash;
} st_media_locations[] = {
	{ "in drive", st_media_location_indrive, 0 },
	{ "offline",  st_media_location_offline, 0 },
	{ "online",   st_media_location_online,  0 },

	{ "unknown", st_media_location_unknown, 0 },
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
	{ "readonly",   st_media_type_readonly,   0 },
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

	for (i = 0; st_media_locations[i].location != st_media_location_unknown; i++)
		st_media_locations[i].hash = st_string_compute_hash2(st_media_locations[i].name);
	st_media_locations[i].hash = st_string_compute_hash2(st_media_locations[i].name);

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


__asm__(".symver st_media_format_data_to_string_v1, st_media_format_data_to_string@@LIBSTONE_1.2");
const char * st_media_format_data_to_string_v1(enum st_media_format_data_type type) {
	unsigned int i;
	for (i = 0; st_media_format_data_types[i].type != st_media_format_data_unknown; i++)
		if (st_media_format_data_types[i].type == type)
			return st_media_format_data_types[i].name;

	return st_media_format_data_types[i].name;
}

__asm__(".symver st_media_string_to_format_data_v1, st_media_string_to_format_data@@LIBSTONE_1.2");
enum st_media_format_data_type st_media_string_to_format_data_v1(const char * type) {
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


__asm__(".symver st_media_location_to_string_v1, st_media_location_to_string@@LIBSTONE_1.2");
const char * st_media_location_to_string_v1(enum st_media_location location) {
	unsigned int i;
	for (i = 0; st_media_locations[i].location != st_media_location_unknown; i++)
		if (st_media_locations[i].location == location)
			return st_media_locations[i].name;

	return st_media_locations[i].name;
}

__asm__(".symver st_media_string_to_location_v1, st_media_string_to_location@@LIBSTONE_1.2");
enum st_media_location st_media_string_to_location_v1(const char * location) {
	if (location == NULL)
		return st_media_location_unknown;

	unsigned int i;
	unsigned long long hash = st_string_compute_hash2(location);
	for (i = 0; st_media_locations[i].location != st_media_location_unknown; i++)
		if (hash == st_media_locations[i].hash)
			return st_media_locations[i].location;

	return st_media_locations[i].location;
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

