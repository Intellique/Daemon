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
*  Last modified: Mon, 04 Feb 2013 19:23:11 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// asprintf, sscanf
#include <stdio.h>
// free
#include <stdlib.h>
// string
#include <string.h>
// access
#include <unistd.h>

#include <libstone/library/media.h>
#include <libstone/util/file.h>
#include <libstone/util/hashtable.h>

#include "common.h"
#include "vtl/common.h"


void st_vtl_add(const struct st_hashtable * params) {
	char * path = st_hashtable_get(params, "path").value.string;
	char * nb_drives = st_hashtable_get(params, "nb_drives").value.string;
	char * prefix = st_hashtable_get(params, "prefix_slot").value.string;
	char * nb_slots = st_hashtable_get(params, "nb_slots").value.string;
	char * media_format = st_hashtable_get(params, "media_format").value.string;

	if (path == NULL || nb_drives == NULL || prefix == NULL || nb_slots == NULL || media_format == NULL)
		return;

	unsigned int nb_drs = 0, nb_slts = 0;
	if (sscanf(nb_drives, "%u", &nb_drs) < 1) {
		return;
	}

	if (sscanf(nb_slots, "%u", &nb_slts) < 1) {
		return;
	}

	if (access(path, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(path, 0700)) {
		return;
	}

	// drives
	char * drive_dir;
	asprintf(&drive_dir, "%s/drives", path);

	if (access(drive_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(drive_dir, 0700)) {
		free(drive_dir);
		return;
	}

	unsigned int i;
	for (i = 0; i < nb_drs; i++) {
		char * dr_dir;
		asprintf(&dr_dir, "%s/%u", drive_dir, i);

		if (access(dr_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(dr_dir, 0700))
			return;

		free(dr_dir);
	}

	free(drive_dir);

	// media format
	struct st_media_format * format = st_media_format_get_by_name(media_format, st_media_format_mode_linear);

	// media
	char * media_dir;
	asprintf(&media_dir, "%s/medias", path);

	if (access(media_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(media_dir, 0700)) {
		free(media_dir);
		return;
	}

	for (i = 0; i < nb_slts; i++) {
		char * md_dir;
		asprintf(&md_dir, "%s/%s%03u", media_dir, prefix, i);

		if (access(md_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(md_dir, 0700))
			return;

		free(md_dir);
	}

	free(media_dir);

	// slots
	char * slot_dir;
	asprintf(&slot_dir, "%s/slots", path);

	if (access(slot_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(slot_dir, 0700)) {
		free(slot_dir);
		return;
	}

	for (i = 0; i < nb_slts; i++) {
		char * sl_dir;
		asprintf(&sl_dir, "%s/%u", slot_dir, i);

		if (access(sl_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(sl_dir, 0700))
			return;

		free(sl_dir);
	}

	free(slot_dir);

	struct st_changer * ch = st_vtl_changer_init(nb_drs, nb_slts, path, prefix, format);
	if (ch != NULL)
		st_changer_add(ch);
}

