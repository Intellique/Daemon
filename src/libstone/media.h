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

#ifndef __LIBSTONE_MEDIA_P_H__
#define __LIBSTONE_MEDIA_P_H__

#include <libstone/media.h>

const char * st_media_format_data_to_string_v1(enum st_media_format_data_type type);
enum st_media_format_data_type st_media_string_to_format_data_v1(const char * type) __attribute__((nonnull));

const char * st_media_format_mode_to_string_v1(enum st_media_format_mode mode);
enum st_media_format_mode st_media_string_to_format_mode_v1(const char * mode) __attribute__((nonnull));

const char * st_media_location_to_string_v1(enum st_media_location location);
enum st_media_location st_media_string_to_location_v1(const char * location) __attribute__((nonnull));

const char * st_media_status_to_string_v1(enum st_media_status status);
enum st_media_status st_media_string_to_status_v1(const char * status) __attribute__((nonnull));

enum st_media_type st_media_string_to_type_v1(const char * type) __attribute__((nonnull));
const char * st_media_type_to_string_v1(enum st_media_type type);

const char * st_pool_autocheck_mode_to_string_v1(enum st_pool_autocheck_mode mode);
enum st_pool_autocheck_mode st_pool_string_to_autocheck_mode_v1(const char * mode) __attribute__((nonnull));

enum st_pool_unbreakable_level st_pool_string_to_unbreakable_level_v1(const char * level) __attribute__((nonnull));
const char * st_pool_unbreakable_level_to_string_v1(enum st_pool_unbreakable_level level);

#endif

