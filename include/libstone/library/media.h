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
*  Last modified: Fri, 27 Jul 2012 09:42:56 +0200                         *
\*************************************************************************/

#ifndef __STONE_LIBRARY_MEDIA_H__
#define __STONE_LIBRARY_MEDIA_H__

// ssize_t, time_t
#include <sys/types.h>

struct st_drive;
struct st_pool;
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
	st_media_type_readonly,
	st_media_type_rewritable,
	st_media_type_cleanning,
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


struct st_media {
	char uuid[37];
	char * label;
	char * medium_serial_number;
	char * name;

	enum st_media_status status;
	enum st_media_location location;

	time_t first_used;
	time_t use_before;

	long load_count;
	long read_count;
	long write_count;

	ssize_t block_size;
	ssize_t end_position; // in block size, not in bytes
	ssize_t available_block; // in block size, not in bytes

	unsigned int nb_volumes;
	enum st_media_type type;

	struct st_tape_format * format;
	struct st_pool * pool;
};

struct st_media_format {
	char name[64];

	unsigned char density_code;
	enum st_media_format_data_type type;
	enum st_media_format_mode mode;

	long max_load_count;
	long max_read_count;
	long max_write_count;
	long max_op_count;

	long long life_span;

	ssize_t capacity;
	ssize_t block_size;

	char support_partition;
	char support_mam;
};

struct st_pool {
	char uuid[37];
	char name[64];
	unsigned char growable;

	struct st_tape_format * format;
};


const char * st_media_format_data_to_string(enum st_media_format_data_type type);
const char * st_media_format_mode_to_string(enum st_media_format_mode mode);
const char * st_media_location_to_string(enum st_media_location location);
const char * st_media_status_to_string(enum st_media_status status);
enum st_media_location st_media_string_to_location(const char * location);
enum st_media_status st_media_string_to_status(const char * status);
enum st_media_format_data_type st_media_string_to_format_data(const char * type);
enum st_media_format_mode st_media_string_to_format_mode(const char * mode);

#endif

