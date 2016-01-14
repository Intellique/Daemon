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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// free
#include <stdlib.h>

#include <libstoriqone/checksum.h>
#include <libstoriqone/json.h>
#include <libstoriqone/value.h>

#include "auth.h"

bool soctl_auth_do_authentification(int fd, const char * password) {
	// step 1
	struct so_value * request = so_value_pack("{siss}", "step", 1, "method", "login");
	so_json_encode_to_fd(request, fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(fd, 10000);
	if (response == NULL)
		return false;

	char * salt = NULL;
	bool error = true;
	so_value_unpack(response, "{sbss}", "error", &error, "salt", &salt);
	so_value_free(response);

	if (salt == NULL || error) {
		free(salt);
		return false;
	}


	// step 2
	char * hash = so_checksum_salt_password("sha1", password, salt);
	free(salt);
	if (hash == NULL)
		return false;

	request = so_value_pack("{sissss}", "step", 2, "method", "login", "password", hash);
	free(hash);

	so_json_encode_to_fd(request, fd, true);
	so_value_free(request);

	response = so_json_parse_fd(fd, 10000);
	if (response == NULL)
		return false;

	so_value_unpack(response, "{sb}", "error", &error);
	so_value_free(response);

	return !error;
}

