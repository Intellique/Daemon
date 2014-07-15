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

#ifndef __LIBSTONE_SLOT_P_H__
#define __LIBSTONE_SLOT_P_H__

#include <libstone/slot.h>

#define st_slot_v1 st_slot

struct st_value * st_slot_convert_v1(struct st_slot * slot) __attribute__((nonnull,warn_unused_result));
void st_slot_free_v1(struct st_slot * slot) __attribute__((nonnull));
void st_slot_free2_v1(void * slot) __attribute__((nonnull));
const char * st_slot_type_to_string_v1(enum st_slot_type type);
enum st_slot_type st_slot_string_to_type_v1(const char * type) __attribute__((nonnull));
void st_slot_sync_v1(struct st_slot * slot, struct st_value * new_slot) __attribute__((nonnull));

#endif

