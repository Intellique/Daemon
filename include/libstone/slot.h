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
struct st_value;

struct st_slot {
	struct st_changer * changer;
	unsigned int index;
	struct st_drive * drive;

	struct st_media * media;
	char * volume_name;
	bool full;

	bool is_ie_port;
	bool enable;

	void * data;
	/**
	 * \brief Private data used by database plugin
	 */
	void * db_data;
};

struct st_value * st_slot_convert(struct st_slot * slot) __attribute__((nonnull,warn_unused_result));
void st_slot_free(struct st_slot * slot) __attribute__((nonnull));
void st_slot_free2(void * slot) __attribute__((nonnull));
void st_slot_sync(struct st_slot * slot, struct st_value * new_slot) __attribute__((nonnull));

#endif

