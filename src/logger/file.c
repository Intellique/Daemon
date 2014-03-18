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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
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

#include <libstone/value.h>
#include <libstone-logger/log.h>

#include <checksum/libstone-logger.chcksum>

struct logger_log_file_private {
	char * path;
	int fd;

	struct stat current;
};

static void file_driver_init(void) __attribute__((constructor));
static struct lgr_log_module * file_driver_new_module(struct st_value * param);

static void file_module_check_logrotate(struct logger_log_file_private * self);
static void file_module_free(struct lgr_log_module * module);
static void file_module_write(struct lgr_log_module * module, struct st_value * message);

static struct lgr_log_driver file_driver = {
	.name         = "file",
	.new_module   = file_driver_new_module,
	.cookie       = NULL,
	.api_level    = 0,
	.src_checksum = LIBSTONELOGGER_SRCSUM,
};

static struct lgr_log_module_ops file_module_ops = {
	.free  = file_module_free,
	.write = file_module_write,
};


static void file_driver_init() {
	lgr_log_register_driver(&file_driver);
}

static struct lgr_log_module * file_driver_new_module(struct st_value * param) {
	if (!st_value_hashtable_has_key2(param, "path"))
		return NULL;

	struct st_value * path = st_value_hashtable_get2(param, "path", false);
	if (path == NULL || path->type != st_value_string)
		return NULL;

	struct st_value * level = st_value_hashtable_get2(param, "verbosity", false);
	if (level == NULL || level->type != st_value_string)
		return NULL;

	int fd = open(path->value.string, O_WRONLY | O_APPEND | O_CREAT, 0640);
	if (fd < 0)
		return NULL;

	struct logger_log_file_private * self = malloc(sizeof(struct logger_log_file_private));
	self->path = strdup(path->value.string);
	self->fd = fd;
	fstat(self->fd, &self->current);

	struct lgr_log_module * mod = malloc(sizeof(struct lgr_log_module));
	mod->level = st_log_string_to_level(level->value.string);
	mod->ops = &file_module_ops;
	mod->driver = &file_driver;
	mod->data = self;

	return mod;
}


static void file_module_check_logrotate(struct logger_log_file_private * self) {
	struct stat reference;
	int failed = 0;
	do {
		if (failed)
			sleep(1);

		failed = stat(self->path, &reference);
	} while (failed);

	if (self->current.st_ino != reference.st_ino) {
		close(self->fd);

		self->fd = open(self->path, O_WRONLY | O_APPEND | O_CREAT, 0640);
		fstat(self->fd, &self->current);

		lgr_log_write2(st_log_level_notice, st_log_type_plugin_log, "Log: file: logrotate detected");
	}
}

static void file_module_free(struct lgr_log_module * module) {
	struct logger_log_file_private * self = module->data;

	module->ops = NULL;
	module->data = NULL;

	free(self->path);
	self->path = NULL;
	if (self->fd >= 0)
		close(self->fd);
	self->fd = -1;

	free(self);
}

static void file_module_write(struct lgr_log_module * module, struct st_value * message) {
	struct logger_log_file_private * self = module->data;

	if (!st_value_hashtable_has_key2(message, "type") || !st_value_hashtable_has_key2(message, "message"))
		return;

	file_module_check_logrotate(self);

	struct st_value * level = st_value_hashtable_get2(message, "level", false);
	if (level->type != st_value_string)
		return;

	struct st_value * vtype = st_value_hashtable_get2(message, "type", false);
	if (vtype->type != st_value_string)
		return;

	enum st_log_type type = st_log_string_to_type(vtype->value.string);
	if (type == st_log_type_unknown)
		return;

	struct st_value * vtimestamp = st_value_hashtable_get2(message, "timestamp", false);
	if (vtimestamp->type != st_value_integer)
		return;

	struct st_value * vmessage = st_value_hashtable_get2(message, "message", false);
	if (vmessage->type != st_value_string)
		return;

	struct tm curTime2;
	char strtime[32];

	time_t timestamp = vtimestamp->value.integer;
	localtime_r(&timestamp, &curTime2);
	strftime(strtime, 32, "%F %T", &curTime2);

	dprintf(self->fd, "[L:%-7s | T:%-15s | @%s]: %s\n", level->value.string, vtype->value.string, strtime, vmessage->value.string);
}

