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
*  Last modified: Mon, 27 Sep 2010 18:15:39 +0200                       *
\***********************************************************************/

#include "storiqArchiver/log.h"

static void log_file_free(struct log_module * module);
static void log_file_writeAll(struct log_module * module, enum Log_level level, const char * message);


static struct log_module_ops log_file_moduleOps = {
	free:		log_file_free,
	writeAll:	log_file_writeAll,
};

static struct log_module log_file_module = {
	moduleName:		"file",
	ops:			&log_file_moduleOps,
	data:			0,
	cookie:			0,
	subModules:		0,
	nbSubModules:	0,
};


void log_file_free(struct log_module * module) {}

__attribute__((constructor))
static void log_file_init() {
	log_registerModule(&log_file_module);
}

void log_file_writeAll(struct log_module * module, enum Log_level level, const char * message) {}

