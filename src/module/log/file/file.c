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
*  Copyright (C) 2010, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Tue, 28 Sep 2010 10:23:47 +0200                       *
\***********************************************************************/

// realloc
#include <malloc.h>

#include "common.h"
#include "storiqArchiver/hashtable.h"

static int log_file_add(struct log_module * module, const char * alias, enum Log_level level, struct hashtable * params);


static struct log_module_ops log_file_moduleOps = {
	add: log_file_add,
};

static struct log_module log_file_module = {
	moduleName:		"file",
	ops:			&log_file_moduleOps,
	data:			0,
	cookie:			0,
	subModules:		0,
	nbSubModules:	0,
};


int log_file_add(struct log_module * module, const char * alias, enum Log_level level, struct hashtable * params) {
	if (!module || !alias || !params)
		return 1;

	if (!hashtable_hasKey(params, "value"))
		return 1;

	module->subModules = realloc(module->subModules, (module->nbSubModules + 1) * sizeof(struct log_moduleSub));
	log_file_new(module->subModules + module->nbSubModules, alias, level, hashtable_value(params, "value"));
	module->nbSubModules++;

	return 0;
}

__attribute__((constructor))
static void log_file_init() {
	log_registerModule(&log_file_module);
}

