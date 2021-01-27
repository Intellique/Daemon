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

// dgettext, gettext
#include <libintl.h>
// NULL
#include <stddef.h>

#define gettext_noop(String) String

#include <libstoriqone/script.h>
#include <libstoriqone/string.h>

static struct so_script_type2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_script_type type;
} so_script_types[] = {
	[so_script_type_on_error] = { 0, 0, gettext_noop("on error"), NULL, so_script_type_on_error },
	[so_script_type_post_job] = { 0, 0, gettext_noop("post job"), NULL, so_script_type_post_job },
	[so_script_type_pre_job]  = { 0, 0, gettext_noop("pre job"),  NULL, so_script_type_pre_job },

	[so_script_type_unknown]  = { 0, 0, gettext_noop("unknown"),  NULL, so_script_type_unknown },
};
static const unsigned int so_script_nb_types = sizeof(so_script_types) / sizeof(*so_script_types);

static void so_script_init(void) __attribute__((constructor));


static void so_script_init() {
	unsigned int i;
	for (i = 0; i < so_script_nb_types; i++) {
		so_script_types[i].hash = so_string_compute_hash2(so_script_types[i].name);
		so_script_types[i].translation = dgettext("libstoriqone", so_script_types[i].name);
		so_script_types[i].hash_translated = so_string_compute_hash2(so_script_types[i].translation);
	}
}

enum so_script_type so_script_string_to_type(const char * string, bool translate) {
	if (string == NULL)
		return so_script_type_unknown;

	unsigned int i;
	const unsigned long long hash = so_string_compute_hash2(string);

	if (translate) {
		for (i = 0; i < so_script_nb_types; i++)
			if (hash == so_script_types[i].hash_translated)
				return so_script_types[i].type;
	} else {
		for (i = 0; i < so_script_nb_types; i++)
			if (hash == so_script_types[i].hash)
				return so_script_types[i].type;
	}

	return so_script_type_unknown;
}

const char * so_script_type_to_string(enum so_script_type type, bool translate) {
	if (translate)
		return so_script_types[type].translation;
	else
		return so_script_types[type].name;
}

