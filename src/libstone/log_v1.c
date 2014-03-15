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

// strcasecmp
#include <string.h>

#include "log_v1.h"

static struct st_log_level2 {
	enum st_log_level level;
	const char * name;
} st_log_levels[] = {
	{ st_log_level_alert,      "Alert" },
	{ st_log_level_critical,   "Critical" },
	{ st_log_level_debug,      "Debug" },
	{ st_log_level_emergencey, "Emergency" },
	{ st_log_level_error,      "Error" },
	{ st_log_level_info,       "Info" },
	{ st_log_level_notice,     "Notice" },
	{ st_log_level_warning,    "Warning" },

	{ st_log_level_unknown, "Unknown level" },
};

static struct st_log_type2 {
	enum st_log_type type;
	const char * name;
} st_log_types[] = {
	{ st_log_type_changer,         "Changer" },
	{ st_log_type_daemon,          "Daemon" },
	{ st_log_type_drive,           "Drive" },
	{ st_log_type_job,             "Job" },
	{ st_log_type_plugin_checksum, "Plugin Checksum" },
	{ st_log_type_plugin_db,       "Plugin Database" },
	{ st_log_type_plugin_log,      "Plugin Log" },
	{ st_log_type_scheduler,       "Scheduler" },

	{ st_log_type_ui,              "User Interface" },
	{ st_log_type_user_message,    "User Message" },

	{ st_log_type_unknown, "Unknown type" },
};


__asm__(".symver st_log_level_to_string_v1, st_log_level_to_string@@LIBSTONE_1.0");
const char * st_log_level_to_string_v1(enum st_log_level level) {
	unsigned int i;
	for (i = 0; st_log_levels[i].level != st_log_level_unknown; i++)
		if (st_log_levels[i].level == level)
			return st_log_levels[i].name;

	return st_log_levels[i].name;
}

__asm__(".symver st_log_string_to_level_v1, st_log_string_to_level@@LIBSTONE_1.0");
enum st_log_level st_log_string_to_level_v1(const char * level) {
	if (level == NULL)
		return st_log_level_unknown;

	unsigned int i;
	for (i = 0; st_log_levels[i].level != st_log_level_unknown; i++)
		if (!strcasecmp(st_log_levels[i].name, level))
			return st_log_levels[i].level;

	return st_log_levels[i].level;
}

__asm__(".symver st_log_string_to_type_v1, st_log_string_to_type@@LIBSTONE_1.0");
enum st_log_type st_log_string_to_type_v1(const char * type) {
	if (type == NULL)
		return st_log_type_unknown;

	unsigned int i;
	for (i = 0; st_log_types[i].type != st_log_type_unknown; i++)
		if (!strcasecmp(type, st_log_types[i].name))
			return st_log_types[i].type;

	return st_log_types[i].type;
}

__asm__(".symver st_log_type_to_string_v1, st_log_type_to_string@@LIBSTONE_1.0");
const char * st_log_type_to_string_v1(enum st_log_type type) {
	unsigned int i;
	for (i = 0; st_log_types[i].type != st_log_type_unknown; i++)
		if (st_log_types[i].type == type)
			return st_log_types[i].name;

	return st_log_types[i].name;
}

