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

// waitpid
#include <sys/types.h>
// waitpid
#include <sys/wait.h>
// sleep
#include <unistd.h>

#include <libstone/json.h>
#include <libstone/process.h>
#include <libstone/socket.h>
#include <libstone/value.h>

#include "auth.h"
#include "common.h"
#include "config.h"

static bool stctl_daemon_try_connect(void);


static bool stctl_daemon_try_connect() {
	struct st_value * config = st_json_parse_file(DAEMON_CONFIG_FILE);
	if (config == NULL || !st_value_hashtable_has_key2(config, "admin"))
		return 1;

	struct st_value * admin = st_value_hashtable_get2(config, "admin", false);
	if (admin == NULL || admin->type != st_value_hashtable || !st_value_hashtable_has_key2(admin, "password") || !st_value_hashtable_has_key2(admin, "socket")) {
		st_value_free(config);
		return 1;
	}

	struct st_value * password = st_value_hashtable_get2(admin, "password", false);
	if (password == NULL || password->type != st_value_string) {
		st_value_free(config);
		return 1;
	}

	struct st_value * socket = st_value_hashtable_get2(admin, "socket", false);
	if (socket == NULL || socket->type != st_value_hashtable) {
		st_value_free(config);
		return 1;
	}

	int fd = st_socket(socket);

	if (fd < 0)
		return 2;

	bool ok = stctl_auth_do_authentification(fd, password->value.string);

	st_value_free(config);

	return ok;
}

int stctl_start_daemon(int argc __attribute__((unused)), char ** argv __attribute__((unused))) {
	struct st_process daemon;
	st_process_new(&daemon, "./bin/stoned", NULL, 0);
	st_process_start(&daemon, 1);

	bool ok = false;
	short retry;
	for (retry = 0; retry < 10 && !ok; retry++) {
		sleep(1);

		int status = 0;
		pid_t ret = waitpid(daemon.pid, &status, WNOHANG);
		if (ret != 0)
			return 3;

		if (stctl_daemon_try_connect())
			ok = true;
	}

	return ok ? 0 : 3;
}

int stctl_status_daemon(int argc __attribute__((unused)), char ** argv __attribute__((unused))) {
	return stctl_daemon_try_connect() ? 0 : 3;
}

