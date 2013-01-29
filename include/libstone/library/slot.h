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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Fri, 07 Dec 2012 17:01:16 +0100                         *
\*************************************************************************/

#ifndef __STONE_LIBRARY_SLOT_H__
#define __STONE_LIBRARY_SLOT_H__

// bool
#include <stdbool.h>

struct st_changer;
struct st_drive;
struct st_media;

enum st_slot_type {
	st_slot_type_drive,
	st_slot_type_import_export,
	st_slot_type_storage,
	st_slot_type_transport,

	st_slot_type_unkown,
};

struct st_slot {
	struct st_changer * changer;
	struct st_drive * drive;
	struct st_media * media;

	char * volume_name;
	bool full;
	enum st_slot_type type;

	struct st_ressource * lock;

	void * data;

	/**
	 * \brief Private data used by database plugin
	 */
	void * db_data;
};

struct st_slot_iterator {
	struct st_slot_iterator_ops {
		void (*free)(struct st_slot_iterator * iterator);
		bool (*has_next)(struct st_slot_iterator * iterator);
		struct st_slot * (*next)(struct st_slot_iterator * iterator);
	} * ops;
	void * data;
};

const char * st_slot_type_to_string(enum st_slot_type type);
enum st_slot_type st_slot_string_to_type(const char * type);

#endif

