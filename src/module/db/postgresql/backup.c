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
*  Last modified: Thu, 24 May 2012 16:22:01 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// PQfreemem, PQgetCopyData
#include <postgresql/libpq-fe.h>
// free
#include <stdlib.h>
// asprintf
#include <stdio.h>
// memcpy
#include <string.h>

#include <stone/io.h>

#include "common.h"

struct st_db_postgresql_stream_backup_private {
	PGconn * con;

	PGresult * c_result;
	const char ** c_table;
	char * c_line;

	ssize_t position;
};

static const char * tables[] = {
	"tapeformat", "pool", "tape", "driveformat", "driveformatsupport", "host", "changer", "drive", "changerslot", 
	"selectedfile", "users", "userlog", "archive", "archivefile", "archivevolume", "archivefiletoarchivevolume", 
	"checksum", "checksumresult", "archivefiletochecksumresult", "archivevolumetochecksumresult", "jobtype", 
	"job", "jobtochecksum", "jobrecord", "jobtoselectedfile", "log", "restoreto", 

	0,
};

static int st_db_postgresql_stream_backup_close(struct st_stream_reader * io);
static int st_db_postgresql_stream_backup_end_of_file(struct st_stream_reader * io);
static off_t st_db_postgresql_stream_backup_forward(struct st_stream_reader * io, off_t offset);
static void st_db_postgresql_stream_backup_free(struct st_stream_reader * io);
static ssize_t st_db_postgresql_stream_backup_get_block_size(struct st_stream_reader * io);
static int st_db_postgresql_stream_backup_last_errno(struct st_stream_reader * io);
static ssize_t st_db_postgresql_stream_backup_position(struct st_stream_reader * io);
static ssize_t st_db_postgresql_stream_backup_read(struct st_stream_reader * io, void * buffer, ssize_t length);
static off_t st_db_postgresql_stream_backup_set_position(struct st_stream_reader * io, off_t position);

static struct st_stream_reader_ops st_db_postgresql_stream_backup_ops = {
	.close          = st_db_postgresql_stream_backup_close,
	.end_of_file    = st_db_postgresql_stream_backup_end_of_file,
	.forward        = st_db_postgresql_stream_backup_forward,
	.free           = st_db_postgresql_stream_backup_free,
	.get_block_size = st_db_postgresql_stream_backup_get_block_size,
	.last_errno     = st_db_postgresql_stream_backup_last_errno,
	.position       = st_db_postgresql_stream_backup_position,
	.read           = st_db_postgresql_stream_backup_read,
	.set_position   = st_db_postgresql_stream_backup_set_position,
};


struct st_stream_reader * st_db_postgresql_init_backup(struct st_db_postgresql_private * driver_private) {
	struct st_db_postgresql_stream_backup_private * self = malloc(sizeof(struct st_db_postgresql_stream_backup_private));
	self->con = PQsetdbLogin(driver_private->host, driver_private->port, 0, 0, driver_private->db, driver_private->user, driver_private->password);
	self->c_result = 0;
	self->c_table = tables;
	self->c_line = 0;
	self->position = 0;

	char * query;
	asprintf(&query, "COPY %s TO STDOUT WITH CSV HEADER", *self->c_table);
	self->c_result = PQexec(self->con, query);
	free(query);

	struct st_stream_reader * reader = malloc(sizeof(struct st_stream_reader));
	reader->ops = &st_db_postgresql_stream_backup_ops;
	reader->data = self;

	return reader;
}

int st_db_postgresql_stream_backup_close(struct st_stream_reader * io) {
	struct st_db_postgresql_stream_backup_private * self = io->data;

	if (self->con)
		PQfinish(self->con);
	self->con = 0;

	self->c_table = 0;
	if (self->c_line)
		PQfreemem(self->c_line);
	self->c_line = 0;

	return 0;
}

int st_db_postgresql_stream_backup_end_of_file(struct st_stream_reader * io) {
	struct st_db_postgresql_stream_backup_private * self = io->data;
	return self->c_table != 0;
}

off_t st_db_postgresql_stream_backup_forward(struct st_stream_reader * io __attribute__((unused)), off_t offset __attribute__((unused))) {
	return -1;
}

void st_db_postgresql_stream_backup_free(struct st_stream_reader * io) {
	struct st_db_postgresql_stream_backup_private * self = io->data;

	if (self->con)
		PQfinish(self->con);
	self->con = 0;

	free(self);
	free(io);
}

ssize_t st_db_postgresql_stream_backup_get_block_size(struct st_stream_reader * io __attribute__((unused))) {
	return 4096;
}

int st_db_postgresql_stream_backup_last_errno(struct st_stream_reader * io __attribute__((unused))) {
	return 0;
}

ssize_t st_db_postgresql_stream_backup_position(struct st_stream_reader * io) {
	struct st_db_postgresql_stream_backup_private * self = io->data;
	return self->position;
}

ssize_t st_db_postgresql_stream_backup_read(struct st_stream_reader * io, void * buffer, ssize_t length) {
	if (length < 1)
		return 0;

	struct st_db_postgresql_stream_backup_private * self = io->data;

	if (!*self->c_table)
		return 0;

	ssize_t nb_total_read = 0;
	char * c_buffer = buffer;
	if (self->c_line) {
		size_t c_length = strlen(self->c_line);
		memcpy(c_buffer, self->c_line, c_length);
		PQfreemem(self->c_line);
		self->c_line = 0;

		self->position += c_length;
		c_buffer += c_length;
		nb_total_read += c_length;
	}

	for (;;) {
		int status = PQgetCopyData(self->con, &self->c_line, 0);

		if (status > 0) {
			ssize_t c_length = strlen(self->c_line);
			if (c_length <= length - nb_total_read) {
				memcpy(c_buffer, self->c_line, c_length);

				self->position += c_length;
				c_buffer += c_length;
				nb_total_read += c_length;
			} else
				return nb_total_read;
		}

		if (self->c_line) {
			PQfreemem(self->c_line);
			self->c_line = 0;
		}

		if (status == -1) {
			PQclear(self->c_result);
			self->c_result = 0;

			self->c_table++;
			if (!*self->c_table)
				break;

			char * query;
			asprintf(&query, "COPY %s TO STDOUT WITH CSV HEADER", *self->c_table);

			self->c_result = PQexec(self->con, query);
			free(query);
		}
	}

	return nb_total_read;
}

off_t st_db_postgresql_stream_backup_set_position(struct st_stream_reader * io __attribute__((unused)), off_t position __attribute__((unused))) {
	return -1;
}

