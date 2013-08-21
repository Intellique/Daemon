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
*  Last modified: Wed, 21 Aug 2013 13:16:55 +0200                            *
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

#include <libstone/database.h>
#include <libstone/library/media.h>
#include <libstone/library/vtl.h>
#include <libstone/log.h>
#include <libstone/util/file.h>
#include <libstone/util/hashtable.h>
#include <libstone/util/string.h>

#include "common.h"
#include "vtl/common.h"


static struct st_hashtable * st_vtl_changers = NULL;

static bool st_vtl_create(struct st_vtl_config * cfg);
static void st_vtl_exit(void) __attribute__((destructor));
static void st_vtl_hash_free(void * key, void * value);
static void st_vtl_init(void) __attribute__((constructor));


static bool st_vtl_create(struct st_vtl_config * cfg) {
	st_log_write_all(st_log_level_info, st_log_type_daemon, "VTL: Create new vtl");

	if (st_util_file_mkdir(cfg->path, 0700)) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "VTL: Failed to create root directory: %s", cfg->path);
		return false;
	}

	char * drive_dir;
	asprintf(&drive_dir, "%s/drives", cfg->path);

	if (access(drive_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(drive_dir, 0700)) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "VTL: Failed to create drives directory: %s", drive_dir);
		free(drive_dir);
		return false;
	}

	unsigned int i;
	for (i = 0; i < cfg->nb_drives; i++) {
		char * dr_dir;
		asprintf(&dr_dir, "%s/%u", drive_dir, i);

		if (access(dr_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(dr_dir, 0700)) {
			st_log_write_all(st_log_level_error, st_log_type_daemon, "VTL: Failed to create drive directory #%u: %s", i, dr_dir);
			free(dr_dir);
			free(drive_dir);
			return false;
		}

		free(dr_dir);
	}

	free(drive_dir);

	char * media_dir;
	asprintf(&media_dir, "%s/medias", cfg->path);

	if (access(media_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(media_dir, 0700)) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "VTL: Failed to create medias directory: %s", media_dir);
		free(media_dir);
		return false;
	}

	for (i = 0; i < cfg->nb_slots; i++) {
		char * md_dir;
		asprintf(&md_dir, "%s/%s%03u", media_dir, cfg->prefix, i);

		if (access(md_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(md_dir, 0700)) {
			st_log_write_all(st_log_level_error, st_log_type_daemon, "VTL: Failed to create media directory #%u: %s", i, md_dir);
			free(md_dir);
			free(media_dir);
			return false;
		}

		free(md_dir);
	}

	free(media_dir);

	// slots
	char * slot_dir;
	asprintf(&slot_dir, "%s/slots", cfg->path);

	if (access(slot_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(slot_dir, 0700)) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "VTL: Failed to create slots directory: %s", slot_dir);
		free(slot_dir);
		return false;
	}

	for (i = 0; i < cfg->nb_slots; i++) {
		char * sl_dir;
		asprintf(&sl_dir, "%s/%u", slot_dir, i);

		if (access(sl_dir, F_OK | R_OK | W_OK | X_OK) && st_util_file_mkdir(sl_dir, 0700)) {
			st_log_write_all(st_log_level_error, st_log_type_daemon, "VTL: Failed to create slot directory #%u: %s", i, sl_dir);
			free(sl_dir);
			free(slot_dir);
			return false;
		}

		free(sl_dir);
	}

	free(slot_dir);

	struct st_changer * ch = st_vtl_changer_init(cfg);
	if (ch == NULL)
		return false;

	st_changer_add(ch);
	st_hashtable_put(st_vtl_changers, strdup(cfg->path), st_hashtable_val_custom(ch));

	return true;
}

static void st_vtl_exit() {
	st_hashtable_free(st_vtl_changers);
	st_vtl_changers = NULL;
}

static void st_vtl_hash_free(void * key, void * value __attribute__((unused))) {
	free(key);
}

static void st_vtl_init() {
	st_vtl_changers = st_hashtable_new2(st_util_string_compute_hash, st_vtl_hash_free);
}

void st_vtl_sync(struct st_database_connection * connect) {
	unsigned int nb_vtls = 0;
	struct st_vtl_config * configs = connect->ops->get_vtls(connect, &nb_vtls);

	if (configs == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Failed to get vtls configurations");
		return;
	}

	unsigned int i;
	for (i = 0; i < nb_vtls; i++) {
		struct st_vtl_config * cfg = configs + i;

		bool exists = !access(cfg->path, F_OK | R_OK | W_OK | X_OK);

		/**
		 * Note: if !exists && cfg->deleted means that the vtl is already deleted
		 * and should not be used anymore
		 */
		if (!exists && !cfg->deleted) {
			bool ok = st_vtl_create(cfg);
			if (!ok)
				st_log_write_all(st_log_level_error, st_log_type_daemon, "Failed to create vtl { path: %s, prefix: %s }", cfg->path, cfg->prefix);
		} else if (exists && !cfg->deleted) {
			// check for non-running vtl & if vtl need update
			if (st_hashtable_has_key(st_vtl_changers, cfg->path)) {
				struct st_hashtable_value chv = st_hashtable_get(st_vtl_changers, cfg->path);
				struct st_changer * ch = chv.value.custom;

				st_vtl_changer_sync(ch, cfg);
			} else {
				struct st_changer * ch = st_vtl_changer_init(cfg);

				if (ch != NULL) {
					st_changer_add(ch);
					st_hashtable_put(st_vtl_changers, strdup(cfg->path), st_hashtable_val_custom(ch));
				} else {
					st_log_write_all(st_log_level_error, st_log_type_daemon, "Failed to restart vtl { path: %s, prefix: %s }", cfg->path, cfg->prefix);
				}
			}
		} else if (exists && cfg->deleted) {
			// remove and delete vtl
			if (st_hashtable_has_key(st_vtl_changers, cfg->path)) {
				// remove the vtl
				struct st_hashtable_value chv = st_hashtable_get(st_vtl_changers, cfg->path);
				struct st_changer * ch = chv.value.custom;

				st_hashtable_remove(st_vtl_changers, cfg->path);

				st_changer_remove(ch);
			}

			connect->ops->delete_vtl(connect, cfg);

			st_util_file_rm(cfg->path);
		}
	}

	st_vtl_config_free(configs, nb_vtls);
}

