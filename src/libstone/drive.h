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

#ifndef __LIBSTONE_DRIVE_P_H__
#define __LIBSTONE_DRIVE_P_H__

#include <libstone/drive.h>

#define st_drive_v1 st_drive

struct st_value;

struct st_value * st_drive_convert_v1(struct st_drive_v1 * drive, bool with_slot) __attribute__((nonnull,warn_unused_result));
void st_drive_free_v1(struct st_drive_v1 * drive) __attribute__((nonnull));
void st_drive_free2_v1(void * drive) __attribute__((nonnull));
const char * st_drive_status_to_string_v1(enum st_drive_status status);
enum st_drive_status st_drive_string_to_status_v1(const char * status) __attribute__((nonnull));
void st_drive_sync_v1(struct st_drive_v1 * drive, struct st_value * new_drive, bool with_slot) __attribute__((nonnull));

#endif
