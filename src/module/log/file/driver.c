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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sat, 08 Dec 2012 15:23:48 +0100                         *
\*************************************************************************/

// realloc
#include <stdlib.h>

#include <libstone/log.h>
#include <libstone/util/hashtable.h>

#include "common.h"

static int st_log_file_add(const char * alias, enum st_log_level level, const struct st_hashtable * params);
static void st_log_file_init(void) __attribute__((constructor));

static struct st_log_driver st_log_file_driver = {
	.name = "file",

	.add = st_log_file_add,

	.cookie = NULL,
	.api_level = STONE_LOG_API_LEVEL,

	.modules = NULL,
	.nb_modules = 0,
};


static int st_log_file_add(const char * alias, enum st_log_level level, const struct st_hashtable * params) {
	if (alias == NULL || params == NULL)
		return 1;

	char * path = st_hashtable_get(params, "path").value.string;
	if (path == NULL)
		return 2;

	void * new_addr = realloc(st_log_file_driver.modules, (st_log_file_driver.nb_modules + 1) * sizeof(struct st_log_module));
	if (new_addr == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_plugin_log, "Error, there is not enough memory to allocate new file module");
		return 3;
	}

	st_log_file_driver.modules = new_addr;
	int failed = st_log_file_new(st_log_file_driver.modules + st_log_file_driver.nb_modules, alias, level, path);
	if (!failed)
		st_log_file_driver.nb_modules++;

	return failed + 3;
}

static void st_log_file_init(void) {
	st_log_register_driver(&st_log_file_driver);
}

