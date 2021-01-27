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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_SLOT_H__
#define __LIBSTORIQONE_SLOT_H__

// bool
#include <stdbool.h>

struct so_changer;
struct so_drive;
struct so_media;
struct so_value;

struct so_slot {
	struct so_changer * changer;
	unsigned int index;
	struct so_drive * drive;

	struct so_media * media;
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

struct so_value * so_slot_convert(struct so_slot * slot) __attribute__((warn_unused_result));
void so_slot_free(struct so_slot * slot);
void so_slot_free2(void * slot);
void so_slot_sync(struct so_slot * slot, struct so_value * new_slot);
void so_slot_swap(struct so_slot * sa, struct so_slot * sb);

#endif

