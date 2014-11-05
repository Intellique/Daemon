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

struct st_value * st_media_convert_v1(struct st_media * media) __attribute__((nonnull,warn_unused_result));
struct st_value * st_media_format_convert_v1(struct st_media_format * format) __attribute__((nonnull,warn_unused_result));
void st_media_format_sync_v1(struct st_media_format * format, struct st_value * new_format) __attribute__((nonnull));
void st_media_sync_v1(struct st_media * media, struct st_value * new_media) __attribute__((nonnull));
struct st_value * st_pool_convert_v1(struct st_pool * pool) __attribute__((nonnull,warn_unused_result));
void st_pool_sync_v1(struct st_pool * pool, struct st_value * new_pool) __attribute__((nonnull));

int st_media_format_cmp_v1(struct st_media_format * f1, struct st_media_format * f2) __attribute__((nonnull));

void st_media_free_v1(struct st_media * media) __attribute__((nonnull));
void st_media_format_free_v1(struct st_media_format * format) __attribute__((nonnull));
void st_pool_free_v1(struct st_pool * pool) __attribute__((nonnull));

const char * st_media_format_data_type_to_string_v1(enum st_media_format_data_type type, bool translate);
enum st_media_format_data_type st_media_string_to_format_data_type_v1(const char * type) __attribute__((nonnull));

const char * st_media_format_mode_to_string_v1(enum st_media_format_mode mode, bool translate);
enum st_media_format_mode st_media_string_to_format_mode_v1(const char * mode) __attribute__((nonnull));

const char * st_media_status_to_string_v1(enum st_media_status status, bool translate);
enum st_media_status st_media_string_to_status_v1(const char * status) __attribute__((nonnull));

enum st_media_type st_media_string_to_type_v1(const char * type) __attribute__((nonnull));
const char * st_media_type_to_string_v1(enum st_media_type type, bool translate);

const char * st_pool_autocheck_mode_to_string_v1(enum st_pool_autocheck_mode mode, bool translate);
enum st_pool_autocheck_mode st_pool_string_to_autocheck_mode_v1(const char * mode) __attribute__((nonnull));

enum st_pool_unbreakable_level st_pool_string_to_unbreakable_level_v1(const char * level) __attribute__((nonnull));
const char * st_pool_unbreakable_level_to_string_v1(enum st_pool_unbreakable_level level, bool translate);

#endif

