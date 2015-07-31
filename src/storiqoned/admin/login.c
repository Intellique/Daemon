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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// free
#include <stdlib.h>
// strcmp
#include <string.h>

#include <libstoriqone/checksum.h>
#include <libstoriqone/value.h>

#include "common.h"

struct so_value * sod_admin_login(struct sod_admin_client * client, struct so_value * request, struct so_value * config) {
	if (!so_value_hashtable_has_key2(request, "step"))
		return so_value_pack("{sbss}", "error", true, "message", "parameter required");

	struct so_value * step = so_value_hashtable_get2(request, "step", false, false);
	if (step->type != so_value_integer)
		return so_value_pack("{sbss}", "error", true, "message", "parameter step : should be an integer");

	if (client->logged)
		return so_value_pack("{sbss}", "error", false, "message", "already logged");

	switch (so_value_integer_get(step)) {
		case 1: {
				free(client->salt);
				client->salt = so_checksum_gen_salt("sha1", 64);

				return so_value_pack("{sbssss}", "error", false, "message", "do step 2 of loggin process", "salt", client->salt);
			}

		case 2: {
				if (client->salt == NULL)
					return so_value_pack("{sbss}", "error", true, "message", "do step 1 of loggin process");

				struct so_value * vhash = so_value_hashtable_get2(request, "password", false, false);
				if (vhash == NULL || vhash->type != so_value_string)
					return so_value_pack("{sbss}", "error", true, "message", "parameter password : should be a string");

				struct so_value * vpasswd = so_value_hashtable_get2(config, "password", false, false);
				char * hash = so_checksum_salt_password("sha1", so_value_string_get(vpasswd), client->salt);

				if (!strcmp(hash, so_value_string_get(vhash))) {
					free(hash);
					free(client->salt);

					client->salt = NULL;
					client->logged = true;

					return so_value_pack("{sbss}", "error", false, "message", "access granted");
				} else {
					free(hash);
					free(client->salt);

					client->salt = NULL;
					client->logged = false;

					return so_value_pack("{sbss}", "error", true, "message", "access refused");
				}
			}

		default:
			return so_value_pack("{sbss}", "error", true, "message", "parameter step : invalid step value");
	}
}

