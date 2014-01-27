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
*  Last modified: Fri, 24 Jan 2014 13:17:33 +0100                            *
\****************************************************************************/

// json
#include <jansson.h>
// strdup
#include <string.h>

#include <libstone/util/hashtable.h>
#include <libstone/util/json.h>
#include <libstone/util/string.h>
#include <libstone/util/util.h>

#include "../io.h"

struct st_hashtable * st_util_json_from_string(const char * string) {
	if (string == NULL)
		return NULL;

	json_error_t error;
	json_t * obj = json_loads(string, JSON_REJECT_DUPLICATES | JSON_DECODE_ANY, &error);
	if (obj == NULL)
		return NULL;

	struct st_hashtable * values = st_hashtable_new2(st_util_string_compute_hash, st_util_basic_free);

	const char * key;
	json_t * value;
	json_object_foreach(obj, key, value) {
		if (json_is_string(value))
			st_hashtable_put(values, strdup(key), st_hashtable_val_string(strdup(json_string_value(value))));
		else
			st_hashtable_put(values, strdup(key), st_hashtable_val_string(json_dumps(value, JSON_COMPACT | JSON_ENCODE_ANY)));
	}

	json_decref(obj);

	return values;
}

char * st_util_json_archive_to_string(struct st_archive * archive) {
	json_t * jarchive = st_io_json_archive(archive);
	char * str = json_dumps(jarchive, JSON_COMPACT);
	json_decref(jarchive);
	return str;
}

