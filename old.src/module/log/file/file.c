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
*  Last modified: Sat, 17 Dec 2011 19:29:13 +0100                         *
\*************************************************************************/

// realloc
#include <malloc.h>

#include <stone/util/hashtable.h>

#include "common.h"

static int st_log_file_add(struct st_log_driver * driver, const char * alias, enum st_log_level level, struct st_hashtable * params);
static void st_log_file_init(void) __attribute__((constructor));


static struct st_log_driver st_log_file_driver = {
	.name         = "file",
	.add          = st_log_file_add,
	.data         = 0,
	.cookie       = 0,
	.api_version  = STONE_LOG_APIVERSION,
	.modules      = 0,
	.nb_modules   = 0,
};


int st_log_file_add(struct st_log_driver * driver, const char * alias, enum st_log_level level, struct st_hashtable * params) {
	if (!driver || !alias || !params)
		return 1;

	char * path = st_hashtable_value(params, "path");
	if (!path)
		return 1;

	driver->modules = realloc(driver->modules, (driver->nb_modules + 1) * sizeof(struct st_log_module));
	st_log_file_new(driver->modules + driver->nb_modules, alias, level, path);
	driver->nb_modules++;

	return 0;
}

static void st_log_file_init() {
	st_log_register_driver(&st_log_file_driver);
}

