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

#ifndef __STORIQONE_JOB_COPY_ARCHIVE_H__
#define __STORIQONE_JOB_COPY_ARCHIVE_H__

struct soj_copyarchive_private {
	struct so_archive * src_archive;
	struct so_drive * src_drive;

	struct so_archive * copy_archive;
	struct so_pool * pool;
	struct so_value_iterator * media_iterator;
	struct so_media * media;
	struct so_drive * dest_drive;
};

struct so_database_connection;
struct so_job;

int soj_copyarchive_indirect_copy(struct so_job * job, struct so_database_connection * db_connect, struct soj_copyarchive_private * self);

#endif
