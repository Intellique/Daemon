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
*  Last modified: Sat, 17 Dec 2011 17:39:59 +0100                         *
\*************************************************************************/

#ifndef __STORIQARCHIVER_LIBRARY_TAPE_H__
#define __STORIQARCHIVER_LIBRARY_TAPE_H__

// ssize_t, time_t
#include <sys/types.h>

enum sa_tape_location {
    SA_TAPE_LOCATION_OFFLINE,
    SA_TAPE_LOCATION_ONLINE,
    SA_TAPE_LOCATION_UNKNOWN,
};

enum sa_tape_status {
    SA_TAPE_STATUS_ERASABLE,
    SA_TAPE_STATUS_ERROR,
    SA_TAPE_STATUS_FOREIGN,
    SA_TAPE_STATUS_IN_USE,
    SA_TAPE_STATUS_LOCKED,
    SA_TAPE_STATUS_NEEDS_REPLACEMENT,
    SA_TAPE_STATUS_NEW,
    SA_TAPE_STATUS_POOLED,
    SA_TAPE_STATUS_UNKNOWN,
};

enum sa_tape_format_data_type {
    SA_TAPE_FORMAT_DATA_AUDIO,
    SA_TAPE_FORMAT_DATA_CLEANING,
    SA_TAPE_FORMAT_DATA_DATA,
    SA_TAPE_FORMAT_DATA_VIDEO,
    SA_TAPE_FORMAT_DATA_UNKNOWN,
};

enum sa_tape_format_mode {
    SA_TAPE_FORMAT_MODE_DISK,
    SA_TAPE_FORMAT_MODE_LINEAR,
    SA_TAPE_FORMAT_MODE_OPTICAL,
    SA_TAPE_FORMAT_MODE_UNKNOWN,
};

struct sa_drive;
struct sa_pool;
struct sa_tape_format;

struct sa_tape {
	long id;
    char label[37];
    char name[64];
    enum sa_tape_status status;
    enum sa_tape_location location;
    time_t first_used;
    time_t use_before;
    long load_count;
    long read_count;
    long write_count;
    ssize_t end_position;
    ssize_t block_size;
    unsigned int nb_files;
    unsigned char has_partition;
    struct sa_tape_format * format;
    struct sa_pool * pool;
};

struct sa_tape_format {
	long id;
    char name[64];
	unsigned char density_code;
    enum sa_tape_format_data_type type;
    enum sa_tape_format_mode mode;
    long max_load_count;
    long max_read_count;
    long max_write_count;
    long max_op_count;
    long life_span;
    ssize_t capacity;
    ssize_t block_size;
    char support_partition;
};

struct sa_pool {
    long id;
    char name[64];
    unsigned long retention;
    time_t retention_limit;
    unsigned char auto_recycle;
    struct sa_tape_format * format;
};


const char * sa_tape_format_data_to_string(enum sa_tape_format_data_type type);
const char * sa_tape_format_mode_to_string(enum sa_tape_format_mode mode);
const char * sa_tape_location_to_string(enum sa_tape_location location);
const char * sa_tape_status_to_string(enum sa_tape_status status);
enum sa_tape_location sa_tape_string_to_location(const char * location);
enum sa_tape_status sa_tape_string_to_status(const char * status);
enum sa_tape_format_data_type sa_tape_string_to_format_data(const char * type);
enum sa_tape_format_mode sa_tape_string_to_format_mode(const char * mode);

void sa_tape_detect(struct sa_drive * dr);
struct sa_tape * sa_tape_new(struct sa_drive * dr);

struct sa_tape_format * sa_tape_format_get_by_density_code(unsigned char density_code);

#endif

