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
*  Last modified: Wed, 20 Mar 2013 13:02:18 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// va_end, va_start
#include <stdarg.h>
// free, mkstemps
#include <stdlib.h>
// vasprintf
#include <stdio.h>

#include <libstone/log.h>

#include "io.h"


ssize_t st_stream_writer_printf(struct st_stream_writer * writer, const char * format, ...) {
	if (writer == NULL || format == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "st_stream_writer_printf: error because");
		if (writer == NULL)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "writer is NULL");
		if (format == NULL)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "format is NULL");
		return -1;
	}

	char * message = NULL;

	va_list va;
	va_start(va, format);
	ssize_t str_message = vasprintf(&message, format, va);
	va_end(va);

	ssize_t nb_write = writer->ops->write(writer, message, str_message);

	free(message);

	return nb_write;
}

struct st_stream_writer * st_io_temp_writer(char * filename, int sufixlength) {
	if (filename == NULL)
		return NULL;

	int fd = mkstemps(filename, sufixlength);
	if (fd < 0)
		return NULL;

	return st_io_file_writer2(fd);
}

