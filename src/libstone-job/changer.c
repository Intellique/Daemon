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

// calloc, malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>

#include <libstone/database.h>
#include <libstone/drive.h>
#include <libstone/json.h>
#include <libstone/slot.h>
#include <libstone/socket.h>
#include <libstone/value.h>

#include "changer.h"
#include "drive.h"

struct stj_changer {
	int fd;
	struct st_value * config;
};

static struct st_changer * stj_changers = NULL;
static unsigned int stj_nb_changers = 0;

static int stj_changer_load(struct st_changer * changer, struct st_slot * from, struct st_drive * to);
static int stj_changer_release_all_media(struct st_changer * changer);
static int stj_changer_release_media(struct st_changer * changer, struct st_slot * slot);
static int stj_changer_reserve_media(struct st_changer * changer, struct st_slot * slot);
static int stj_changer_sync(struct st_changer * changer);
static int stj_changer_unload(struct st_changer * changer, struct st_drive * from);

static struct st_changer_ops stj_changer_ops = {
	.load              = stj_changer_load,
	.release_all_media = stj_changer_release_all_media,
	.release_media     = stj_changer_release_media,
	.reserve_media     = stj_changer_reserve_media,
	.sync              = stj_changer_sync,
	.unload            = stj_changer_unload,
};


__asm__(".symver stj_changer_find_media_by_job_v1, stj_changer_find_media_by_job@@LIBSTONE_JOB_1.2");
struct st_slot * stj_changer_find_media_by_job_v1(struct st_job * job, struct st_database_connection * db_connection) {
	if (job == NULL || db_connection == NULL)
		return NULL;

	struct st_media * media = db_connection->ops->get_media(db_connection, NULL, NULL, job);
	if (media == NULL)
		return NULL;

	unsigned int i;
	for (i = 0; i < stj_nb_changers; i++) {
		struct st_changer * ch = stj_changers + i;

		unsigned int j;
		for (j = 0; j < ch->nb_slots; j++) {
			struct st_slot * sl = ch->slots + j;
			if (sl->media == NULL)
				continue;

			if (!strcmp(sl->media->medium_serial_number, media->medium_serial_number))
				return sl;
		}
	}

	return NULL;
}

static int stj_changer_load(struct st_changer * changer, struct st_slot * from, struct st_drive * to) {
	struct stj_changer * self = changer->data;

	struct st_value * request = st_value_pack("{sss{sisi}}", "command", "load", "params", "from", (long int) from->index, "to", (long int) to->slot->index);
	st_json_encode_to_fd(request, self->fd, true);
	st_value_free(request);

	struct st_value * response = st_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return 1;

	bool ok = false;
	st_value_unpack(response, "{sb}", "status", &ok);
	st_value_free(response);

	return ok ? 0 : 1;
}

static int stj_changer_release_all_media(struct st_changer * changer) {
	struct stj_changer * self = changer->data;

	struct st_value * request = st_value_pack("{ss}", "command", "release all media");
	st_json_encode_to_fd(request, self->fd, true);
	st_value_free(request);

	struct st_value * response = st_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return 1;

	bool ok = false;
	st_value_unpack(response, "{sb}", "status", &ok);
	st_value_free(response);

	return ok ? 0 : 1;
}

static int stj_changer_release_media(struct st_changer * changer, struct st_slot * slot) {
	struct stj_changer * self = changer->data;

	struct st_value * request = st_value_pack("{sss{si}}", "command", "release media", "params", "slot", slot->index);
	st_json_encode_to_fd(request, self->fd, true);
	st_value_free(request);

	struct st_value * response = st_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return 1;

	bool ok = false;
	st_value_unpack(response, "{sb}", "status", &ok);
	st_value_free(response);

	return ok ? 0 : 1;
}

static int stj_changer_reserve_media(struct st_changer * changer, struct st_slot * slot) {
	struct stj_changer * self = changer->data;

	struct st_value * request = st_value_pack("{sss{si}}", "command", "reserve media", "params", "slot", slot->index);
	st_json_encode_to_fd(request, self->fd, true);
	st_value_free(request);

	struct st_value * response = st_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return 1;

	bool ok = false;
	st_value_unpack(response, "{sb}", "status", &ok);
	st_value_free(response);

	return ok ? 0 : 1;
}

__asm__(".symver stj_changer_set_config_v1, stj_changer_set_config@@LIBSTONE_JOB_1.2");
void stj_changer_set_config_v1(struct st_value * config) {
	stj_nb_changers = st_value_list_get_length(config);
	stj_changers = calloc(stj_nb_changers, sizeof(struct st_changer));

	struct st_value_iterator * iter = st_value_list_get_iterator(config);

	unsigned int i;
	for (i = 0; i < stj_nb_changers; i++) {
		struct st_changer * ch = stj_changers + i;
		struct st_value * vch = st_value_iterator_get_value(iter, false);

		struct st_value * drives = NULL, * socket = NULL;
		st_value_unpack(vch, "{sosO}", "drives", &drives, "socket", &socket);

		ch->nb_drives = st_value_list_get_length(drives);
		ch->drives = calloc(ch->nb_drives, sizeof(struct st_drive));

		struct stj_changer * self = ch->data = malloc(sizeof(struct stj_changer));
		bzero(self, sizeof(struct stj_changer));
		self->config = socket;
		self->fd = st_socket(socket);

		ch->ops = &stj_changer_ops;

		stj_changer_sync(ch);

		unsigned int j;
		struct st_value_iterator * iter = st_value_list_get_iterator(drives);
		for (j = 0; j < ch->nb_drives; j++) {
			struct st_value * dr_config = st_value_iterator_get_value(iter, false);
			stj_drive_init(ch->drives + j, dr_config);
		}
		st_value_iterator_free(iter);
	}

	st_value_iterator_free(iter);
}

static int stj_changer_sync(struct st_changer * changer) {
	struct stj_changer * self = changer->data;

	struct st_value * request = st_value_pack("{ss}", "command", "sync");
	st_json_encode_to_fd(request, self->fd, true);
	st_value_free(request);

	struct st_value * response = st_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return 1;

	struct st_value * vch = NULL;
	st_value_unpack(response, "{so}", "changer", &vch);

	st_changer_sync(changer, vch);

	st_value_free(response);

	return 0;
}

__asm__(".symver stj_changer_sync_all_v1, stj_changer_sync_all@@LIBSTONE_JOB_1.2");
int stj_changer_sync_all_v1() {
	int failed = 0;
	unsigned int i;
	for (i = 0; i < stj_nb_changers && failed == 0; i++) {
		struct st_changer * ch = stj_changers + i;
		failed = ch->ops->sync(ch);
	}
	return failed;
}

static int stj_changer_unload(struct st_changer * changer, struct st_drive * from) {
	struct stj_changer * self = changer->data;

	struct st_value * request = st_value_pack("{sss{si}}", "command", "unload", "params", "from", (long int) from->slot->index);
	st_json_encode_to_fd(request, self->fd, true);
	st_value_free(request);

	struct st_value * response = st_json_parse_fd(self->fd, -1);
	if (response == NULL)
		return 1;

	bool ok = false;
	st_value_unpack(response, "{sb}", "status", &ok);
	st_value_free(response);

	return ok ? 0 : 1;
}

