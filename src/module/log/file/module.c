/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
*  it under the terms of the GNU Affero General Public License               *
*  as published by the Free Software Foundation; either version 3            *
*  of the License, or (at your option) any later version.                    *
*                                                                            *
*  This program is distributed in the hope that it will be useful,           *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
*  GNU Affero General Public License for more details.                       *
*                                                                            *
*  You should have received a copy of the GNU Affero General Public License  *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  Copyright (C) 2013-2015, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Fri, 17 Aug 2012 00:06:07 +0200                            *
\****************************************************************************/

// open
#include <fcntl.h>
// free, malloc
#include <malloc.h>
// dprintf
#include <stdio.h>
// strdup
#include <string.h>
// fstat, open, stat
#include <sys/stat.h>
// gettimeofday
#include <sys/time.h>
// fstat, open, stat
#include <sys/types.h>
// localtime_r, strftime
#include <time.h>
// close, fstat, sleep, stat
#include <unistd.h>

#include <libstone/user.h>

#include "common.h"

struct st_log_file_private {
	char * path;
	int fd;

	struct stat current;
};

static void st_log_file_check_logrotate(struct st_log_file_private * self);
static void st_log_file_module_free(struct st_log_module * module);
static void st_log_file_module_write(struct st_log_module * module, const struct st_log_message * message);

static struct st_log_module_ops st_log_file_module_ops = {
	.free  = st_log_file_module_free,
	.write = st_log_file_module_write,
};


static void st_log_file_check_logrotate(struct st_log_file_private * self) {
	struct stat reference;
	int failed = 0;
	do {
		if (failed)
			sleep(1);

		failed = stat(self->path, &reference);
	} while (failed);

	if (self->current.st_ino != reference.st_ino) {
		st_log_write_all(st_log_level_info, st_log_type_plugin_log, "Log: file: logrotate detected");

		close(self->fd);

		self->fd = open(self->path, O_WRONLY | O_APPEND | O_CREAT, 0640);
		fstat(self->fd, &self->current);
	}
}

static void st_log_file_module_free(struct st_log_module * module) {
	struct st_log_file_private * self = module->data;

	free(module->alias);
	module->alias = NULL;
	module->ops = NULL;
	module->data = NULL;

	free(self->path);
	self->path = NULL;
	if (self->fd >= 0)
		close(self->fd);
	self->fd = -1;

	free(self);
}

int st_log_file_new(struct st_log_module * module, const char * alias, enum st_log_level level, const char * path) {
	if (module == NULL || alias == NULL || path == NULL)
		return 1;

	int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0640);
	if (fd < 0)
		return 2;

	struct st_log_file_private * self = malloc(sizeof(struct st_log_file_private));
	self->path = strdup(path);
	self->fd = fd;

	fstat(fd, &self->current);

	module->alias = strdup(alias);
	module->level = level;
	module->ops = &st_log_file_module_ops;
	module->data = self;

	return 0;
}

static void st_log_file_module_write(struct st_log_module * module, const struct st_log_message * message) {
	struct st_log_file_private * self = module->data;

	st_log_file_check_logrotate(self);

	struct tm curTime2;
	char strtime[32];

	localtime_r(&message->timestamp, &curTime2);
	strftime(strtime, 32, "%F %T", &curTime2);

	if (message->user)
		dprintf(self->fd, "[L:%-7s | T:%-15s | U:%s | @%s]: %s\n", st_log_level_to_string(message->level), st_log_type_to_string(message->type), message->user->login, strtime, message->message);
	else
		dprintf(self->fd, "[L:%-7s | T:%-15s | @%s]: %s\n", st_log_level_to_string(message->level), st_log_type_to_string(message->type), strtime, message->message);
}

