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

#ifndef __LIBSTORIQONE_IO_CHECKSUM_H__
#define __LIBSTORIQONE_IO_CHECKSUM_H__

struct so_io_stream_checksum_backend {
	struct so_io_stream_checksum_backend_ops {
		struct so_value * (*digest)(struct so_io_stream_checksum_backend * worker);
		void (*finish)(struct so_io_stream_checksum_backend * worker);
		void (*free)(struct so_io_stream_checksum_backend * worker);
		void (*reset)(struct so_io_stream_checksum_backend * worker);
		void (*update)(struct so_io_stream_checksum_backend * worker, const void * buffer, ssize_t length);
	} * ops;
	void * data;
};

struct so_io_stream_checksum_backend * so_io_stream_checksum_backend_new(struct so_value * checksums);
struct so_io_stream_checksum_backend * so_io_stream_checksum_threaded_backend_new(struct so_value * checksums);

#endif
