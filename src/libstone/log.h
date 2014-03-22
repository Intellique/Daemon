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

#ifndef __LIBSTONE_LOG_P_H__
#define __LIBSTONE_LOG_P_H__

#include <libstone/log.h>

#include "value_v1.h"

#define st_log_level_v1 st_log_level
#define st_log_type_v1 st_log_type

void st_log_configure_v1(struct st_value_v1 * config, enum st_log_type_v1 default_type);
const char * st_log_level_to_string_v1(enum st_log_level_v1 level);
enum st_log_level_v1 st_log_string_to_level_v1(const char * string);
enum st_log_type_v1 st_log_string_to_type_v1(const char * string);
const char * st_log_type_to_string_v1(enum st_log_type type_v1);
void st_log_write_v1(enum st_log_level level, const char * format, ...) __attribute__ ((format (printf, 2, 3)));
void st_log_write2_v1(enum st_log_level_v1 level, enum st_log_type_v1 type, const char * format, ...) __attribute__ ((format (printf, 3, 4)));

#endif

