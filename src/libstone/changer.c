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

// calloc, free
#include <stdlib.h>

#include "changer.h"
#include "drive.h"
#include "slot.h"
#include "string.h"
#include "value.h"

static struct st_changer_action2 {
	const char * name;
	const enum st_changer_action action;
	unsigned long long hash;
} st_library_actions[] = {
	{ "none",        st_changer_action_none,        0 },
	{ "put offline", st_changer_action_put_offline, 0 },
	{ "put online",  st_changer_action_put_online,  0 },

	{ "unknown", st_changer_action_unknown, 0 },
};

static struct st_changer_status2 {
	const char * name;
	const enum st_changer_status status;
	unsigned long long hash;
} st_library_status[] = {
	{ "error",		st_changer_status_error,      0 },
	{ "exporting",	st_changer_status_exporting,  0 },
	{ "idle",		st_changer_status_idle,       0 },
	{ "go offline", st_changer_status_go_offline, 0 },
	{ "go online",  st_changer_status_go_online,  0 },
	{ "importing",	st_changer_status_importing,  0 },
	{ "loading",	st_changer_status_loading,    0 },
	{ "offline",    st_changer_status_offline,    0 },
	{ "unloading",	st_changer_status_unloading,  0 },

	{ "unknown", st_changer_status_unknown, 0 },
};

static void st_changer_init(void) __attribute__((constructor));


__asm__(".symver st_changer_action_to_string_v1, st_changer_action_to_string@@LIBSTONE_1.2");
const char * st_changer_action_to_string_v1(enum st_changer_action action) {
	unsigned int i;
	for (i = 0; st_library_actions[i].action != st_changer_action_unknown; i++)
		if (st_library_actions[i].action == action)
			return st_library_actions[i].name;

	return st_library_actions[i].name;
}

__asm__(".symver st_changer_convert_v1, st_changer_convert@@LIBSTONE_1.2");
struct st_value * st_changer_convert_v1(struct st_changer_v1 * changer) {
	struct st_value * drives = st_value_new_array_v1(changer->nb_drives);
	unsigned int i;
	for (i = 0; i < changer->nb_drives; i++)
		st_value_list_push_v1(drives, st_drive_convert_v1(changer->drives + i, false), true);

	struct st_value * slots = st_value_new_array_v1(changer->nb_slots);
	for (i = 0; i < changer->nb_slots; i++)
		st_value_list_push_v1(slots, st_slot_convert_v1(changer->slots + i), true);

	return st_value_pack_v1("{sssssssssssbsssbsbsoso}",
		"model", changer->model,
		"vendor", changer->vendor,
		"revision", changer->revision,
		"serial number", changer->serial_number,
		"wwn", changer->wwn,
		"barcode", changer->barcode,

		"status", st_changer_status_to_string_v1(changer->status),
		"is online", changer->is_online,
		"enable", changer->enable,

		"drives", drives,
		"slots", slots
	);
}

__asm__(".symver st_changer_free_v1, st_changer_free@@LIBSTONE_1.2");
void st_changer_free_v1(struct st_changer_v1 * changer) {
	if (changer == NULL)
		return;

	free(changer->model);
	free(changer->vendor);
	free(changer->revision);
	free(changer->serial_number);
	free(changer->wwn);

	unsigned int i;
	for (i = 0; i < changer->nb_drives; i++)
		st_drive_free_v1(changer->drives + i);
	free(changer->drives);

	for (i = 0; i < changer->nb_slots; i++)
		st_slot_free_v1(changer->slots + i);
	free(changer->slots);

	free(changer->data);
	st_value_free(changer->db_data);
}

__asm__(".symver st_changer_free2_v1, st_changer_free2@@LIBSTONE_1.2");
void st_changer_free2_v1(void * changer) {
	st_changer_free_v1(changer);
}

static void st_changer_init() {
	int i;
	for (i = 0; st_library_actions[i].action != st_changer_action_unknown; i++)
		st_library_actions[i].hash = st_string_compute_hash2(st_library_actions[i].name);
	st_library_actions[i].hash = st_string_compute_hash2(st_library_actions[i].name);

	for (i = 0; st_library_status[i].status != st_changer_status_unknown; i++)
		st_library_status[i].hash = st_string_compute_hash2(st_library_status[i].name);
	st_library_status[i].hash = st_string_compute_hash2(st_library_status[i].name);
}

__asm__(".symver st_changer_string_to_action_v1, st_changer_string_to_action@@LIBSTONE_1.2");
enum st_changer_action st_changer_string_to_action_v1(const char * action) {
	if (action == NULL)
		return st_changer_action_unknown;

	unsigned int i;
	const unsigned long long hash = st_string_compute_hash2(action);
	for (i = 0; st_library_actions[i].action != st_changer_action_unknown; i++)
		if (hash == st_library_actions[i].hash)
			return st_library_actions[i].action;

	return st_library_actions[i].action;
}

__asm__(".symver st_changer_status_to_string_v1, st_changer_status_to_string@@LIBSTONE_1.2");
const char * st_changer_status_to_string_v1(enum st_changer_status status) {
	unsigned int i;
	for (i = 0; st_library_status[i].status != st_changer_status_unknown; i++)
		if (st_library_status[i].status == status)
			return st_library_status[i].name;

	return st_library_status[i].name;
}

__asm__(".symver st_changer_string_to_status_v1, st_changer_string_to_status@@LIBSTONE_1.2");
enum st_changer_status st_changer_string_to_status_v1(const char * status) {
	if (status == NULL)
		return st_changer_status_unknown;

	unsigned int i;
	const unsigned long long hash = st_string_compute_hash2(status);
	for (i = 0; st_library_status[i].status != st_changer_status_unknown; i++)
		if (hash == st_library_status[i].hash)
			return st_library_status[i].status;

	return st_library_status[i].status;
}

__asm__(".symver st_changer_sync_v1, st_changer_sync@@LIBSTONE_1.2");
void st_changer_sync_v1(struct st_changer_v1 * changer, struct st_value * new_changer) {
	free(changer->model);
	free(changer->vendor);
	free(changer->revision);
	free(changer->serial_number);
	free(changer->wwn);
	changer->model = changer->vendor = changer->revision = changer->serial_number = changer->wwn = NULL;

	char * status = NULL;
	struct st_value * drives = NULL, * slots = NULL;

	st_value_unpack_v1(new_changer, "{sssssssssssbsssbsbsoso}",
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
		changer->status = st_changer_string_to_status_v1(status);
	free(status);

	unsigned int i;
	struct st_value_iterator * iter = st_value_list_get_iterator_v1(drives);
	for (i = 0; i < changer->nb_drives; i++) {
		struct st_value * drive = st_value_iterator_get_value_v1(iter, false);
		st_drive_sync_v1(changer->drives + i, drive, false);
	}
	st_value_iterator_free_v1(iter);

	if (changer->nb_slots == 0) {
		changer->nb_slots = st_value_list_get_length(slots);
		changer->slots = calloc(changer->nb_slots, sizeof(struct st_slot));

		for (i = 0; i < changer->nb_slots; i++) {
			struct st_slot * sl = changer->slots + i;
			sl->changer = changer;
			sl->index = i;

			if (i < changer->nb_drives) {
				sl->drive = changer->drives + i;
				changer->drives[i].slot = sl;
			}
		}
	}

	iter = st_value_list_get_iterator_v1(slots);
	for (i = 0; i < changer->nb_slots; i++) {
		struct st_value * slot = st_value_iterator_get_value_v1(iter, false);
		st_slot_sync_v1(changer->slots + i, slot);
	}
	st_value_iterator_free_v1(iter);
}

