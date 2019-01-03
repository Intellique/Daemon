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
*  Copyright (C) 2013-2019, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_DRIVE_H__
#define __LIBSTORIQONE_DRIVE_H__

// bool
#include <stdbool.h>
// time_t
#include <sys/types.h>

#include <libstoriqone/media.h>

struct so_value;

enum so_drive_status {
	so_drive_status_cleaning = 0x1,
	so_drive_status_empty_idle = 0x2,
	so_drive_status_erasing = 0x3,
	so_drive_status_error = 0x4,
	so_drive_status_loaded_idle = 0x5,
	so_drive_status_loading = 0x6,
	so_drive_status_positioning = 0x7,
	so_drive_status_reading = 0x8,
	so_drive_status_rewinding = 0x9,
	so_drive_status_unloading = 0xa,
	so_drive_status_writing = 0xb,

	so_drive_status_unknown = 0x0,
};

struct so_drive {
	char * model;
	char * vendor;
	char * revision;
	char * serial_number;

	enum so_drive_status status;
	bool enable;

	unsigned char density_code;
	enum so_media_format_mode mode;
	double operation_duration;
	time_t last_clean;
	bool is_empty;

	struct so_changer * changer;
	unsigned int index;
	struct so_slot * slot;

	struct so_drive_ops * ops;

	void * data;
	/**
	 * \brief Private data used by database plugin
	 */
	void * db_data;
};

struct so_value * so_drive_convert(struct so_drive * drive, bool with_slot) __attribute__((warn_unused_result));
void so_drive_free(struct so_drive * drive);
void so_drive_free2(void * drive);
const char * so_drive_status_to_string(enum so_drive_status status, bool translate);
enum so_drive_status so_drive_string_to_status(const char * status, bool translate);
void so_drive_sync(struct so_drive * drive, struct so_value * new_drive, bool with_slot);

#endif
