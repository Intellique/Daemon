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
*  Last modified: Fri, 15 Nov 2013 16:05:07 +0100                            *
\****************************************************************************/

#ifndef __STONE_SCRIPT_H__
#define __STONE_SCRIPT_H__

struct st_database_connection;
struct json_t;
struct st_pool;

enum st_script_type {
	st_script_type_on_error,
	st_script_type_post,
	st_script_type_pre,

	st_script_type_unknown,
};

struct json_t * st_script_run(struct st_database_connection * connect, const char * job_type, enum st_script_type type, struct st_pool * pool, struct json_t * data);
enum st_script_type st_script_string_to_type(const char * string);
void st_script_sync(struct st_database_connection * connection);
const char * st_script_type_to_string(enum st_script_type type);

#endif

