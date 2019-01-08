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
*  Copyright (C) 2013-2019, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// tee
#include <fcntl.h>
// poll
#include <poll.h>
// bool
#include <stdbool.h>
// free, mkstemp
#include <stdlib.h>
// dprintf
#include <stdio.h>
// memcpy, strcpy
#include <string.h>
// ioctl
#include <sys/ioctl.h>
// fstat
#include <sys/stat.h>
// fstat
#include <sys/types.h>
// close, fstat, pipe, read, tee
#include <unistd.h>

#include <libstoriqone/file.h>
#include <libstoriqone/io.h>
#include <libstoriqone/process.h>

#include "common.h"

struct so_database_postgresql_stream_backup_private {
	struct so_process pg_dump;
	char pgpass[20];
	int pg_out;
	ssize_t position;
};

static int so_database_postgresql_stream_backup_close(struct so_stream_reader * sr);
static bool so_database_postgresql_stream_backup_end_of_file(struct so_stream_reader * sr);
static off_t so_database_postgresql_stream_backup_forward(struct so_stream_reader * sr, off_t offset);
static void so_database_postgresql_stream_backup_free(struct so_stream_reader * sr);
static ssize_t so_database_postgresql_stream_backup_get_available_size(struct so_stream_reader * sr);
static ssize_t so_database_postgresql_stream_backup_get_block_size(struct so_stream_reader * sr);
static int so_database_postgresql_stream_backup_last_errno(struct so_stream_reader * sr);
static ssize_t so_database_postgresql_stream_backup_peek(struct so_stream_reader * sr, void * buffer, ssize_t length);
static ssize_t so_database_postgresql_stream_backup_position(struct so_stream_reader * sr);
static ssize_t so_database_postgresql_stream_backup_read(struct so_stream_reader * sr, void * buffer, ssize_t length);
static int so_database_postgresql_stream_rewind(struct so_stream_reader * sr);

static struct so_stream_reader_ops so_database_postgresql_stream_backup_ops = {
	.close              = so_database_postgresql_stream_backup_close,
	.end_of_file        = so_database_postgresql_stream_backup_end_of_file,
	.forward            = so_database_postgresql_stream_backup_forward,
	.free               = so_database_postgresql_stream_backup_free,
	.get_available_size = so_database_postgresql_stream_backup_get_available_size,
	.get_block_size     = so_database_postgresql_stream_backup_get_block_size,
	.last_errno         = so_database_postgresql_stream_backup_last_errno,
	.peek               = so_database_postgresql_stream_backup_peek,
	.position           = so_database_postgresql_stream_backup_position,
	.read               = so_database_postgresql_stream_backup_read,
	.rewind             = so_database_postgresql_stream_rewind,
};


struct so_stream_reader * so_database_postgresql_backup_init(struct so_database_postgresql_config_private * config) {
	if (config == NULL)
		return NULL;

	const char * port = "5432";
	if (config->port != NULL)
		port = config->port;

	struct so_database_postgresql_stream_backup_private * self = malloc(sizeof(struct so_database_postgresql_stream_backup_private));
	self->position = 0;

	strcpy(self->pgpass, "/tmp/.pgpass_XXXXXX");
	int fd = mkstemp(self->pgpass);

	dprintf(fd, "%s:%s:*:%s:%s", config->host, port, config->user, config->password);
	close(fd);

	const char * params[] = { "-Fc", "-Z9", "-C", "-h", config->host, "-p", port, "-U", config->user, "-w", config->db, NULL };
	so_process_new(&self->pg_dump, "pg_dump", params, 11);
	so_process_put_environment(&self->pg_dump, "PGPASSFILE", self->pgpass);
	self->pg_out = so_process_pipe_from(&self->pg_dump, so_process_stdout);
	so_process_start(&self->pg_dump, 1);

	struct so_stream_reader * reader = malloc(sizeof(struct so_stream_reader));
	reader->ops = &so_database_postgresql_stream_backup_ops;
	reader->data = self;

	return reader;
}

static int so_database_postgresql_stream_backup_close(struct so_stream_reader * sr) {
	struct so_database_postgresql_stream_backup_private * self = sr->data;

	if (self->pg_out < -1)
		return 0;

	close(self->pg_out);
	self->pg_out = -1;

	so_process_wait(&self->pg_dump, 1, true);
	so_file_rm(self->pgpass);

	return 0;
}

static bool so_database_postgresql_stream_backup_end_of_file(struct so_stream_reader * sr) {
	if (sr == NULL)
		return false;

	struct so_database_postgresql_stream_backup_private * self = sr->data;
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

static off_t so_database_postgresql_stream_backup_forward(struct so_stream_reader * sr __attribute__((unused)), off_t offset __attribute__((unused))) {
	return -1;
}

static void so_database_postgresql_stream_backup_free(struct so_stream_reader * sr) {
	struct so_database_postgresql_stream_backup_private * self = sr->data;

	if (self->pg_out >= 0)
		so_database_postgresql_stream_backup_close(sr);

	so_process_free(&self->pg_dump, 1);

	free(self);
	free(sr);
}

static ssize_t so_database_postgresql_stream_backup_get_available_size(struct so_stream_reader * sr) {
	struct so_database_postgresql_stream_backup_private * self = sr->data;

	ssize_t available_in_pipe = 0;
	int failed = ioctl(self->pg_out, FIONREAD, &available_in_pipe);
	return failed != 0 ? -1 : available_in_pipe;
}

static ssize_t so_database_postgresql_stream_backup_get_block_size(struct so_stream_reader * sr) {
	struct so_database_postgresql_stream_backup_private * self = sr->data;

	struct stat st;
	if (fstat(self->pg_out, &st))
		return -1;

	return st.st_blksize;
}

static int so_database_postgresql_stream_backup_last_errno(struct so_stream_reader * sr __attribute__((unused))) {
	return 0;
}

static ssize_t so_database_postgresql_stream_backup_peek(struct so_stream_reader * sr, void * buffer, ssize_t length) {
	if (sr == NULL || buffer == NULL)
		return -1;

	if (length < 1)
		return 0;

	int fds[2];
	int failed = pipe(fds);
	if (failed != 0)
		return -1;

	struct so_database_postgresql_stream_backup_private * self = sr->data;

	ssize_t nb_copied = tee(self->pg_out, fds[1], length, 0);
	if (nb_copied > 0)
		nb_copied = read(fds[0], buffer, nb_copied);

	close(fds[0]);
	close(fds[1]);

	return nb_copied;
}

static ssize_t so_database_postgresql_stream_backup_position(struct so_stream_reader * sr) {
	struct so_database_postgresql_stream_backup_private * self = sr->data;
	return self->position;
}

static ssize_t so_database_postgresql_stream_backup_read(struct so_stream_reader * sr, void * buffer, ssize_t length) {
	if (sr == NULL || buffer == NULL)
		return -1;

	if (length < 1)
		return 0;

	struct so_database_postgresql_stream_backup_private * self = sr->data;
	ssize_t nb_read = read(self->pg_out, buffer, length);
	if (nb_read > 0)
		self->position += nb_read;

	return nb_read;
}

static int so_database_postgresql_stream_rewind(struct so_stream_reader * sr __attribute__((unused))) {
	return -1;
}
