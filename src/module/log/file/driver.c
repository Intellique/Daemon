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
*  Last modified: Mon, 23 Jul 2012 09:36:16 +0200                         *
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

	.cookie = 0,
	.api_level = STONE_LOG_API_LEVEL,

	.modules = 0,
	.nb_modules = 0,
};


int st_log_file_add(const char * alias, enum st_log_level level, const struct st_hashtable * params) {
	if (!alias || !params)
		return 1;

	char * path = st_hashtable_value(params, "path");
	if (!path)
		return 2;

	void * new_addr = realloc(st_log_file_driver.modules, (st_log_file_driver.nb_modules + 1) * sizeof(struct st_log_module));
	if (new_addr) {
		st_log_write_all(st_log_level_error, st_log_type_plugin_log, "Error, there is not enough memory to allocate new file module");
		return 3;
	}

	st_log_file_driver.modules = new_addr;
	int failed = st_log_file_new(st_log_file_driver.modules + st_log_file_driver.nb_modules, alias, level, path);
	if (!failed)
		st_log_file_driver.nb_modules++;

	return failed + 3;
}

void st_log_file_init() {
	st_log_register_driver(&st_log_file_driver);
}

