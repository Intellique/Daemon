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

#ifndef __LIBSTONE_MEDIA_H__
#define __LIBSTONE_MEDIA_H__

// bool
#include <stdbool.h>
// ssize_t, time_t
#include <sys/types.h>

enum st_media_format_data_type {
	st_media_format_data_audio = 0x1,
	st_media_format_data_cleaning = 0x2,
	st_media_format_data_data = 0x3,
	st_media_format_data_video = 0x4,

	st_media_format_data_unknown = 0x0,
};

enum st_media_format_mode {
	st_media_format_mode_disk = 0x1,
	st_media_format_mode_linear = 0x2,
	st_media_format_mode_optical = 0x3,

	st_media_format_mode_unknown = 0x0,
};

enum st_media_location {
	st_media_location_indrive = 0x1,
	st_media_location_offline = 0x2,
	st_media_location_online = 0x3,

	st_media_location_unknown = 0x0,
};

enum st_media_status {
	st_media_status_erasable = 0x1,
	st_media_status_error = 0x2,
	st_media_status_foreign = 0x3,
	st_media_status_in_use = 0x4,
	st_media_status_locked = 0x5,
	st_media_status_needs_replacement = 0x6,
	st_media_status_new = 0x7,
	st_media_status_pooled = 0x8,

	st_media_status_unknown = 0x0,
};

enum st_media_type {
	st_media_type_cleaning = 0x1,
	st_media_type_readonly = 0x2,
	st_media_type_rewritable = 0x3,
	st_media_type_worm = 0x4, //TODO: à ajouter dans la base de données

	st_media_type_unknown = 0x0,
};

enum st_pool_autocheck_mode {
	st_pool_autocheck_quick_mode = 0x1,
	st_pool_autocheck_thorough_mode = 0x2,
	st_pool_autocheck_mode_none = 0x3,

	st_pool_autocheck_mode_unknown = 0x0,
};

enum st_pool_unbreakable_level {
	st_pool_unbreakable_level_archive = 0x1,
	st_pool_unbreakable_level_file = 0x2,
	st_pool_unbreakable_level_none = 0x3,

	st_pool_unbreakable_level_unknown = 0x0,
};


struct st_media {
	char uuid[37];
	char * label;
	char * medium_serial_number;
	char * name;

	enum st_media_status status;
	enum st_media_location location;

	time_t first_used;
	time_t use_before;
	time_t last_read;
	time_t last_write;

	long load_count;
	long read_count;
	long write_count;
	long operation_count;

	ssize_t nb_total_read;
	ssize_t nb_total_write;

	unsigned int nb_read_errors;
	unsigned int nb_write_errors;

	ssize_t block_size;
	ssize_t free_block; // in block size, not in bytes
	ssize_t total_block; // size in block

	unsigned int nb_volumes;
	enum st_media_type type;

	struct st_media_format * format;
	struct st_pool * pool;

	struct st_value * db_data;
};

struct st_media_format {
	char name[64];

	unsigned char density_code;
	enum st_media_format_data_type type;
	enum st_media_format_mode mode;

	long max_load_count;
	long max_read_count;
	long max_write_count;
	long max_operation_count;

	long life_span;

	ssize_t capacity;
	ssize_t block_size;

	bool support_partition;
	bool support_mam;
};

struct st_pool {
	char uuid[37];
	char * name;

	enum st_pool_autocheck_mode auto_check;
	bool growable;
	enum st_pool_unbreakable_level unbreakable_level;
	bool rewritable;
	bool deleted;

	struct st_media_format * format;
};


void st_media_free(struct st_media * media) __attribute__((nonnull));
void st_media_format_free(struct st_media_format * format) __attribute__((nonnull));
void st_pool_free(struct st_pool * pool) __attribute__((nonnull));

const char * st_media_format_data_to_string(enum st_media_format_data_type type);
enum st_media_format_data_type st_media_string_to_format_data(const char * type) __attribute__((nonnull));

const char * st_media_format_mode_to_string(enum st_media_format_mode mode);
enum st_media_format_mode st_media_string_to_format_mode(const char * mode) __attribute__((nonnull));

const char * st_media_location_to_string(enum st_media_location location);
enum st_media_location st_media_string_to_location(const char * location) __attribute__((nonnull));

const char * st_media_status_to_string(enum st_media_status status);
enum st_media_status st_media_string_to_status(const char * status) __attribute__((nonnull));

enum st_media_type st_media_string_to_type(const char * type) __attribute__((nonnull));
const char * st_media_type_to_string(enum st_media_type type);

const char * st_pool_autocheck_mode_to_string(enum st_pool_autocheck_mode mode);
enum st_pool_autocheck_mode st_pool_string_to_autocheck_mode(const char * mode) __attribute__((nonnull));

enum st_pool_unbreakable_level st_pool_string_to_unbreakable_level(const char * level) __attribute__((nonnull));
const char * st_pool_unbreakable_level_to_string(enum st_pool_unbreakable_level level);

#endif

