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

#define NULL (void *) 0

#include <libstone/database.h>
#include <libstone/host.h>
#include <libstone/log.h>
#include <libstone/media.h>
#include <libstone/slot.h>
#include <libstone/value.h>
#include <libstone-job/changer.h>
#include <libstone-job/job.h>
#include <libstone-job/script.h>

#include <job_format-media.chcksum>

static struct st_pool * formatmedia_pool = NULL;
static struct st_slot * formatmedia_slot = NULL;

static void formatmedia_init(void) __attribute__((constructor));
static int formatmedia_simulate(struct st_job * job, struct st_database_connection * db_connect);
static int formatmedia_script_pre_run(struct st_job * job, struct st_database_connection * db_connect);

static struct st_job_driver formatmedia = {
	.name = "format-media",

	.simulate       = formatmedia_simulate,
	.script_pre_run = formatmedia_script_pre_run,
};


static void formatmedia_init() {
	stj_job_register(&formatmedia);
}

static int formatmedia_simulate(struct st_job * job, struct st_database_connection * db_connect) {
	formatmedia_pool = db_connect->ops->get_pool(db_connect, NULL, job);
	formatmedia_slot = stj_changer_find_media_by_job(job, db_connect);

	if (formatmedia_pool == NULL) {
		st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "Media not found");
		return 1;
	}
	if (formatmedia_slot == NULL) {
		st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "Media not found");
		return 1;
	}

	return 0;
}

static int formatmedia_script_pre_run(struct st_job * job, struct st_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, st_script_type_pre_job, formatmedia_pool) < 1)
		return 0;

	struct st_value * json = st_value_pack("{sosososo}",
		"job", st_job_convert(job),
		"host", st_host_get_info(),
		"media", st_media_convert(formatmedia_slot->media),
		"pool", st_pool_convert(formatmedia_pool)
	);

	int failed = 0;

	st_value_free(json);

	return failed;
}

