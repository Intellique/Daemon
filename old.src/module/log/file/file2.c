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
*  Last modified: Tue, 03 Apr 2012 15:40:47 +0200                         *
\*************************************************************************/

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

#include <stone/user.h>

#include "common.h"

struct st_log_file_private {
	char * path;
	int fd;
};

static void st_log_file_module_free(struct st_log_module * module);
static void st_log_file_module_write(struct st_log_module * module, struct st_log_message * message);

static struct st_log_module_ops st_log_file_module_ops = {
	.free  = st_log_file_module_free,
	.write = st_log_file_module_write,
};


void st_log_file_module_free(struct st_log_module * module) {
	struct st_log_file_private * self = module->data;

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

struct st_log_module * st_log_file_new(struct st_log_module * module, const char * alias, enum st_log_level level, const char * path) {
	if (!module)
		module = malloc(sizeof(struct st_log_module));

	module->alias = strdup(alias);
	module->level = level;
	module->ops = &st_log_file_module_ops;

	struct st_log_file_private * self = malloc(sizeof(struct st_log_file_private));
	self->path = strdup(path);
	self->fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0640);

	module->data = self;
	return module;
}

void st_log_file_module_write(struct st_log_module * module, struct st_log_message * message) {
	struct st_log_file_private * self = module->data;

	struct tm curTime2;
	char strtime[32];

	localtime_r(&message->timestamp, &curTime2);
	strftime(strtime, 32, "%F %T", &curTime2);

	if (message->user)
		dprintf(self->fd, "[L:%-7s | T:%-15s | U:%s | @%s]: %s\n", st_log_level_to_string(message->level), st_log_type_to_string(message->type), message->user->login, strtime, message->message);
	else
		dprintf(self->fd, "[L:%-7s | T:%-15s | @%s]: %s\n", st_log_level_to_string(message->level), st_log_type_to_string(message->type), strtime, message->message);
}

