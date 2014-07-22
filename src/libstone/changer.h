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

#ifndef __LIBSTONE_CHANGER_P_H__
#define __LIBSTONE_CHANGER_P_H__

#include <libstone/changer.h>

#define st_changer_v1 st_changer

const char * st_changer_action_to_string_v1(enum st_changer_action action);
struct st_value * st_changer_convert_v1(struct st_changer * changer) __attribute__((nonnull,warn_unused_result));
void st_changer_free_v1(struct st_changer * changer) __attribute__((nonnull));
void st_changer_free2_v1(void * changer) __attribute__((nonnull));
enum st_changer_action st_changer_string_to_action_v1(const char * action);
const char * st_changer_status_to_string_v1(enum st_changer_status status) __attribute__((nonnull));
enum st_changer_status st_changer_string_to_status_v1(const char * status) __attribute__((nonnull));
void st_changer_sync_v1(struct st_changer * changer, struct st_value * new_changer) __attribute__((nonnull));

#endif

