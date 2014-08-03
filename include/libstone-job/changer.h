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

#ifndef __STONEJOB_CHANGER_H__
#define __STONEJOB_CHANGER_H__

#include <libstone/changer.h>

struct st_drive;
struct st_media;
struct st_slot;
struct st_value;

struct st_changer_ops {
	int (*load)(struct st_changer * changer, struct st_slot * from, struct st_drive * to);
	int (*release_all_media)(struct st_changer * changer);
	int (*release_media)(struct st_changer * changer, struct st_slot * slot);
	int (*reserve_media)(struct st_changer * changer, struct st_slot * slot);
	int (*sync)(struct st_changer * changer);
	int (*unload)(struct st_changer * changer, struct st_drive * from);
};

void stj_changer_set_config(struct st_value * config) __attribute__((nonnull));
int stj_changer_sync_all(void);

#endif

