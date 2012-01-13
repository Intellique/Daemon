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
*  Last modified: Fri, 13 Jan 2012 15:43:54 +0100                         *
\*************************************************************************/

#ifndef __STONE_LIBRARY_TAPE_H__
#define __STONE_LIBRARY_TAPE_H__

// ssize_t, time_t
#include <sys/types.h>

enum st_tape_location {
	ST_TAPE_LOCATION_INDRIVE,
	ST_TAPE_LOCATION_OFFLINE,
	ST_TAPE_LOCATION_ONLINE,
	ST_TAPE_LOCATION_UNKNOWN,
};

enum st_tape_status {
	ST_TAPE_STATUS_ERASABLE,
	ST_TAPE_STATUS_ERROR,
	ST_TAPE_STATUS_FOREIGN,
	ST_TAPE_STATUS_IN_USE,
	ST_TAPE_STATUS_LOCKED,
	ST_TAPE_STATUS_NEEDS_REPLACEMENT,
	ST_TAPE_STATUS_NEW,
	ST_TAPE_STATUS_POOLED,
	ST_TAPE_STATUS_UNKNOWN,
};

enum st_tape_format_data_type {
	ST_TAPE_FORMAT_DATA_AUDIO,
	ST_TAPE_FORMAT_DATA_CLEANING,
	ST_TAPE_FORMAT_DATA_DATA,
	ST_TAPE_FORMAT_DATA_VIDEO,
	ST_TAPE_FORMAT_DATA_UNKNOWN,
};

enum st_tape_format_mode {
	ST_TAPE_FORMAT_MODE_DISK,
	ST_TAPE_FORMAT_MODE_LINEAR,
	ST_TAPE_FORMAT_MODE_OPTICAL,
	ST_TAPE_FORMAT_MODE_UNKNOWN,
};

struct st_drive;
struct st_pool;
struct st_tape_format;

struct st_tape {
	long id;
	char uuid[37];
	char label[37];
	char name[256];
	enum st_tape_status status;
	enum st_tape_location location;
	time_t first_used;
	time_t use_before;
	long load_count;
	long read_count;
	long write_count;
	ssize_t end_position;
	ssize_t block_size;
	int nb_files;
	char has_partition;
	struct st_tape_format * format;
	struct st_pool * pool;
};

struct st_tape_format {
	long id;
	char name[64];
	unsigned char density_code;
	enum st_tape_format_data_type type;
	enum st_tape_format_mode mode;
	long max_load_count;
	long max_read_count;
	long max_write_count;
	long max_op_count;
	long life_span;
	ssize_t capacity;
	ssize_t block_size;
	char support_partition;
};

struct st_pool {
	long id;
	char uuid[37];
	char name[64];
	long retention;
	time_t retention_limit;
	unsigned char auto_recycle;
	struct st_tape_format * format;
};


const char * st_tape_format_data_to_string(enum st_tape_format_data_type type);
const char * st_tape_format_mode_to_string(enum st_tape_format_mode mode);
const char * st_tape_location_to_string(enum st_tape_location location);
const char * st_tape_status_to_string(enum st_tape_status status);
enum st_tape_location st_tape_string_to_location(const char * location);
enum st_tape_status st_tape_string_to_status(const char * status);
enum st_tape_format_data_type st_tape_string_to_format_data(const char * type);
enum st_tape_format_mode st_tape_string_to_format_mode(const char * mode);

struct st_tape * st_tape_get_by_id(long id);
struct st_tape * st_tape_get_by_uuid(const char * uuid);
struct st_tape * st_tape_new(struct st_drive * dr);
int st_tape_write_header(struct st_drive * dr, struct st_pool * pool);

struct st_tape_format * st_tape_format_get_by_id(long id);
struct st_tape_format * st_tape_format_get_by_density_code(unsigned char density_code);

struct st_pool * st_pool_get_by_id(long id);
struct st_pool * st_pool_get_by_uuid(const char * uuid);
int st_pool_sync(struct st_pool * pool);

#endif

