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

// gettext
#include <libintl.h>
// NULL
#include <stddef.h>

#define gettext_noop(String) String

#include "script.h"
#include "string.h"

static struct st_script_type2 {
	unsigned long long hash;
	const char * name;
	const enum st_script_type type;
} st_script_types[] = {
	[st_script_type_on_error] = { 0, gettext_noop("on error"), st_script_type_on_error },
	[st_script_type_post_job] = { 0, gettext_noop("post job"), st_script_type_post_job },
	[st_script_type_pre_job]  = { 0, gettext_noop("pre job"),  st_script_type_pre_job },

	[st_script_type_unknown]  = { 0, gettext_noop("unknown"),  st_script_type_unknown },
};

static void st_script_init(void) __attribute__((constructor));


static void st_script_init() {
	unsigned int i;
	for (i = 0; st_script_types[i].type != st_script_type_unknown; i++)
		st_script_types[i].hash = st_string_compute_hash2(st_script_types[i].name);
	st_script_types[i].hash = st_string_compute_hash2(st_script_types[i].name);
}

__asm__(".symver st_script_string_to_type_v1, st_script_string_to_type@@LIBSTONE_1.2");
enum st_script_type st_script_string_to_type_v1(const char * string) {
	if (string == NULL)
		return st_script_type_unknown;

	unsigned int i;
	const unsigned long long hash = st_string_compute_hash2(string);
	for (i = 0; st_script_types[i].type != st_script_type_unknown; i++)
		if (hash == st_script_types[i].hash)
			return st_script_types[i].type;

	return st_script_type_unknown;
}

__asm__(".symver st_script_type_to_string_v1, st_script_type_to_string@@LIBSTONE_1.2");
const char * st_script_type_to_string_v1(enum st_script_type type, bool translate) {
	const char * value = st_script_types[type].name;
	if (translate)
		value = gettext(value);
	return value;
}

