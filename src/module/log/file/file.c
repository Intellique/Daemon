/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Tue, 22 Nov 2011 18:02:27 +0100                         *
\*************************************************************************/

// realloc
#include <malloc.h>

#include <storiqArchiver/util/hashtable.h>

#include "common.h"

static int sa_log_file_add(struct sa_log_driver * driver, const char * alias, enum sa_log_level level, struct sa_hashtable * params);
static void sa_log_file_init(void) __attribute__((constructor));


static struct sa_log_driver sa_log_file_driver = {
	.name         = "file",
	.add          = sa_log_file_add,
	.data         = 0,
	.cookie       = 0,
	.api_version  = STORIQARCHIVER_LOG_APIVERSION,
	.modules      = 0,
	.nb_modules   = 0,
};


int sa_log_file_add(struct sa_log_driver * driver, const char * alias, enum sa_log_level level, struct sa_hashtable * params) {
	if (!driver || !alias || !params)
		return 1;

	char * path = sa_hashtable_value(params, "path");
	if (!path)
		return 1;

	driver->modules = realloc(driver->modules, (driver->nb_modules + 1) * sizeof(struct sa_log_module));
	sa_log_file_new(driver->modules + driver->nb_modules, alias, level, path);
	driver->nb_modules++;

	return 0;
}

static void sa_log_file_init() {
	sa_log_register_driver(&sa_log_file_driver);
}

