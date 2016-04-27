/****************************************************************************\
*                    ______           _      ____                            *
*                   / __/ /____  ____(_)__ _/ __ \___  ___                   *
*                  _\ \/ __/ _ \/ __/ / _ `/ /_/ / _ \/ -_)                  *
*                 /___/\__/\___/_/ /_/\_, /\____/_//_/\__/                   *
*                                      /_/                                   *
*  ------------------------------------------------------------------------  *
*  This file is a part of Storiq One                                         *
*                                                                            *
*  Storiq One is free software; you can redistribute it and/or modify        *
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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// open
#include <fcntl.h>
// gettext
#include <libintl.h>
// free, malloc
#include <malloc.h>
// dprintf
#include <stdio.h>
// memset, strdup, strlen, strncpy
#include <string.h>
// bzero
#include <strings.h>
// fstat, open, stat
#include <sys/stat.h>
// fstat, open, stat
#include <sys/types.h>
// close, fstat, sleep, stat
#include <unistd.h>

#include <libstoriqone/string.h>
#include <libstoriqone/time.h>
#include <libstoriqone/value.h>
#include <logger/log.h>

#include <checksum/logger.chcksum>

struct logger_log_file_private {
	char * path;
	int fd;

	struct stat current;

	char * buf_level;
	size_t buf_level_length;
	char * buf_type;
	size_t buf_type_length;

	size_t level_width;
	size_t type_width;
};

static void solgr_file_driver_init(void) __attribute__((constructor(1001)));
static struct solgr_log_module * solgr_file_driver_new_module(struct so_value * param);

static void solgr_file_module_check_logrotate(struct logger_log_file_private * self);
static void solgr_file_module_format_string(char * buffer, size_t length, const char * string, size_t width);
static void solgr_file_module_free(struct solgr_log_module * module);
static void solgr_file_module_write(struct solgr_log_module * module, struct so_value * message);

static struct solgr_log_driver solgr_file_driver = {
	.name         = "file",
	.new_module   = solgr_file_driver_new_module,
	.cookie       = NULL,
	.src_checksum = LOGGER_SRCSUM,
};

static struct solgr_log_module_ops solgr_file_module_ops = {
	.free  = solgr_file_module_free,
	.write = solgr_file_module_write,
};


static void solgr_file_driver_init() {
	solgr_log_register_driver(&solgr_file_driver);
}

static struct solgr_log_module * solgr_file_driver_new_module(struct so_value * param) {
	char * path = NULL, * level;
	if (so_value_unpack(param, "{ssss}", "path", &path, "verbosity", &level) < 2) {
		free(path);
		return NULL;
	}

	int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0640);
	if (fd < 0)
		return NULL;

	struct logger_log_file_private * self = malloc(sizeof(struct logger_log_file_private));
	self->path = path;
	self->fd = fd;
	fstat(self->fd, &self->current);

	self->level_width = so_log_level_max_length();
	self->type_width = so_log_type_max_length();

	self->buf_level_length = 4 * self->level_width;
	self->buf_level = malloc(self->buf_level_length + 1);
	bzero(self->buf_level, self->buf_level_length + 1);
	self->buf_type_length = 4 * self->type_width;
	self->buf_type = malloc(self->buf_type_length + 1);
	bzero(self->buf_type, self->buf_type_length + 1);

	struct solgr_log_module * mod = malloc(sizeof(struct solgr_log_module));
	mod->level = so_log_string_to_level(level, false);
	mod->ops = &solgr_file_module_ops;
	mod->driver = &solgr_file_driver;
	mod->data = self;

	free(level);

	return mod;
}


static void solgr_file_module_check_logrotate(struct logger_log_file_private * self) {
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

		solgr_log_write2(so_log_level_notice, so_log_type_plugin_log, gettext("Log: file: logrotate detected"));
	}
}

static void solgr_file_module_format_string(char * buffer, size_t length, const char * string, size_t width) {
	memset(buffer, ' ', length);

	size_t string_utf8_length = strlen(string);
	size_t string_length = so_string_utf8_length(string);

	strncpy(buffer, string, string_utf8_length);
	buffer[width + string_utf8_length - string_length] = '\0';
}

static void solgr_file_module_free(struct solgr_log_module * module) {
	struct logger_log_file_private * self = module->data;

	module->ops = NULL;
	module->data = NULL;

	free(self->path);
	self->path = NULL;
	if (self->fd >= 0)
		close(self->fd);
	self->fd = -1;

	free(self->buf_level);
	free(self->buf_type);

	free(self);
}

static void solgr_file_module_write(struct solgr_log_module * module, struct so_value * message) {
	struct logger_log_file_private * self = module->data;

	char * slevel = NULL, * stype = NULL, * smessage = NULL;
	int pid = -1;
	long long int iTimestamp;

	int ret = so_value_unpack(message, "{sssssIsiss}",
		"level", &slevel,
		"type", &stype,
		"timestamp", &iTimestamp,
		"pid", &pid,
		"message", &smessage
	);

	enum so_log_level level = so_log_level_unknown;
	if (ret == 5)
		level = so_log_string_to_level(slevel, false);

	enum so_log_type type = so_log_type_unknown;
	if (ret == 5)
		type = so_log_string_to_type(stype, false);

	if (ret < 5 || level == so_log_level_unknown || type == so_log_type_unknown) {
		free(slevel);
		free(stype);
		free(smessage);
		return;
	}

	solgr_file_module_check_logrotate(self);

	solgr_file_module_format_string(self->buf_level, self->buf_level_length, so_log_level_to_string(level, true), self->level_width);
	solgr_file_module_format_string(self->buf_type, self->buf_type_length, so_log_type_to_string(type, true), self->type_width);

	char strtime[36];
	time_t timestamp = iTimestamp;
	so_time_convert(&timestamp, "%c", strtime, 36);

	dprintf(self->fd, "[L:%s | T:%s | P:%5d | %s]: %s\n", self->buf_level, self->buf_type, pid, strtime, smessage);

	free(slevel);
	free(stype);
	free(smessage);
}

