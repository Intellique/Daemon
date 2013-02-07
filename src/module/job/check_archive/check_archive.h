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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Thu, 07 Feb 2013 11:25:39 +0100                         *
\*************************************************************************/

#ifndef __STONE_JOB_CHECKARCHIVE_H__
#define __STONE_JOB_CHECKARCHIVE_H__

#include <libstone/database.h>
#include <libstone/library/archive.h>

struct st_job_check_archive_private {
	struct st_job * job;
	struct st_database_connection * connect;

	struct st_archive * archive;

	char ** vol_checksums;
	unsigned int nb_vol_checksums;

	struct st_stream_reader * checksum_reader;
};

int st_job_check_archive_quick_mode(struct st_job_check_archive_private * self);
int st_job_check_archive_thorough_mode(struct st_job_check_archive_private * self);

#endif

