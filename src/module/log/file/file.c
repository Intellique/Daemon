/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Thu, 30 Jun 2011 08:57:46 +0200                       *
\***********************************************************************/

// realloc
#include <malloc.h>

#include <storiqArchiver/util/hashtable.h>

#include "common.h"

static int _sa_log_file_add(struct sa_log_driver * driver, const char * alias, enum sa_log_level level, struct hashtable * params);


static struct sa_log_driver _sa_log_file_driver = {
	.name      = "file",
	.add       = _sa_log_file_add,
	.data      = 0,
	.cookie    = 0,
	.modules   = 0,
	.nbModules = 0,
};


int _sa_log_file_add(struct sa_log_driver * driver, const char * alias, enum sa_log_level level, struct hashtable * params) {
	if (!driver || !alias || !params)
		return 1;

	char * path = hashtable_value(params, "path");
	if (!path)
		return 1;

	driver->modules = realloc(driver->modules, (driver->nbModules + 1) * sizeof(struct sa_log_module));
	_sa_log_file_new(driver->modules + driver->nbModules, alias, level, path);
	driver->nbModules++;

	return 0;
}

__attribute__((constructor))
static void _sa_log_file_init() {
	sa_log_register_driver(&_sa_log_file_driver);
}

