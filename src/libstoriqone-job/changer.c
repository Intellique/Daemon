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

// pthread_mutex_init, pthread_mutex_lock, pthread_mutex_unlock
// pthread_mutexattr_destroy, pthread_mutexattr_init, pthread_mutexattr_settype
#include <pthread.h>
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
#include "media.h"

struct soj_changer {
	int fd;
	struct so_value * config;

	pthread_mutex_t lock;

	bool is_online;
};

static struct so_changer * soj_changers = NULL;
static unsigned int soj_nb_changers = 0;

static struct so_drive * soj_changer_get_media(struct so_changer * changer, struct so_media * media, bool no_wait);
static ssize_t soj_changer_reserve_media(struct so_changer * changer, struct so_media * media, size_t size_need, enum so_pool_unbreakable_level unbreakable_level);
static int soj_changer_release_media(struct so_changer * changer, struct so_media * media);
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

static struct so_drive * soj_changer_get_media(struct so_changer * changer, struct so_media * media, bool no_wait) {
	struct soj_changer * self = changer->data;
	struct so_job * job = soj_job_get();

	struct so_slot * slot = NULL;
	unsigned int i;
	for (i = 0; i < changer->nb_slots; i++) {
		struct so_slot * sl = changer->slots + i;
		if (sl->media == NULL)
			continue;

		if (!strcmp(sl->media->medium_serial_number, media->medium_serial_number)) {
			slot = sl;
			break;
		}
	}

	struct so_value * request = so_value_pack("{sss{sssssb}}",
		"command", "get media",
		"params",
			"job id", job->id,
			"medium serial number", media->medium_serial_number,
			"no wait", no_wait
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	job->status = so_job_status_waiting;

	struct so_value * response = NULL;
	while (!job->stopped_by_user && response == NULL)
		response = so_json_parse_fd(self->fd, 5000);

	pthread_mutex_unlock(&self->lock);

	if (response != NULL) {
		bool error = true;
		unsigned int index = 0;
		struct so_value * vch = NULL;

		int nb_parsed = so_value_unpack(response, "{sbsosu}",
			"error", &error,
			"changer", &vch,
			"index", &index
		);

		if (!error) {
			if (nb_parsed >= 3 && slot->index != index)
				so_slot_swap(slot, changer->drives[index].slot);

			so_changer_sync(changer, vch);
		}

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

static int soj_changer_release_media(struct so_changer * changer, struct so_media * media) {
	struct soj_changer * self = changer->data;

	struct so_value * request = so_value_pack("{sss{ss}}",
		"command", "release media",
		"params",
			"medium serial number", media->medium_serial_number
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	bool ok = false;
	struct so_value * response = so_json_parse_fd(self->fd, -1);

	pthread_mutex_unlock(&self->lock);

	if (response != NULL) {
		so_value_unpack(response, "{sb}", "status", &ok);
		so_value_free(response);
	}

	return ok ? 0 : 1;
}

static ssize_t soj_changer_reserve_media(struct so_changer * changer, struct so_media * media, size_t size_need, enum so_pool_unbreakable_level unbreakable_level) {
	struct soj_changer * self = changer->data;

	struct so_value * request = so_value_pack("{sss{ssszss}}",
		"command", "reserve media",
		"params",
			"medium serial number", media->medium_serial_number,
			"size need", size_need,
			"unbreakable level", so_pool_unbreakable_level_to_string(unbreakable_level, false)
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	ssize_t returned = 1;
	struct so_value * response = so_json_parse_fd(self->fd, -1);

	pthread_mutex_unlock(&self->lock);

	if (response != NULL) {
		so_value_unpack(response, "{sz}", "returned", &returned);
		so_value_free(response);
	}

	return returned;
}

void soj_changer_set_config(struct so_value * config) {
	struct so_job * job = soj_job_get();

	soj_nb_changers = so_value_list_get_length(config);
	soj_changers = calloc(soj_nb_changers, sizeof(struct so_changer));

	struct so_value_iterator * iter = so_value_list_get_iterator(config);

	unsigned int i;
	for (i = 0; i < soj_nb_changers; i++) {
		struct so_changer * ch = soj_changers + i;
		struct so_value * vch = so_value_iterator_get_value(iter, false);

		struct so_value * socket = NULL;
		so_value_unpack(vch, "{sO}", "socket", &socket);

		struct soj_changer * self = ch->data = malloc(sizeof(struct soj_changer));
		bzero(self, sizeof(struct soj_changer));
		self->config = socket;
		self->fd = so_socket(socket);
		self->is_online = true;

		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

		pthread_mutex_init(&self->lock, &attr);

		ch->ops = &soj_changer_ops;

		pthread_mutexattr_destroy(&attr);

		struct so_value * command = so_value_pack("{sss{s{sssI}}}",
			"command", "get drives config",
			"params",
				"job",
					"id", job->id,
					"num run", job->num_runs
		);
		so_json_encode_to_fd(command, self->fd, true);
		so_value_free(command);

		struct so_value * response = so_json_parse_fd(self->fd, -1);

		ch->nb_drives = so_value_list_get_length(response);
		ch->drives = calloc(ch->nb_drives, sizeof(struct so_drive));

		soj_changer_sync(ch);

		unsigned int j;
		struct so_value_iterator * iter = so_value_list_get_iterator(response);
		for (j = 0; j < ch->nb_drives; j++) {
			struct so_value * dr_config = so_value_iterator_get_value(iter, false);
			soj_drive_init(ch->drives + j, dr_config);
		}
		so_value_iterator_free(iter);

		so_value_free(response);
	}

	so_value_iterator_free(iter);
}

static int soj_changer_sync(struct so_changer * changer) {
	struct soj_changer * self = changer->data;

	struct so_value * request = so_value_pack("{ss}", "command", "sync");

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);

	pthread_mutex_unlock(&self->lock);

	if (response == NULL)
		return 1;

	struct so_value * vch = NULL;
	so_value_unpack(response, "{so}", "changer", &vch);

	so_changer_sync(changer, vch);

	so_value_free(response);


	if (changer->is_online && !self->is_online)
		soj_media_check_reserved();
	else if (!changer->is_online && self->is_online)
		soj_media_reserve_new_media();
	self->is_online = changer->is_online;

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

