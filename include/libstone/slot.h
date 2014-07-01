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

#ifndef __LIBSTONE_SLOT_H__
#define __LIBSTONE_SLOT_H__

// bool
#include <stdbool.h>

struct st_changer;
struct st_drive;
struct st_media;

enum st_slot_type {
	st_slot_type_drive = 0x1,
	st_slot_type_import_export = 0x2,
	st_slot_type_storage = 0x3,
	st_slot_type_transport = 0x4,

	st_slot_type_unkown = 0x0,
};

struct st_slot {
	struct st_changer * changer;
	struct st_drive * drive;
	struct st_media * media;

	unsigned int index;
	char * volume_name;
	bool full;
	enum st_slot_type type;
	bool enable;

	void * data;
	/**
	 * \brief Private data used by database plugin
	 */
	void * db_data;
};

void st_slot_free(struct st_slot * slot) __attribute__((nonnull));
void st_slot_free2(void * slot) __attribute__((nonnull));
const char * st_slot_type_to_string(enum st_slot_type type);
enum st_slot_type st_slot_string_to_type(const char * type) __attribute__((nonnull));

#endif

