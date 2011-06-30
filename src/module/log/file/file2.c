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
*  Last modified: Thu, 30 Jun 2011 08:51:23 +0200                       *
\***********************************************************************/

// open
#include <fcntl.h>
// free, malloc
#include <malloc.h>
// dprintf
#include <stdio.h>
// strdup
#include <string.h>
// open
#include <sys/stat.h>
// gettimeofday
#include <sys/time.h>
// open
#include <sys/types.h>
// localtime_r, strftime
#include <time.h>
// close
#include <unistd.h>

#include "common.h"

struct _sa_log_file_private {
	char * path;
	int fd;
};

static void _sa_log_file_module_free(struct sa_log_module * module);
static void _sa_log_file_module_write(struct sa_log_module * module, enum sa_log_level level, const char * message);

static struct sa_log_module_ops _sa_log_file_module_ops = {
	.free  = _sa_log_file_module_free,
	.write = _sa_log_file_module_write,
};


void _sa_log_file_module_free(struct sa_log_module * module) {
	struct _sa_log_file_private * self = module->data;

	if (module->alias)
		free(module->alias);
	module->alias = 0;
	module->ops = 0;
	module->data = 0;

	if (self->path)
		free(self->path);
	self->path = 0;
	if (self->fd >= 0)
		close(self->fd);
	self->fd = -1;

	free(self);
}

struct sa_log_module * _sa_log_file_new(struct sa_log_module * module, const char * alias, enum sa_log_level level, const char * path) {
	if (!module)
		module = malloc(sizeof(struct sa_log_module));

	module->alias = strdup(alias);
	module->level = level;
	module->ops = &_sa_log_file_module_ops;

	struct _sa_log_file_private * self = malloc(sizeof(struct _sa_log_file_private));
	self->path = strdup(path);
	self->fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0640);

	module->data = self;
	return module;
}

void _sa_log_file_module_write(struct sa_log_module * module, enum sa_log_level level, const char * message) {
	struct _sa_log_file_private * self = module->data;

	struct timeval curTime;
	struct tm curTime2;
	char buffer[32];

	gettimeofday(&curTime, 0);
	localtime_r(&(curTime.tv_sec), &curTime2);
	strftime(buffer, 32, "%F %T", &curTime2);

	dprintf(self->fd, "@%s [%s] %s\n", buffer, sa_log_level_to_string(level), message);
}

