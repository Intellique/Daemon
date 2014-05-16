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

// getopt_long
#include <getopt.h>
// printf
#include <stdio.h>
// waitpid
#include <sys/types.h>
// waitpid
#include <sys/wait.h>
// close, sleep
#include <unistd.h>

#include <libstone/json.h>
#include <libstone/process.h>
#include <libstone/socket.h>
#include <libstone/value.h>

#include "auth.h"
#include "common.h"
#include "config.h"

enum {
	OPT_HELP = 'h',
};

static int stctl_daemon_init_socket(struct st_value * config);
static bool stctl_daemon_try_connect(void);


static int stctl_daemon_init_socket(struct st_value * config) {
	struct st_value * admin = st_value_hashtable_get2(config, "admin", false, false);
	if (admin == NULL || admin->type != st_value_hashtable || !st_value_hashtable_has_key2(admin, "password") || !st_value_hashtable_has_key2(admin, "socket")) {
		st_value_free(config);
		return false;
	}

	struct st_value * socket = st_value_hashtable_get2(admin, "socket", false, false);
	if (socket == NULL || socket->type != st_value_hashtable) {
		st_value_free(config);
		return false;
	}

	return st_socket(socket);
}

static bool stctl_daemon_try_connect() {
	struct st_value * config = st_json_parse_file(DAEMON_CONFIG_FILE);
	if (config == NULL || !st_value_hashtable_has_key2(config, "admin")) {
		if (config != NULL)
			st_value_free(config);
		return false;
	}

	struct st_value * admin = st_value_hashtable_get2(config, "admin", false, false);
	if (admin == NULL || admin->type != st_value_hashtable || !st_value_hashtable_has_key2(admin, "password") || !st_value_hashtable_has_key2(admin, "socket")) {
		st_value_free(config);
		return false;
	}

	int fd = stctl_daemon_init_socket(config);
	if (fd < 0) {
		st_value_free(config);
		return false;
	}

	struct st_value * password = st_value_hashtable_get2(admin, "password", false, false);
	if (password == NULL || password->type != st_value_string) {
		st_value_free(config);
		return false;
	}

	bool ok = stctl_auth_do_authentification(fd, st_value_string_get(password));

	st_value_free(config);
	close(fd);

	return ok;
}

int stctl_start_daemon(int argc, char ** argv) {
	int option_index = 0;
	static struct option long_options[] = {
		{ "help",     0, NULL, OPT_HELP },

		{ NULL, 0, NULL, 0 },
	};

	// parse option
	int opt;
	optind = 0;
	do {
		opt = getopt_long(argc, argv, "h", long_options, &option_index);

		switch (opt) {
			case -1:
				break;

			case OPT_HELP:
				printf("stonectl start : start stoned\n");
				printf("  -h, --help : show this and exit\n");
				return 0;

			default:
				return 1;
		}
	} while (opt > -1);

	if (stctl_daemon_try_connect())
		return 1;

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
			break;

		if (stctl_daemon_try_connect())
			ok = true;
	}

	st_process_free(&daemon, 1);

	return ok ? 0 : 2;
}

int stctl_status_daemon(int argc, char ** argv) {
	int option_index = 0;
	static struct option long_options[] = {
		{ "help",     0, NULL, OPT_HELP },

		{ NULL, 0, NULL, 0 },
	};

	// parse option
	int opt;
	optind = 0;
	do {
		opt = getopt_long(argc, argv, "h", long_options, &option_index);

		switch (opt) {
			case -1:
				break;

			case OPT_HELP:
				printf("stonectl status : get status of stoned\n");
				printf("  -h, --help : show this and exit\n");
				return 0;

			default:
				return 1;
		}
	} while (opt > -1);

	return stctl_daemon_try_connect() ? 0 : 1;
}

int stctl_stop_daemon(int argc, char ** argv) {
	int option_index = 0;
	static struct option long_options[] = {
		{ "help",     0, NULL, OPT_HELP },

		{ NULL, 0, NULL, 0 },
	};

	// parse option
	int opt;
	optind = 0;
	do {
		opt = getopt_long(argc, argv, "h", long_options, &option_index);

		switch (opt) {
			case -1:
				break;

			case OPT_HELP:
				printf("stonectl stop : stop stoned daemon\n");
				printf("  -h, --help : show this and exit\n");
				return 0;

			default:
				return 1;
		}
	} while (opt > -1);

	if (!stctl_daemon_try_connect())
		return 0;

	struct st_value * config = st_json_parse_file(DAEMON_CONFIG_FILE);
	if (config == NULL || !st_value_hashtable_has_key2(config, "admin"))
		return 1;

	struct st_value * admin = st_value_hashtable_get2(config, "admin", false, false);
	if (admin == NULL || admin->type != st_value_hashtable || !st_value_hashtable_has_key2(admin, "password") || !st_value_hashtable_has_key2(admin, "socket")) {
		st_value_free(config);
		return 1;
	}

	int fd = stctl_daemon_init_socket(config);
	if (fd < 0)
		return 2;

	struct st_value * password = st_value_hashtable_get2(admin, "password", false, false);
	if (password == NULL || password->type != st_value_string) {
		st_value_free(config);
		return 2;
	}

	bool ok = stctl_auth_do_authentification(fd, st_value_string_get(password));

	if (!ok) {
		st_value_free(config);
		close(fd);
		return 3;
	}

	struct st_value * request = st_value_pack("{ss}", "method", "shutdown");
	st_json_encode_to_fd(request, fd, true);
	st_value_free(request);

	struct st_value * response = st_json_parse_fd(fd, 10000);
	if (response == NULL || !st_value_hashtable_has_key2(response, "pid")) {
		if (response != NULL)
			st_value_free(response);
		st_value_free(config);
		close(fd);
		return 4;
	}

	struct st_value * error = st_value_hashtable_get2(response, "error", false, false);
	if (error == NULL || error->type != st_value_boolean) {
		st_value_free(response);
		st_value_free(config);
		close(fd);
		return 4;
	}

	struct st_value * pid = st_value_hashtable_get2(response, "pid", false, false);
	if (pid == NULL || pid->type != st_value_integer) {
		st_value_free(response);
		st_value_free(config);
		close(fd);
		return 4;
	}

	pid_t process = st_value_integer_get(pid);

	ok = !st_value_boolean_get(error);

	st_value_free(response);
	st_value_free(config);
	close(fd);

	if (!ok)
		return 5;

	short retry;
	for (retry = 0; retry < 10; retry++) {
		sleep(1);

		int status = 0;
		pid_t ret = waitpid(process, &status, WNOHANG);
		if (ret != 0)
			return 0;
	}

	return 5;
}

