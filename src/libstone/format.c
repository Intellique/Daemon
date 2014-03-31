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
*  Last modified: Wed, 13 Feb 2013 16:29:57 +0100                            *
\****************************************************************************/

// free
#include <stdlib.h>

#include "format.h"

ssize_t st_format_find_greater_file(char * path, struct st_media_format * format __attribute__((unused))) {
	return st_format_tar_find_greater_file(path);
}

void st_format_file_free(struct st_format_file * file) {
	if (file == NULL)
		return;

	free(file->filename);
	free(file->link);
	free(file->user);
	free(file->group);
}

struct st_format_reader * st_format_get_reader(struct st_stream_reader * reader, struct st_media_format * format __attribute__((unused))) {
	return st_format_tar_get_reader(reader);
}

ssize_t st_format_get_size(const char * path, bool recursive, struct st_media_format * format __attribute__((unused))) {
	return st_format_tar_get_size(path, recursive);
}

struct st_format_writer * st_format_get_writer(struct st_stream_writer * writer, struct st_media_format * format __attribute__((unused))) {
	return st_format_tar_get_writer(writer);
}

