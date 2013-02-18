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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Sun, 03 Feb 2013 10:30:58 +0100                            *
\****************************************************************************/

#ifndef __STONE_LIBRARY_COMMON_H__
#define __STONE_LIBRARY_COMMON_H__

#include <libstone/library/changer.h>
#include <stoned/library/slot.h>

struct st_changer;
struct st_drive;
struct st_database_connection;
struct st_hashtable;
struct st_media_format;
struct st_pool;

void st_scsi_tape_drive_setup(struct st_drive * drive);

void st_standalone_drive_setup(struct st_changer * changer);
void st_scsi_changer_setup(struct st_changer * changer, struct st_database_connection * connection);

void st_changer_add(struct st_changer * vtl);
void st_changer_stop(void);
void st_changer_sync(struct st_database_connection * connection);

struct st_slot_iterator * st_slot_iterator_by_new_media2(struct st_media_format * format, struct st_changer * changers, unsigned int nb_changers);
struct st_slot_iterator * st_slot_iterator_by_pool2(struct st_pool * pool, struct st_changer * changers, unsigned int nb_changers);

void st_vtl_add(const struct st_hashtable * params);

#endif

