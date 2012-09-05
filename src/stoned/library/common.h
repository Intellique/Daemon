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
*  Last modified: Wed, 05 Sep 2012 18:52:07 +0200                         *
\*************************************************************************/

#ifndef __STONE_LIBRARY_COMMON_H__
#define __STONE_LIBRARY_COMMON_H__

#include <stone/library/changer.h>

struct st_changer;
struct st_database_connection;
struct st_drive;

void st_changer_update_status(struct st_database_connection * connect);

void st_drive_setup(struct st_drive * drive);

void st_fakechanger_setup(struct st_changer * changer);
void st_realchanger_setup(struct st_changer * changer);

#endif

