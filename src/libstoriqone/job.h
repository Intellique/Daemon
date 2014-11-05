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
\****************************************************************************/

#ifndef __LIBSTONE_JOB_P_H__
#define __LIBSTONE_JOB_P_H__

#include <libstone/job.h>

int st_job_add_record_v1(struct st_job * job, struct st_database_connection * db_connect, enum st_log_level level, enum st_job_record_notif notif, const char * format, ...) __attribute__ ((nonnull(1,2),format(printf, 5, 6)));
struct st_value * st_job_convert_v1(struct st_job * job) __attribute__((nonnull,warn_unused_result));
void st_job_free_v1(struct st_job * job) __attribute__((nonnull));
void st_job_free2_v1(void * job) __attribute__((nonnull));
const char * st_job_report_notif_to_string_v1(enum st_job_record_notif notif, bool translate);
const char * st_job_status_to_string_v1(enum st_job_status status, bool translate);
enum st_job_record_notif st_job_string_to_record_notif_v1(const char * notif) __attribute__((nonnull));
enum st_job_status st_job_string_to_status_v1(const char * status) __attribute__((nonnull));
void st_job_sync_v1(struct st_job * job, struct st_value * new_job) __attribute__((nonnull));

#endif

