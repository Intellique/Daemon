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

// calloc, malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>

#include <libstoriqone/database.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/json.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/changer.h>

#include "drive.h"
#include "job.h"

struct soj_changer {
	int fd;
	struct so_value * config;
};

static struct so_changer * soj_changers = NULL;
static unsigned int soj_nb_changers = 0;

static struct so_drive * soj_changer_get_media(struct so_changer * changer, struct so_slot * slot, bool no_wait);
static ssize_t soj_changer_reserve_media(struct so_changer * changer, struct so_slot * slot, size_t size_need, enum so_pool_unbreakable_level unbreakable_level);
static int soj_changer_release_media(struct so_changer * changer, struct so_slot * slot);
static int soj_changer_sync(struct so_changer * changer);

static struct so_changer_ops soj_changer_ops = {
	.get_media     = soj_changer_get_media,
	.release_media = soj_changer_release_media,
	.reserve_media = soj_changer_reserve_media,
	.sync          = soj_changer_sync,
};


struct so_slot * soj_changer_find_media_by_job(struct so_job * job, struct so_database_connection * db_connection) {
	if (job == NULL || db_connection == NULL)
		return NULL;

	struct so_media * media = db_connection->ops->get_media(db_connection, NULL, NULL, job);
	if (media == NULL)
		return NULL;

	unsigned int i;
	for (i = 0; i < soj_nb_changers; i++) {
		struct so_changer * ch = soj_changers + i;

		unsigned int j;
		for (j = 0; j < ch->nb_slots; j++) {
			struct so_slot * sl = ch->slots + j;
			if (sl->media == NULL)
				continue;

			if (!strcmp(sl->media->medium_serial_number, media->medium_serial_number))
				return sl;
		}
	}

	return NULL;
}

struct so_slot * soj_changer_find_slot(struct so_media * media) {
	if (media == NULL)
		return NULL;

	unsigned int i;
	for (i = 0; i < soj_nb_changers; i++) {
		struct so_changer * ch = soj_changers + i;

		unsigned int j;
		for (j = 0; j < ch->nb_slots; j++) {
			struct so_slot * sl = ch->slots + j;
			if (sl->media == NULL)
				continue;

			if (!strcmp(sl->media->medium_serial_number, media->medium_serial_number))
				return sl;
		}
	}

	return NULL;
}

static struct so_drive * soj_changer_get_media(struct so_changer * changer, struct so_slot * slot, bool no_wait) {
	struct soj_changer * self = changer->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{sss{sssisb}}",
		"command", "get media",
		"params",
			"job key", job->key,
			"slot", slot->index,
			"no wait", no_wait
	);
	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	job->status = so_job_status_waiting;

	bool error = false;
	while (!job->stopped_by_user && !error) {
		struct so_value * response = so_json_parse_fd(self->fd, 5000);
		if (response == NULL)
			continue;

		long int index = -1;
		struct so_value * vch = NULL;
		so_value_unpack(response, "{sbsosi}",
			"error", &error,
			"changer", &vch,
			"index", &index
		);
		if (index > -1 && slot->index != index)
			so_slot_swap(slot, changer->drives[index].slot);
		so_changer_sync(changer, vch);
		so_value_free(response);

		if (!error) {
			job->status = so_job_status_running;
			return changer->drives + index;
		}
	}

	if (no_wait)
		job->status = so_job_status_running;
	else
		job->status = so_job_status_error;
	return NULL;
}

static int soj_changer_release_media(struct so_changer * changer, struct so_slot * slot) {
	struct soj_changer * self = changer->data;

	struct so_value * request = so_value_pack("{sss{si}}", "command", "release media", "params", "slot", slot->index);
	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return 1;

	bool ok = false;
	so_value_unpack(response, "{sb}", "status", &ok);
	so_value_free(response);

	return ok ? 0 : 1;
}

static ssize_t soj_changer_reserve_media(struct so_changer * changer, struct so_slot * slot, size_t size_need, enum so_pool_unbreakable_level unbreakable_level) {
	struct soj_changer * self = changer->data;

	struct so_value * request = so_value_pack("{sss{sisiss}}",
		"command", "reserve media",
		"params",
			"slot", slot->index,
			"size need", size_need,
			"unbreakable level", so_pool_unbreakable_level_to_string(unbreakable_level, false)
	);
	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return 1;

	ssize_t returned = 0;
	so_value_unpack(response, "{si}", "returned", &returned);
	so_value_free(response);

	return returned;
}

void soj_changer_set_config(struct so_value * config) {
	soj_nb_changers = so_value_list_get_length(config);
	soj_changers = calloc(soj_nb_changers, sizeof(struct so_changer));

	struct so_value_iterator * iter = so_value_list_get_iterator(config);

	unsigned int i;
	for (i = 0; i < soj_nb_changers; i++) {
		struct so_changer * ch = soj_changers + i;
		struct so_value * vch = so_value_iterator_get_value(iter, false);

		struct so_value * drives = NULL, * socket = NULL;
		so_value_unpack(vch, "{sosO}", "drives", &drives, "socket", &socket);

		ch->nb_drives = so_value_list_get_length(drives);
		ch->drives = calloc(ch->nb_drives, sizeof(struct so_drive));

		struct soj_changer * self = ch->data = malloc(sizeof(struct soj_changer));
		bzero(self, sizeof(struct soj_changer));
		self->config = socket;
		self->fd = so_socket(socket);

		ch->ops = &soj_changer_ops;

		soj_changer_sync(ch);

		unsigned int j;
		struct so_value_iterator * iter = so_value_list_get_iterator(drives);
		for (j = 0; j < ch->nb_drives; j++) {
			struct so_value * dr_config = so_value_iterator_get_value(iter, false);
			soj_drive_init(ch->drives + j, dr_config);
		}
		so_value_iterator_free(iter);
	}

	so_value_iterator_free(iter);
}

static int soj_changer_sync(struct so_changer * changer) {
	struct soj_changer * self = changer->data;

	struct so_value * request = so_value_pack("{ss}", "command", "sync");
	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return 1;

	struct so_value * vch = NULL;
	so_value_unpack(response, "{so}", "changer", &vch);

	so_changer_sync(changer, vch);

	so_value_free(response);

	return 0;
}

int soj_changer_sync_all() {
	int failed = 0;
	unsigned int i;
	for (i = 0; i < soj_nb_changers && failed == 0; i++) {
		struct so_changer * ch = soj_changers + i;
		failed = ch->ops->sync(ch);
	}
	return failed;
}


bool soj_changer_has_apt_drive(struct so_media_format * format, bool for_writing) {
	if (format == NULL)
		return false;

	unsigned int i;
	for (i = 0; i < soj_nb_changers; i++) {
		struct so_changer * ch = soj_changers + i;

		unsigned int j;
		for (j = 0; j < ch->nb_drives; j++) {
			struct so_drive * dr = ch->drives + i;
			if (dr->ops->check_support(dr, format, for_writing))
				return true;
		}
	}

	return false;
}

