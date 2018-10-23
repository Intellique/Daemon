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
*  Copyright (C) 2013-2018, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// strlen
#include <string.h>
// S_*
#include <sys/stat.h>

#include <libstoriqone-job/format.h>

ssize_t soj_format_compute_tar_size(struct so_format_reader * reader) {
	ssize_t sum = 0;
	enum so_format_reader_header_status status;
	struct so_format_file file;
	while (status = reader->ops->get_header(reader, &file), status == so_format_reader_header_ok) {
		sum += 512;
		ssize_t path_length = strlen(file.filename);
		if (S_ISLNK(file.mode)) {
			ssize_t link_length = strlen(file.link);
			if (path_length > 100 && link_length > 100) {
				sum += 2048 + path_length - path_length % 512 + link_length - link_length % 512;
			} else if (path_length > 100) {
				sum += 1024 + path_length - path_length % 512;
			} else if (link_length > 100) {
				sum += 1024 + link_length - link_length % 512;
			}
		} else if (path_length > 100) {
			sum += 512 + path_length - path_length % 512;
		}

		if (S_ISREG(file.mode))
			sum += 512 + file.size - file.size % 512;

		so_format_file_free(&file);
	}
	return sum;
}
