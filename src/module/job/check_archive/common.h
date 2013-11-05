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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Thu, 06 Jun 2013 14:48:46 +0200                            *
\****************************************************************************/

#ifndef __STONE_JOB_CHECKARCHIVE_H__
#define __STONE_JOB_CHECKARCHIVE_H__

// bool
#include <stdbool.h>

#include <libstone/database.h>
#include <libstone/library/archive.h>

struct st_job_check_archive_private {
	struct st_job * job;
	struct st_database_connection * connect;

	struct st_archive * archive;

	char ** vol_checksums;
	unsigned int nb_vol_checksums;

	struct st_stream_reader * checksum_reader;

	struct st_job_check_archive_report * report;
};

void st_job_check_archive_report_add_file(struct st_job_check_archive_report * report, struct st_archive_files * file);
void st_job_check_archive_report_add_volume(struct st_job_check_archive_report * report, struct st_archive_volume * volume);
void st_job_check_archive_report_check_file(struct st_job_check_archive_report * report, bool ok);
void st_job_check_archive_report_check_volume(struct st_job_check_archive_report * report, bool ok);
void st_job_check_archive_report_check_volume2(struct st_job_check_archive_report * report, struct st_archive_volume * volume, bool ok);
void st_job_check_archive_report_free(struct st_job_check_archive_report * report);
char * st_job_check_archive_report_make(struct st_job_check_archive_report * report);
struct st_job_check_archive_report * st_job_check_archive_report_new(struct st_job * job, struct st_archive * archive, bool quick_mode);
int st_job_check_archive_quick_mode(struct st_job_check_archive_private * self);
int st_job_check_archive_thorough_mode(struct st_job_check_archive_private * self);

#endif

