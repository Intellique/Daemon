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
*  Last modified: Sun, 09 Sep 2012 23:00:01 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// PQfreemem, PQgetCopyData
#include <postgresql/libpq-fe.h>
// bool
#include <stdbool.h>
// free
#include <stdlib.h>
// asprintf
#include <stdio.h>
// memcpy
#include <string.h>

#include <libstone/io.h>

#include "common.h"

struct st_db_postgresql_stream_backup_private {
	PGconn * con;

	PGresult * c_result;
	const char ** c_table;
	char * c_header;
	char * c_line;

	enum {
		BACKUP_STATUS_END,
		BACKUP_STATUS_FOOTER,
		BACKUP_STATUS_HEADER,
		BACKUP_STATUS_LINE,
	} status;

	ssize_t position;
};

static const char * tables[] = {
	"tapeformat", "pool", "tape", "driveformat", "driveformatsupport", "host", "changer", "drive", "changerslot", 
	"selectedfile", "users", "userlog", "archive", "archivefile", "archivevolume", "archivefiletoarchivevolume", 
	"checksum", "checksumresult", "archivefiletochecksumresult", "archivevolumetochecksumresult", "jobtype", 
	"job", "jobtochecksum", "jobrecord", "jobtoselectedfile", "log", "restoreto", 

	NULL,
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


struct st_stream_reader * st_db_postgresql_backup_init(PGconn * pg_connect) {
	struct st_db_postgresql_stream_backup_private * self = malloc(sizeof(struct st_db_postgresql_stream_backup_private));
	self->con = pg_connect;
	self->c_result = NULL;
	self->c_table = tables;
	self->c_header = NULL;
	self->c_line = NULL;
	self->status = BACKUP_STATUS_HEADER;
	self->position = 0;

	PGresult * result = PQexec(self->con, "BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY");
	PQclear(result);

	char * query;
	asprintf(&query, "COPY %s TO STDOUT WITH CSV HEADER", *self->c_table);
	self->c_result = PQexec(self->con, query);
	free(query);

	struct st_stream_reader * reader = malloc(sizeof(struct st_stream_reader));
	reader->ops = &st_db_postgresql_stream_backup_ops;
	reader->data = self;

	return reader;
}

static int st_db_postgresql_stream_backup_close(struct st_stream_reader * io) {
	struct st_db_postgresql_stream_backup_private * self = io->data;

	if (self->con != NULL) {
		PGresult * result = PQexec(self->con, "ROLLBACK");
		PQclear(result);
		PQfinish(self->con);
	}
	self->con = NULL;

	self->c_table = NULL;
	if (self->c_header)
		free(self->c_header);
	self->c_header = NULL;
	if (self->c_line)
		PQfreemem(self->c_line);
	self->c_line = NULL;

	return 0;
}

static bool st_db_postgresql_stream_backup_end_of_file(struct st_stream_reader * io) {
	struct st_db_postgresql_stream_backup_private * self = io->data;
	return self->c_table != NULL ? true : false;
}

static off_t st_db_postgresql_stream_backup_forward(struct st_stream_reader * io __attribute__((unused)), off_t offset __attribute__((unused))) {
	return -1;
}

static void st_db_postgresql_stream_backup_free(struct st_stream_reader * io) {
	struct st_db_postgresql_stream_backup_private * self = io->data;

	if (self->con == NULL)
		PQfinish(self->con);
	self->con = NULL;

	free(self);
	free(io);
}

static ssize_t st_db_postgresql_stream_backup_get_block_size(struct st_stream_reader * io __attribute__((unused))) {
	return 4096;
}

static int st_db_postgresql_stream_backup_last_errno(struct st_stream_reader * io __attribute__((unused))) {
	return 0;
}

static ssize_t st_db_postgresql_stream_backup_position(struct st_stream_reader * io) {
	struct st_db_postgresql_stream_backup_private * self = io->data;
	return self->position;
}

static ssize_t st_db_postgresql_stream_backup_read(struct st_stream_reader * io, void * buffer, ssize_t length) {
	if (length < 1)
		return 0;

	struct st_db_postgresql_stream_backup_private * self = io->data;

	ssize_t nb_total_read = 0;
	char * c_buffer = buffer;

	ssize_t c_length;
	int status = 0;

	while (status > -2) {
		switch (self->status) {
			case BACKUP_STATUS_END:
				return nb_total_read;

			case BACKUP_STATUS_FOOTER:
				if (1 <= length - nb_total_read)
					memcpy(c_buffer, "\n", 1);
				else
					return nb_total_read;

				self->position++;
				c_buffer++;
				nb_total_read++;

				PQclear(self->c_result);
				self->c_result = NULL;

				self->c_table++;
				if (!*self->c_table) {
					self->status = BACKUP_STATUS_END;
					return nb_total_read;
				}

				char * query;
				asprintf(&query, "COPY %s TO STDOUT WITH CSV HEADER", *self->c_table);

				self->c_result = PQexec(self->con, query);
				free(query);

				self->status = BACKUP_STATUS_HEADER;

				break;

			case BACKUP_STATUS_HEADER:
				status = PQgetCopyData(self->con, &self->c_line, 0);

				if (status < 0)
					return nb_total_read;

				asprintf(&self->c_header, "%s %s", *self->c_table, self->c_line);
				PQfreemem(self->c_line);
				self->c_line = NULL;

				c_length = strlen(self->c_header);

				if (c_length <= length - nb_total_read) {
					memcpy(c_buffer, self->c_header, c_length);
					free(self->c_header);
					self->c_header = NULL;
				} else
					return nb_total_read;

				self->position += c_length;
				c_buffer += c_length;
				nb_total_read += c_length;

				status = PQgetCopyData(self->con, &self->c_line, 0);

				if (status == -1)
					self->status = BACKUP_STATUS_FOOTER;
				else
					self->status = BACKUP_STATUS_LINE;

				break;

			case BACKUP_STATUS_LINE:
				c_length = strlen(self->c_line);

				if (c_length <= length - nb_total_read) {
					memcpy(c_buffer, self->c_line, c_length);
					PQfreemem(self->c_line);
					self->c_line = NULL;
				} else
					return nb_total_read;

				self->position += c_length;
				c_buffer += c_length;
				nb_total_read += c_length;

				status = PQgetCopyData(self->con, &self->c_line, 0);

				if (status == -1)
					self->status = BACKUP_STATUS_FOOTER;

				break;
		}
	}

	return nb_total_read;
}

