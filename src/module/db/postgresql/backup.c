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
*  Last modified: Wed, 27 Mar 2013 10:52:57 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// poll
#include <poll.h>
// bool
#include <stdbool.h>
// free, mkstemp
#include <stdlib.h>
// asprintf, dprintf
#include <stdio.h>
// memcpy, strcpy
#include <string.h>
// fstat
#include <sys/stat.h>
// fstat
#include <sys/types.h>
// close, fstat, read
#include <unistd.h>

#include <libstone/io.h>
#include <libstone/util/command.h>
#include <libstone/util/file.h>

#include "common.h"

struct st_db_postgresql_stream_backup_private {
	struct st_util_command pg_dump;
	char pgpass[20];
	int pg_out;
	ssize_t position;
};

static int st_db_postgresql_stream_backup_close(struct st_stream_reader * io);
static bool st_db_postgresql_stream_backup_end_of_file(struct st_stream_reader * io);
static off_t st_db_postgresql_stream_backup_forward(struct st_stream_reader * io, off_t offset);
static void st_db_postgresql_stream_backup_free(struct st_stream_reader * io);
static ssize_t st_db_postgresql_stream_backup_get_block_size(struct st_stream_reader * io);
static int st_db_postgresql_stream_backup_last_errno(struct st_stream_reader * io);
static ssize_t st_db_postgresql_stream_backup_position(struct st_stream_reader * io);
static ssize_t st_db_postgresql_stream_backup_read(struct st_stream_reader * io, void * buffer, ssize_t length);

static struct st_stream_reader_ops st_db_postgresql_stream_backup_ops = {
	.close          = st_db_postgresql_stream_backup_close,
	.end_of_file    = st_db_postgresql_stream_backup_end_of_file,
	.forward        = st_db_postgresql_stream_backup_forward,
	.free           = st_db_postgresql_stream_backup_free,
	.get_block_size = st_db_postgresql_stream_backup_get_block_size,
	.last_errno     = st_db_postgresql_stream_backup_last_errno,
	.position       = st_db_postgresql_stream_backup_position,
	.read           = st_db_postgresql_stream_backup_read,
};


struct st_stream_reader * st_db_postgresql_backup_init(struct st_db_postgresql_config_private * config) {
	if (config == NULL)
		return NULL;

	const char * port = "5432";
	if (config->port != NULL)
		port = config->port;

	struct st_db_postgresql_stream_backup_private * self = malloc(sizeof(struct st_db_postgresql_stream_backup_private));
	self->position = 0;

	strcpy(self->pgpass, "/tmp/.pgpass_XXXXXX");
	int fd = mkstemp(self->pgpass);

	dprintf(fd, "%s:%s:*:%s:%s", config->host, port, config->user, config->password);
	close(fd);

	const char * params[] = { "-Fc", "-Z9", "-C", "-h", config->host, "-p", port, "-U", config->user, "-w", config->db, NULL };
	st_util_command_new(&self->pg_dump, "pg_dump", params, 11);
	st_util_command_put_environment(&self->pg_dump, "PGPASSFILE", self->pgpass);
	self->pg_out = st_util_command_pipe_from(&self->pg_dump, st_util_command_stdout);
	st_util_command_start(&self->pg_dump, 1);

	struct st_stream_reader * reader = malloc(sizeof(struct st_stream_reader));
	reader->ops = &st_db_postgresql_stream_backup_ops;
	reader->data = self;

	return reader;
}

static int st_db_postgresql_stream_backup_close(struct st_stream_reader * io) {
	struct st_db_postgresql_stream_backup_private * self = io->data;

	if (self->pg_out < -1)
		return 0;

	close(self->pg_out);
	self->pg_out = -1;

	st_util_command_wait(&self->pg_dump, 1);
	st_util_file_rm(self->pgpass);

	return 0;
}

static bool st_db_postgresql_stream_backup_end_of_file(struct st_stream_reader * io) {
	if (io == NULL)
		return false;

	struct st_db_postgresql_stream_backup_private * self = io->data;
	if (self->pg_out < 0)
		return true;

	struct pollfd pfd = {
		.fd = self->pg_out,
		.events = POLLIN | POLLHUP,
	};

	int failed = poll(&pfd, 1, -1);
	if (!failed)
		return (pfd.revents & ~POLLIN) && (pfd.revents & POLLOUT);

	return false;
}

static off_t st_db_postgresql_stream_backup_forward(struct st_stream_reader * io __attribute__((unused)), off_t offset __attribute__((unused))) {
	return -1;
}

static void st_db_postgresql_stream_backup_free(struct st_stream_reader * io) {
	struct st_db_postgresql_stream_backup_private * self = io->data;

	if (self->pg_out >= 0)
		st_db_postgresql_stream_backup_close(io);

	st_util_command_free(&self->pg_dump, 1);

	free(self);
	free(io);
}

static ssize_t st_db_postgresql_stream_backup_get_block_size(struct st_stream_reader * io) {
	struct st_db_postgresql_stream_backup_private * self = io->data;

	struct stat st;
	if (fstat(self->pg_out, &st))
		return -1;

	return st.st_blksize;
}

static int st_db_postgresql_stream_backup_last_errno(struct st_stream_reader * io __attribute__((unused))) {
	return 0;
}

static ssize_t st_db_postgresql_stream_backup_position(struct st_stream_reader * io) {
	struct st_db_postgresql_stream_backup_private * self = io->data;
	return self->position;
}

static ssize_t st_db_postgresql_stream_backup_read(struct st_stream_reader * io, void * buffer, ssize_t length) {
	if (io == NULL || buffer == NULL)
		return -1;

	if (length < 1)
		return 0;

	struct st_db_postgresql_stream_backup_private * self = io->data;
	ssize_t nb_read = read(self->pg_out, buffer, length);
	if (nb_read > 0)
		self->position += nb_read;

	return nb_read;
}

