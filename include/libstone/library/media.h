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
*  Copyright (C) 2013-2015, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Mon, 22 Dec 2014 13:22:00 +0100                            *
\****************************************************************************/

#ifndef __STONE_LIBRARY_MEDIA_H__
#define __STONE_LIBRARY_MEDIA_H__

// bool
#include <stdbool.h>
// ssize_t, time_t
#include <sys/types.h>

struct st_archive;
struct st_database_connection;
struct st_drive;
struct st_job;
struct st_pool;
struct st_ressource;
struct st_tape_format;

enum st_media_location {
	st_media_location_indrive,
	st_media_location_offline,
	st_media_location_online,

	st_media_location_unknown,
};

enum st_media_status {
	st_media_status_erasable,
	st_media_status_error,
	st_media_status_foreign,
	st_media_status_in_use,
	st_media_status_locked,
	st_media_status_needs_replacement,
	st_media_status_new,
	st_media_status_pooled,

	st_media_status_unknown,
};

enum st_media_type {
	st_media_type_cleaning,
	st_media_type_readonly,
	st_media_type_rewritable,
	st_media_type_worm, //TODO: à ajouter dans la base de données

	st_media_type_unknown,
};

enum st_media_format_data_type {
	st_media_format_data_audio,
	st_media_format_data_cleaning,
	st_media_format_data_data,
	st_media_format_data_video,

	st_media_format_data_unknown,
};

enum st_media_format_mode {
	st_media_format_mode_disk,
	st_media_format_mode_linear,
	st_media_format_mode_optical,

	st_media_format_mode_unknown,
};

enum st_pool_autocheck_mode {
	st_pool_autocheck_quick_mode,
	st_pool_autocheck_thorough_mode,
	st_pool_autocheck_mode_none,

	st_pool_autocheck_mode_unknown,
};

enum st_pool_unbreakable_level {
	st_pool_unbreakable_level_archive,
	st_pool_unbreakable_level_file,
	st_pool_unbreakable_level_none,

	st_pool_unbreakable_level_unknown,
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

	struct st_ressource * lock;
	volatile bool locked;

	void * data;
	void * db_data;
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

	void * db_data;
};


const char * st_media_format_data_to_string(enum st_media_format_data_type type);
const char * st_media_format_mode_to_string(enum st_media_format_mode mode);
const char * st_media_location_to_string(enum st_media_location location);
const char * st_media_status_to_string(enum st_media_status status);
const char * st_media_type_to_string(enum st_media_type type);
enum st_media_location st_media_string_to_location(const char * location);
enum st_media_status st_media_string_to_status(const char * status);
enum st_media_format_data_type st_media_string_to_format_data(const char * type);
enum st_media_format_mode st_media_string_to_format_mode(const char * mode);
enum st_media_type st_media_string_to_type(const char * type);
const char * st_pool_autocheck_mode_to_string(enum st_pool_autocheck_mode mode);
enum st_pool_autocheck_mode st_pool_autocheck_string_to_mode(const char * mode);
const char * st_pool_unbreakable_level_to_string(enum st_pool_unbreakable_level level);
enum st_pool_unbreakable_level st_pool_unbreakable_string_to_level(const char * level);

void st_media_add(struct st_media * media);
bool st_media_check_header(struct st_drive * dr);
ssize_t st_media_get_available_size(struct st_media * media);
struct st_media * st_media_get_by_job(struct st_job * job, struct st_database_connection * connection);
struct st_media * st_media_get_by_label(const char * label);
struct st_media * st_media_get_by_medium_serial_number(const char * medium_serial_number);
struct st_media * st_media_get_by_uuid(const char * uuid);
int st_media_read_header(struct st_drive * dr);
int st_media_write_header(struct st_drive * dr, struct st_pool * pool, struct st_database_connection * db_connect);

struct st_media_format * st_media_format_get_by_density_code(unsigned char density_code, enum st_media_format_mode mode);
struct st_media_format * st_media_format_get_by_name(const char * format, enum st_media_format_mode mode);

struct st_pool * st_pool_get_by_archive(struct st_archive * archive, struct st_database_connection * connection);
struct st_pool * st_pool_get_by_job(struct st_job * job, struct st_database_connection * connection);
struct st_pool * st_pool_get_by_uuid(const char * uuid);
int st_pool_sync(struct st_pool * pool, struct st_database_connection * connection);

#endif

