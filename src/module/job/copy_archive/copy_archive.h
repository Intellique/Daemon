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
*  Last modified: Wed, 16 Jan 2013 12:02:44 +0100                         *
\*************************************************************************/

#ifndef __STONE_JOB_COPYARCHIVE_H__
#define __STONE_JOB_COPYARCHIVE_H__

#include <libstone/job.h>

struct st_media;

struct st_job_copy_archive_private {
	struct st_job * job;
	struct st_database_connection * connect;

	ssize_t total_done;
	ssize_t archive_size;
	struct st_archive * archive;

	struct st_drive * drive_input;
	struct st_slot * slot_input;

	struct st_pool * pool;
	struct st_drive * drive_output;
	struct st_slot * slot_output;
};

int st_job_copy_archive_direct_copy(struct st_job_copy_archive_private * self);
int st_job_copy_archive_indirect_copy(struct st_job_copy_archive_private * self);
bool st_job_copy_archive_select_input_media(struct st_job_copy_archive_private * self, struct st_media * media);
bool st_job_copy_archive_select_output_media(struct st_job_copy_archive_private * self, bool load_media);

#endif

