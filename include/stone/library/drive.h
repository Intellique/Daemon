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
*  Last modified: Wed, 25 Jan 2012 11:20:42 +0100                         *
\*************************************************************************/

#ifndef __STONE_LIBRARY_DRIVE_H__
#define __STONE_LIBRARY_DRIVE_H__

// ssize_t
#include <sys/types.h>

struct st_changer;
struct st_slot;
struct st_stream_reader;

enum st_drive_status {
	ST_DRIVE_CLEANING,
	ST_DRIVE_EMPTY_IDLE,
	ST_DRIVE_ERASING,
	ST_DRIVE_ERROR,
	ST_DRIVE_LOADED_IDLE,
	ST_DRIVE_LOADING,
	ST_DRIVE_POSITIONING,
	ST_DRIVE_READING,
	ST_DRIVE_REWINDING,
	ST_DRIVE_UNKNOWN,
	ST_DRIVE_UNLOADING,
	ST_DRIVE_WRITING,
};

struct st_drive {
	long id;
	char * device;
	char * scsi_device;
	enum st_drive_status status;
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
		int (*eod)(struct st_drive * drive);
		ssize_t (*get_block_size)(struct st_drive * drive);
		struct st_stream_reader * (*get_reader)(struct st_drive * drive);
		struct st_stream_writer * (*get_writer)(struct st_drive * drive);
		int (*read_mam)(struct st_drive * drive);
		void (*reset)(struct st_drive * drive);
		int (*rewind_file)(struct st_drive * drive);
		int (*rewind_tape)(struct st_drive * drive);
		int (*set_file_position)(struct st_drive * drive, int file_position);
	} * ops;
	void * data;

	struct st_ressource * lock;

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
	unsigned char best_density_code;
	unsigned char density_code;
	double operation_duration;
	time_t last_clean;
};


const char * st_drive_status_to_string(enum st_drive_status status);
enum st_drive_status st_drive_string_to_status(const char * status);

#endif

