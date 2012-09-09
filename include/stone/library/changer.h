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
*  Last modified: Sun, 09 Sep 2012 11:22:30 +0200                         *
\*************************************************************************/

#ifndef __STONE_LIBRARY_CHANGER_H__
#define __STONE_LIBRARY_CHANGER_H__

// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>

struct st_drive;
struct st_pool;
struct st_ressource;
struct st_slot;
struct st_tape;

enum st_changer_status {
	ST_CHANGER_ERROR,
	ST_CHANGER_EXPORTING,
	ST_CHANGER_IDLE,
	ST_CHANGER_IMPORTING,
	ST_CHANGER_LOADING,
	ST_CHANGER_UNKNOWN,
	ST_CHANGER_UNLOADING,
};

struct st_changer {
	long id;
	char * device;
	enum st_changer_status status;
	char * model;
	char * vendor;
	char * revision;
	char * serial_number;
	bool barcode;

	int host;
	int target;
	int channel;
	int bus;

	struct st_drive * drives;
	unsigned int nb_drives;
	struct st_slot * slots;
	unsigned int nb_slots;
	bool enabled;

	struct st_changer_ops {
		int (*can_load)();
		struct st_slot * (*get_tape)(struct st_changer * ch, struct st_tape * tape);
		int (*load)(struct st_changer * ch, struct st_slot * from, struct st_drive * to);
		int (*unload)(struct st_changer * ch, struct st_drive * from);
	} * ops;
	void * data;

	struct st_ressource * lock;

	// for scsi use only
	int transport_address;
};

struct st_slot {
	long id;
	struct st_changer * changer;
	struct st_drive * drive;
	struct st_tape * tape;

	char volume_name[37];
	bool full;
	bool is_import_export_slot;

	struct st_ressource * lock;

	// for scsi use only
	int address;
	int src_address;
};


ssize_t st_changer_get_available_size_by_pool(struct st_pool * pool);
ssize_t st_changer_get_available_size_of_new_tapes(void);
struct st_changer * st_changer_get_by_tape(struct st_tape * tape);
struct st_changer * st_changer_get_first_changer(void);
struct st_changer * st_changer_get_next_changer(struct st_changer * changer);
const char * st_changer_status_to_string(enum st_changer_status status);
enum st_changer_status st_changer_string_to_status(const char * status);
int st_changer_setup(void);

#endif

