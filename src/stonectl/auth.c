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

#include <stddef.h>

#include <libstone/checksum.h>
#include <libstone/json.h>
#include <libstone/value.h>

#include "auth.h"

bool stctl_auth_do_authentification(int fd, char * password) {
	// step 1
	struct st_value * request = st_value_pack("{siss}", "step", 1, "method", "login");
	st_json_encode_to_fd(request, fd, true);
	st_value_free(request);

	struct st_value * response = st_json_parse_fd(fd, 10000);
	if (response == NULL || !st_value_hashtable_has_key2(response, "salt"))
		return false;

	struct st_value * error = st_value_hashtable_get2(response, "error", false);
	if (error == NULL || error->type != st_value_boolean || error->value.boolean) {
		st_value_free(response);
		return false;
	}

	struct st_value * salt = st_value_hashtable_get2(response, "salt", true);
	st_value_free(response);

	if (salt->type != st_value_string) {
		st_value_free(salt);
		return false;
	}


	// step 2
	char * hash = st_checksum_salt_password("sha1", password, salt->value.string);
	if (hash == NULL) {
		st_value_free(salt);
		return false;
	}

	request = st_value_pack("{sissss}", "step", 2, "method", "login", "password", hash);
	st_json_encode_to_fd(request, fd, true);
	st_value_free(request);


	response = st_json_parse_fd(fd, 10000);
	error = st_value_hashtable_get2(response, "error", false);
	if (error == NULL || error->type != st_value_boolean || error->value.boolean) {
		st_value_free(response);
		return false;
	}

	bool ok = !error->value.boolean;
	st_value_free(response);

	return ok;
}

