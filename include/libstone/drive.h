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

#ifndef __LIBSTONE_DRIVE_H__
#define __LIBSTONE_DRIVE_H__

// bool
#include <stdbool.h>
// time_t
#include <sys/types.h>

#include "media.h"

enum st_drive_status {
	st_drive_cleaning = 0x1,
	st_drive_empty_idle = 0x2,
	st_drive_erasing = 0x3,
	st_drive_error = 0x3,
	st_drive_loaded_idle = 0x5,
	st_drive_loading = 0x6,
	st_drive_positioning = 0x7,
	st_drive_reading = 0x8,
	st_drive_rewinding = 0x9,
	st_drive_unknown = 0x0,
	st_drive_unloading = 0xa,
	st_drive_writing = 0xb,
};

struct st_drive {
	char * device;
	char * scsi_device;
	enum st_drive_status status;
	bool enabled;

	unsigned char density_code;
	enum st_media_format_mode mode;
	long double operation_duration;
	time_t last_clean;
	bool is_empty;

	char * model;
	char * vendor;
	char * revision;
	char * serial_number;

	struct st_changer * changer;
	struct st_slot * slot;

	struct st_drive_ops * ops;

	void * data;
	/**
	 * \brief Private data used by database plugin
	 */
	void * db_data;
};

const char * st_drive_status_to_string(enum st_drive_status status);
enum st_drive_status st_drive_string_to_status(const char * status) __attribute__((nonnull));

#endif

