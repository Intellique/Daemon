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

#ifndef __LIBSTORIQONE_MEDIA_H__
#define __LIBSTORIQONE_MEDIA_H__

// bool
#include <stdbool.h>
// size_t, ssize_t, time_t
#include <sys/types.h>

enum so_media_format_data_type {
	so_media_format_data_audio = 0x1,
	so_media_format_data_cleaning = 0x2,
	so_media_format_data_data = 0x3,
	so_media_format_data_video = 0x4,

	so_media_format_data_unknown = 0x0,
};

enum so_media_format_mode {
	so_media_format_mode_disk = 0x1,
	so_media_format_mode_linear = 0x2,
	so_media_format_mode_optical = 0x3,

	so_media_format_mode_unknown = 0x0,
};

enum so_media_status {
	so_media_status_erasable = 0x1,
	so_media_status_error = 0x2,
	so_media_status_foreign = 0x3,
	so_media_status_in_use = 0x4,
	so_media_status_locked = 0x5,
	so_media_status_needs_replacement = 0x6,
	so_media_status_new = 0x7,
	so_media_status_pooled = 0x8,

	so_media_status_unknown = 0x0,
};

enum so_media_type {
	so_media_type_cleaning = 0x1,
	so_media_type_rewritable = 0x2,
	so_media_type_worm = 0x3,

	so_media_type_unknown = 0x0,
};

enum so_pool_autocheck_mode {
	so_pool_autocheck_quick_mode = 0x1,
	so_pool_autocheck_thorough_mode = 0x2,
	so_pool_autocheck_mode_none = 0x3,

	so_pool_autocheck_mode_unknown = 0x0,
};

enum so_pool_unbreakable_level {
	so_pool_unbreakable_level_archive = 0x1,
	so_pool_unbreakable_level_file = 0x2,
	so_pool_unbreakable_level_none = 0x3,

	so_pool_unbreakable_level_unknown = 0x0,
};


struct so_media {
	char uuid[37];
	char * label;
	char * medium_serial_number;
	char * name;

	enum so_media_status status;

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

	size_t block_size;
	size_t free_block; // in block size, not in bytes
	size_t total_block; // size in block

	unsigned int nb_volumes;
	bool append;
	enum so_media_type type;
	bool write_lock;

	struct so_archive_format * archive_format;
	struct so_media_format * media_format;
	struct so_pool * pool;

	void * private_data;
	void (*free_private_data)(void * data);
	struct so_value * db_data;
};

struct so_media_format {
	char name[64];

	unsigned char density_code;
	enum so_media_format_data_type type;
	enum so_media_format_mode mode;

	long max_load_count;
	long max_read_count;
	long max_write_count;
	long max_operation_count;

	long life_span;

	ssize_t capacity;
	ssize_t block_size;

	bool support_partition;
	bool support_mam;

	struct so_value * db_data;
};

struct so_pool {
	char uuid[37];
	char * name;

	enum so_pool_autocheck_mode auto_check;
	bool growable;
	enum so_pool_unbreakable_level unbreakable_level;
	bool rewritable;
	bool deleted;

	struct so_archive_format * archive_format;
	struct so_media_format * media_format;

	struct so_value * db_data;
};


struct so_value * so_media_convert(struct so_media * media) __attribute__((warn_unused_result));
struct so_media * so_media_dup(struct so_media * media) __attribute__((warn_unused_result));
struct so_value * so_media_format_convert(struct so_media_format * format) __attribute__((warn_unused_result));
const char * so_media_get_name(const struct so_media * media);
void so_media_format_sync(struct so_media_format * format, struct so_value * new_format);
struct so_media * so_media_new(struct so_value * media) __attribute__((warn_unused_result));
void so_media_sync(struct so_media * media, struct so_value * new_media);

struct so_value * so_pool_convert(struct so_pool * pool) __attribute__((warn_unused_result));
struct so_pool * so_pool_dup(struct so_pool * pool) __attribute__((warn_unused_result));
void so_pool_sync(struct so_pool * pool, struct so_value * new_pool);

int so_media_format_cmp(struct so_media_format * f1, struct so_media_format * f2);
struct so_media_format * so_media_format_dup(const struct so_media_format * format);

void so_media_free(struct so_media * media);
void so_media_free2(void * media);
void so_media_format_free(struct so_media_format * format);
void so_pool_free(struct so_pool * pool);
void so_pool_free2(void * pool);

const char * so_media_format_data_type_to_string(enum so_media_format_data_type type, bool translate);
enum so_media_format_data_type so_media_string_to_format_data_type(const char * type, bool translate);

const char * so_media_format_mode_to_string(enum so_media_format_mode mode, bool translate);
enum so_media_format_mode so_media_string_to_format_mode(const char * mode, bool translate);

const char * so_media_status_to_string(enum so_media_status status, bool translate);
enum so_media_status so_media_string_to_status(const char * status, bool translate);

enum so_media_type so_media_string_to_type(const char * type, bool translate);
const char * so_media_type_to_string(enum so_media_type type, bool translate);

const char * so_pool_autocheck_mode_to_string(enum so_pool_autocheck_mode mode, bool translate);
enum so_pool_autocheck_mode so_pool_string_to_autocheck_mode(const char * mode, bool translate);

enum so_pool_unbreakable_level so_pool_string_to_unbreakable_level(const char * level, bool translate);
const char * so_pool_unbreakable_level_to_string(enum so_pool_unbreakable_level level, bool translate);

#endif
