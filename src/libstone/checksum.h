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

#ifndef __LIBSTONE_CHECKSUM_P_H__
#define __LIBSTONE_CHECKSUM_P_H__

#include <libstone/checksum.h>

#define st_checksum_v1 st_checksum
#define st_checksum_driver_v1 st_checksum_driver

char * st_checksum_compute_v1(const char * checksum, const void * data, ssize_t length) __attribute__((nonnull(1),warn_unused_result));
void st_checksum_convert_to_hex_v1(unsigned char * digest, ssize_t length, char * hex_digest) __attribute__((nonnull));
char * st_checksum_gen_salt_v1(const char * checksum, size_t length) __attribute__((nonnull,warn_unused_result));
struct st_checksum_driver_v1 * st_checksum_get_driver_v1(const char * driver) __attribute__((nonnull));
bool st_checksum_is_default_v1(const char * driver) __attribute__((nonnull));
void st_checksum_register_driver_v1(struct st_checksum_driver * driver) __attribute__((nonnull));
char * st_checksum_salt_password_v1(const char * checksum, const char * password, const char * salt) __attribute__((nonnull,warn_unused_result));

#endif
