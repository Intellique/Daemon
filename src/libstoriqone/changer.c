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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// dgettext
#include <libintl.h>
// calloc, free
#include <stdlib.h>

#define gettext_noop(String) String

#include <libstoriqone/changer.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/string.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>

static struct so_changer_action2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_changer_action action;
} so_changer_actions[] = {
	[so_changer_action_none]        = { 0, 0, gettext_noop("none"),        NULL, so_changer_action_none },
	[so_changer_action_put_offline] = { 0, 0, gettext_noop("set offline"), NULL, so_changer_action_put_offline },
	[so_changer_action_put_online]  = { 0, 0, gettext_noop("set online"),  NULL, so_changer_action_put_online },

	[so_changer_action_unknown] = { 0, 0, gettext_noop("unknown action"), NULL, so_changer_action_unknown },
};
static const unsigned int so_changer_nb_actions = sizeof(so_changer_actions) / sizeof(*so_changer_actions);

static struct so_changer_status2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_changer_status status;
} so_changer_status[] = {
	[so_changer_status_error]      = { 0, 0, gettext_noop("error"),      NULL, so_changer_status_error },
	[so_changer_status_exporting]  = { 0, 0, gettext_noop("exporting"),  NULL, so_changer_status_exporting },
	[so_changer_status_idle]       = { 0, 0, gettext_noop("idle"),       NULL, so_changer_status_idle },
	[so_changer_status_go_offline] = { 0, 0, gettext_noop("going offline"), NULL, so_changer_status_go_offline },
	[so_changer_status_go_online]  = { 0, 0, gettext_noop("going online"),  NULL, so_changer_status_go_online },
	[so_changer_status_importing]  = { 0, 0, gettext_noop("importing"),  NULL, so_changer_status_importing },
	[so_changer_status_loading]    = { 0, 0, gettext_noop("loading"),    NULL, so_changer_status_loading },
	[so_changer_status_offline]    = { 0, 0, gettext_noop("offline"),    NULL, so_changer_status_offline },
	[so_changer_status_unloading]  = { 0, 0, gettext_noop("unloading"),  NULL, so_changer_status_unloading },

	[so_changer_status_unknown] = { 0, 0, gettext_noop("unknown status"), NULL, so_changer_status_unknown },
};
static const unsigned int so_changer_nb_status = sizeof(so_changer_status) / sizeof(*so_changer_status);

static void so_changer_init(void) __attribute__((constructor));


const char * so_changer_action_to_string(enum so_changer_action action, bool translate) {
	if (translate)
		return so_changer_actions[action].translation;
	else
		return so_changer_actions[action].name;
}

struct so_value * so_changer_convert(struct so_changer * changer) {
	struct so_value * drives = so_value_new_array(changer->nb_drives);
	unsigned int i;
	for (i = 0; i < changer->nb_drives; i++)
		so_value_list_push(drives, so_drive_convert(changer->drives + i, false), true);

	struct so_value * slots = so_value_new_array(changer->nb_slots);
	for (i = 0; i < changer->nb_slots; i++)
		so_value_list_push(slots, so_slot_convert(changer->slots + i), true);

	return so_value_pack("{sssssssssssbsssbsbsoso}",
		"model", changer->model,
		"vendor", changer->vendor,
		"revision", changer->revision,
		"serial number", changer->serial_number,
		"wwn", changer->wwn,
		"barcode", changer->barcode,

		"status", so_changer_status_to_string(changer->status, false),
		"is online", changer->is_online,
		"enable", changer->enable,

		"drives", drives,
		"slots", slots
	);
}

void so_changer_free(struct so_changer * changer) {
	if (changer == NULL)
		return;

	free(changer->model);
	free(changer->vendor);
	free(changer->revision);
	free(changer->serial_number);
	free(changer->wwn);

	unsigned int i;
	for (i = 0; i < changer->nb_drives; i++)
		so_drive_free(changer->drives + i);
	free(changer->drives);

	for (i = 0; i < changer->nb_slots; i++)
		so_slot_free(changer->slots + i);
	free(changer->slots);

	free(changer->data);
	so_value_free(changer->db_data);
}

void so_changer_free2(void * changer) {
	so_changer_free(changer);
}

static void so_changer_init() {
	unsigned int i;
	for (i = 0; i < so_changer_nb_actions; i++) {
		so_changer_actions[i].hash = so_string_compute_hash2(so_changer_actions[i].name);
		so_changer_actions[i].translation = dgettext("libstoriqone", so_changer_actions[i].name);
		so_changer_actions[i].hash_translated = so_string_compute_hash2(so_changer_actions[i].translation);
	}

	for (i = 0; i < so_changer_nb_status; i++) {
		so_changer_status[i].hash = so_string_compute_hash2(so_changer_status[i].name);
		so_changer_status[i].translation = dgettext("libstoriqone", so_changer_status[i].name);
		so_changer_status[i].hash_translated = so_string_compute_hash2(so_changer_status[i].translation);
	}
}

enum so_changer_action so_changer_string_to_action(const char * action, bool translate) {
	if (action == NULL)
		return so_changer_action_unknown;

	unsigned int i;
	const unsigned long long hash = so_string_compute_hash2(action);

	if (translate) {
		for (i = 0; i < so_changer_nb_actions; i++)
			if (hash == so_changer_actions[i].hash_translated)
				return so_changer_actions[i].action;
	} else {
		for (i = 0; i < so_changer_nb_actions; i++)
			if (hash == so_changer_actions[i].hash)
				return so_changer_actions[i].action;
	}

	return so_changer_action_unknown;
}

const char * so_changer_status_to_string(enum so_changer_status status, bool translate) {
	if (translate)
		return so_changer_status[status].translation;
	else
		return so_changer_status[status].name;
}

enum so_changer_status so_changer_string_to_status(const char * status, bool translate) {
	if (status == NULL)
		return so_changer_status_unknown;

	unsigned int i;
	const unsigned long long hash = so_string_compute_hash2(status);

	if (translate) {
		for (i = 0; i < so_changer_nb_status; i++)
			if (hash == so_changer_status[i].hash_translated)
				return so_changer_status[i].status;
	} else {
		for (i = 0; i < so_changer_nb_status; i++)
			if (hash == so_changer_status[i].hash)
				return so_changer_status[i].status;
	}

	return so_changer_status_unknown;
}

void so_changer_sync(struct so_changer * changer, struct so_value * new_changer) {
	free(changer->model);
	free(changer->vendor);
	free(changer->revision);
	free(changer->serial_number);
	free(changer->wwn);
	changer->model = changer->vendor = changer->revision = changer->serial_number = changer->wwn = NULL;

	const char * status = NULL;
	struct so_value * drives = NULL, * slots = NULL;

	so_value_unpack(new_changer, "{sssssssssssbsSsbsbsoso}",
		"model", &changer->model,
		"vendor", &changer->vendor,
		"revision", &changer->revision,
		"serial number", &changer->serial_number,
		"wwn", &changer->wwn,
		"barcode", &changer->barcode,

		"status", &status,
		"is online", &changer->is_online,
		"enable", &changer->enable,

		"drives", &drives,
		"slots", &slots
	);

	if (status != NULL)
		changer->status = so_changer_string_to_status(status, false);

	unsigned int i;
	struct so_value_iterator * iter = so_value_list_get_iterator(drives);
	for (i = 0; i < changer->nb_drives; i++) {
		struct so_value * drive = so_value_iterator_get_value(iter, false);
		so_drive_sync(changer->drives + i, drive, false);
	}
	so_value_iterator_free(iter);

	if (changer->nb_slots == 0) {
		changer->nb_slots = so_value_list_get_length(slots);
		changer->slots = calloc(changer->nb_slots, sizeof(struct so_slot));

		for (i = 0; i < changer->nb_slots; i++) {
			struct so_slot * sl = changer->slots + i;
			sl->changer = changer;
			sl->index = i;

			if (i < changer->nb_drives) {
				sl->drive = changer->drives + i;
				changer->drives[i].slot = sl;
			}
		}
	}

	iter = so_value_list_get_iterator(slots);
	for (i = 0; i < changer->nb_slots; i++) {
		struct so_value * slot = so_value_iterator_get_value(iter, false);
		so_slot_sync(changer->slots + i, slot);
	}
	so_value_iterator_free(iter);
}

