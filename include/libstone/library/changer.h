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
*  Last modified: Tue, 24 Jul 2012 22:59:07 +0200                         *
\*************************************************************************/

#ifndef __STONE_LIBRARY_CHANGER_H__
#define __STONE_LIBRARY_CHANGER_H__

struct st_database_connection;
struct st_drive;
struct st_media;
struct st_slot;

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
	char * device;
	enum st_changer_status status;
	unsigned char enabled;

	char * model;
	char * vendor;
	char * revision;
	char * serial_number;
	int barcode;

	int host;
	int target;
	int channel;
	int bus;

	struct st_drive * drives;
	unsigned int nb_drives;

	struct st_slot * slots;
	unsigned int nb_slots;

	struct st_changer_ops {
		int (*load_media)(struct st_changer * ch, struct st_media * from, struct st_drive * to);
		int (*load_slot)(struct st_changer * ch, struct st_slot * from, struct st_drive * to);
		int (*sync_db)(struct st_changer * ch, struct st_database_connection * connection);
		int (*unload)(struct st_changer * ch, struct st_drive * from);
	} * ops;
	void * data;
};

const char * st_changer_status_to_string(enum st_changer_status status);
enum st_changer_status st_changer_string_to_status(const char * status);

#endif

