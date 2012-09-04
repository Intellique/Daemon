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
*  Last modified: Fri, 17 Aug 2012 16:41:36 +0200                         *
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
#include <stone/log.h>

#include "common.h"

struct st_db_postgresql_stream_restore_private {
	PGconn * con;

	PGresult * c_result;
	char * buffer;
	size_t buffer_length;

	unsigned char finished;

	enum {
		RESTORE_STATUS_FOOTER,
		RESTORE_STATUS_HEADER,
		RESTORE_STATUS_LINE,
	} status;

	ssize_t position;
};

static const char * tables[] = {
	"restoreto", 
	"log",
	"jobtoselectedfile",
	"jobrecord",
	"jobtochecksum",
	"job",
	"jobtype", 
	"archivevolumetochecksumresult",
	"archivefiletochecksumresult",
	"checksumresult",
	"checksum",
	"archivefiletoarchivevolume", 
	"archivevolume",
	"archivefile",
	"archive",
	"userlog",
	"users",
	"selectedfile",
	"backupvolume",
	"backup",
	"changerslot",
	"drive",
	"changer",
	"host",
	"driveformatsupport",
	"driveformat",
	"tape",
	"pool",
	"tapeformat",

	0,
};


static int st_db_postgresql_stream_restore_close(struct st_stream_writer * io);
static void st_db_postgresql_stream_restore_free(struct st_stream_writer * io);
static ssize_t st_db_postgresql_stream_restore_get_available_size(struct st_stream_writer * io);
static ssize_t st_db_postgresql_stream_restore_get_block_size(struct st_stream_writer * io);
static int st_db_postgresql_stream_restore_last_errno(struct st_stream_writer * f);
static ssize_t st_db_postgresql_stream_restore_position(struct st_stream_writer * io);
static ssize_t st_db_postgresql_stream_restore_write(struct st_stream_writer * io, const void * buffer, ssize_t length);

static struct st_stream_writer_ops st_db_postgresql_stream_restore_ops = {
	.close              = st_db_postgresql_stream_restore_close,
	.free               = st_db_postgresql_stream_restore_free,
	.get_available_size = st_db_postgresql_stream_restore_get_available_size,
	.get_block_size     = st_db_postgresql_stream_restore_get_block_size,
	.last_errno         = st_db_postgresql_stream_restore_last_errno,
	.position           = st_db_postgresql_stream_restore_position,
	.write              = st_db_postgresql_stream_restore_write,
};


struct st_stream_writer * st_db_postgresql_init_restore(struct st_db_postgresql_private * driver_private) {
	struct st_db_postgresql_stream_restore_private * self = malloc(sizeof(struct st_db_postgresql_stream_restore_private));
	self->con = PQsetdbLogin(driver_private->host, driver_private->port, 0, 0, driver_private->db, driver_private->user, driver_private->password);
	self->buffer = 0;
	self->buffer_length = 0;
	self->finished = 0;
	self->status = RESTORE_STATUS_HEADER;
	self->position = 0;

	PQsetErrorVerbosity(self->con, PQERRORS_VERBOSE);

	const char ** ptr_table;
	for (ptr_table = tables; *ptr_table; ptr_table++) {
		char * query;
		asprintf(&query, "DELETE FROM %s", *ptr_table);

		PGresult * result = PQexec(self->con, query);
		ExecStatusType status = PQresultStatus(result);
		if (status == PGRES_FATAL_ERROR)
			st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Error while %s because %s", query, PQresultErrorMessage(result));
		PQclear(result);

		free(query);
	}

	struct st_stream_writer * writer = malloc(sizeof(struct st_stream_writer));
	writer->ops = &st_db_postgresql_stream_restore_ops;
	writer->data = self;

	return writer;
}

int st_db_postgresql_stream_restore_close(struct st_stream_writer * io) {
	struct st_db_postgresql_stream_restore_private * self = io->data;

	if (self->con) {
		if (self->c_result)
			PQclear(self->c_result);

		PQfinish(self->con);
		self->con = 0;
	}

	free(self->buffer);
	self->buffer = 0;

	return 0;
}

void st_db_postgresql_stream_restore_free(struct st_stream_writer * io) {
	st_db_postgresql_stream_restore_close(io);

	free(io->data);
	free(io);
}

ssize_t st_db_postgresql_stream_restore_get_available_size(struct st_stream_writer * io __attribute__((unused))) {
	return 65536;
}

ssize_t st_db_postgresql_stream_restore_get_block_size(struct st_stream_writer * io __attribute__((unused))) {
	return 32768;
}

int st_db_postgresql_stream_restore_last_errno(struct st_stream_writer * io __attribute__((unused))) {
	return 0;
}

ssize_t st_db_postgresql_stream_restore_position(struct st_stream_writer * io) {
	struct st_db_postgresql_stream_restore_private * self = io->data;
	return self->position;
}

ssize_t st_db_postgresql_stream_restore_write(struct st_stream_writer * io, const void * buffer, ssize_t length) {
	struct st_db_postgresql_stream_restore_private * self = io->data;

	if (!self->buffer) {
		self->buffer = malloc(length + 1);
		self->buffer_length = length;
		memcpy(self->buffer, buffer, length);
		self->buffer[length] = '\0';
	} else {
		self->buffer = realloc(self->buffer, self->buffer_length + length + 1);
		memcpy(self->buffer + self->buffer_length, buffer, length);
		self->buffer_length += length;
		self->buffer[self->buffer_length] = '\0';
	}

	int stop = 0;
	char * table = 0;
	char * line = self->buffer;
	char * ptr;
	char * query = 0;
	ExecStatusType status;
	int failed;

	while (*line && !stop) {
		switch (self->status) {
			case RESTORE_STATUS_FOOTER:
				failed = PQputCopyEnd(self->con, NULL);
				if (failed < 0)
					st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Error while finishing copy because %s", PQresultErrorMessage(self->c_result));

				self->status = RESTORE_STATUS_HEADER;
				break;

			case RESTORE_STATUS_HEADER:
				table = line;
				ptr = strchr(line, ' ');

				if (ptr) {
					*ptr = '\0';
					ptr++;

					asprintf(&query, "COPY %s FROM STDIN", table);
					self->c_result = PQexec(self->con, query);
					status = PQresultStatus(self->c_result);
					if (status == PGRES_FATAL_ERROR)
						st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Error while '%s' because %s", query, PQresultErrorMessage(self->c_result));

					PQclear(self->c_result);
					self->c_result = 0;

					self->status = RESTORE_STATUS_LINE;

					line = ptr;
				} else {
					stop = 1;
				}

				break;

			case RESTORE_STATUS_LINE:
				ptr = strchr(line, '\n');
				if (!ptr) {
					stop = 1;
					break;
				}

				*ptr = '\0';
				failed = PQputCopyData(self->con, line, strlen(line));
				line = ptr + 1;

				if (failed < 0)
					st_log_write_all(st_log_level_error, st_log_type_plugin_db, "Error while copy because %s", PQresultErrorMessage(self->c_result));

				if (*line == '\n') {
					line++;
					self->status = RESTORE_STATUS_FOOTER;
				}

				break;
		}
	}

	memmove(self->buffer, line, self->buffer_length - (line - self->buffer));
	self->buffer_length -= line - self->buffer;
	self->buffer[self->buffer_length] = '\0';

	self->position += length;

	return length;
}

