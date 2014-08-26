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

#ifndef __STONEJOB_DRIVE_H__
#define __STONEJOB_DRIVE_H__

#include <libstone/drive.h>

struct st_drive;
struct st_pool;

struct st_drive_ops {
	bool (*check_header)(struct st_drive * drive);
	bool (*check_support)(struct st_drive * drive, struct st_media_format * format, bool for_writing);
	ssize_t (*find_best_block_size)(struct st_drive * drive);
	int (*format_media)(struct st_drive * drive, struct st_pool * pool);
	struct st_stream_reader * (*get_raw_reader)(struct st_drive * drive, int file_position, const char * cookie);
	char * (*lock)(struct st_drive * drive);
	int (*sync)(struct st_drive * drive);
};

#endif

