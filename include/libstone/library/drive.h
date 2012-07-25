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
*  Last modified: Wed, 25 Jul 2012 13:04:30 +0200                         *
\*************************************************************************/

#ifndef __STONE_LIBRARY_DRIVE_H__
#define __STONE_LIBRARY_DRIVE_H__

// ssize_t
#include <sys/types.h>

struct st_changer;
struct st_database_connection;
struct st_slot;
struct st_stream_reader;

enum st_drive_status {
	st_drive_cleaning,
	st_drive_empty_idle,
	st_drive_erasing,
	st_drive_error,
	st_drive_loaded_idle,
	st_drive_loading,
	st_drive_positioning,
	st_drive_reading,
	st_drive_rewinding,
	st_drive_unknown,
	st_drive_unloading,
	st_drive_writing,
};

struct st_drive {
	char * device;
	char * scsi_device;
	enum st_drive_status status;
	unsigned char enabled;

	char * model;
	char * vendor;
	char * revision;
	char * serial_number;

	int host;
	int target;
	int channel;
	int bus;

	struct st_changer * changer;
	struct st_slot * slot;

	struct st_drive_ops {
		int (*eject)(struct st_drive * drive);
		int (*format)(struct st_drive * drive, int quick_mode);
		struct st_stream_reader * (*get_reader)(struct st_drive * drive, int file_position);
		struct st_stream_writer * (*get_writer)(struct st_drive * drive, int file_position);
		int (*update_media_info)(struct st_drive * drive);
	} * ops;
	void * data;

	struct st_ressource * lock;
};

const char * st_drive_status_to_string(enum st_drive_status status);
enum st_drive_status st_drive_string_to_status(const char * status);

#endif

