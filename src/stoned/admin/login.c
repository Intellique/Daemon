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

// free
#include <stdlib.h>
// strcmp
#include <string.h>

#include <libstone/checksum.h>
#include <libstone/value.h>

#include "common.h"

struct st_value * std_admin_login(struct std_admin_client * client, struct st_value * request, struct st_value * config) {
	if (!st_value_hashtable_has_key2(request, "step"))
		return st_value_pack("{sbss}", "error", true, "message", "parameter required");

	struct st_value * step = st_value_hashtable_get2(request, "step", false);
	if (step->type != st_value_integer)
		return st_value_pack("{sbss}", "error", true, "message", "parameter step : should be an integer");

	if (client->logged)
		return st_value_pack("{sbss}", "error", false, "message", "already logged");

	switch (step->value.integer) {
		case 1: {
				free(client->salt);
				client->salt = st_checksum_gen_salt("sha1", 64);

				return st_value_pack("{sbssss}", "error", false, "message", "do step 2 of loggin process", "salt", client->salt);
			}

		case 2: {
				if (client->salt == NULL)
					return st_value_pack("{sbss}", "error", true, "message", "do step 1 of loggin process");

				struct st_value * vhash = st_value_hashtable_get2(request, "password", false);
				if (vhash == NULL || vhash->type != st_value_string)
					return st_value_pack("{sbss}", "error", true, "message", "parameter password : should be a string");

				struct st_value * vpasswd = st_value_hashtable_get2(config, "password", false);
				char * hash = st_checksum_salt_password("sha1", vpasswd->value.string, client->salt);

				if (!strcmp(hash, vhash->value.string)) {
					free(hash);
					free(client->salt);

					client->salt = NULL;
					client->logged = true;

					return st_value_pack("{sbss}", "error", false, "message", "access granted");
				} else {
					free(hash);
					free(client->salt);

					client->salt = NULL;
					client->logged = false;

					return st_value_pack("{sbss}", "error", true, "message", "access refused");
				}
			}

		default:
			return st_value_pack("{sbss}", "error", true, "message", "parameter step : invalid step value");
	}
}

