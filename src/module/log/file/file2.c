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
*  Last modified: Wed, 29 Sep 2010 11:17:36 +0200                       *
\***********************************************************************/

// open
#include <fcntl.h>
// malloc
#include <malloc.h>
// dprintf
#include <stdio.h>
// strdup
#include <string.h>
// open
#include <sys/types.h>
// open
#include <sys/stat.h>
// close
#include <unistd.h>

#include "common.h"

struct log_file_private {
	char * path;
	int fd;
};

static void log_file_subFree(struct log_moduleSub * subModule);
static void log_file_subWrite(struct log_moduleSub * subModule, enum Log_level level, const char * message);

static struct log_moduleSub_ops log_file_moduleSubOps = {
	.free = 	log_file_subFree,
	.write = 	log_file_subWrite,
};


void log_file_subFree(struct log_moduleSub * subModule) {
	struct log_file_private * self = subModule->data;

	if (subModule->alias)
		free(subModule->alias);
	subModule->alias = 0;
	subModule->ops = 0;
	subModule->data = 0;

	if (self->path)
		free(self->path);
	self->path = 0;
	if (self->fd >= 0)
		close(self->fd);
	self->fd = -1;

	free(self);
}

struct log_moduleSub * log_file_new(struct log_moduleSub * subModule, const char * alias, enum Log_level level, const char * path) {
	if (!subModule)
		subModule = malloc(sizeof(struct log_moduleSub));

	subModule->alias = strdup(alias);
	subModule->level = level;
	subModule->ops = &log_file_moduleSubOps;

	struct log_file_private * self = malloc(sizeof(struct log_file_private));
	self->path = strdup(path);
	self->fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0640);

	subModule->data = self;
	return subModule;
}

void log_file_subWrite(struct log_moduleSub * subModule, enum Log_level level, const char * message) {
	struct log_file_private * self = subModule->data;
	dprintf(self->fd, "[%s] %s\n", log_levelToString(level), message);
}

