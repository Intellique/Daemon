/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
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
*  Last modified: Thu, 08 Dec 2011 16:43:35 +0100                         *
\*************************************************************************/

#ifndef __STORIQARCHIVER_LIBRARY_DRIVE_H__
#define __STORIQARCHIVER_LIBRARY_DRIVE_H__

// ssize_t
#include <sys/types.h>

struct sa_changer;
struct sa_slot;
struct sa_stream_reader;

enum sa_drive_status {
	SA_DRIVE_CLEANING,
	SA_DRIVE_EMPTY_IDLE,
	SA_DRIVE_ERASING,
	SA_DRIVE_ERROR,
	SA_DRIVE_LOADED_IDLE,
	SA_DRIVE_LOADING,
	SA_DRIVE_POSITIONING,
	SA_DRIVE_READING,
	SA_DRIVE_REWINDING,
	SA_DRIVE_UNKNOWN,
	SA_DRIVE_UNLOADING,
	SA_DRIVE_WRITING,
};

struct sa_drive {
	long id;
	char * device;
	char * scsi_device;
	enum sa_drive_status status;
	char * model;
	char * vendor;
	char * revision;
	char * serial_number;

	int host;
	int target;
	int channel;
	int bus;

	struct sa_changer * changer;
	struct sa_slot * slot;

	struct sa_drive_ops {
		int (*eject)(struct sa_drive * drive);
		int (*eod)(struct sa_drive * drive);
		ssize_t (*get_block_size)(struct sa_drive * drive);
		struct sa_stream_reader * (*get_reader)(struct sa_drive * drive);
		struct sa_stream_writer * (*get_writer)(struct sa_drive * drive);
		void (*reset)(struct sa_drive * drive);
		int (*rewind_file)(struct sa_drive * drive);
		int (*rewind_tape)(struct sa_drive * drive);
		int (*set_file_position)(struct sa_drive * drive, int file_position);
	} * ops;
	void * data;

	unsigned int file_position;
	unsigned int nb_files;
	ssize_t block_number;

	unsigned char is_bottom_of_tape;
	unsigned char is_end_of_file;
	unsigned char is_end_of_tape;
	unsigned char is_writable;
	unsigned char is_online;
	unsigned char is_door_opened;

	// block size defined for this drive
	// 0 => soft block
	ssize_t block_size;
	unsigned char density_code;
	double operation_duration;
	time_t last_clean;
};


const char * sa_drive_status_to_string(enum sa_drive_status status);
enum sa_drive_status sa_drive_string_to_status(const char * status);

#endif

