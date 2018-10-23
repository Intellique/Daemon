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
*  Copyright (C) 2013-2018, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// getopt_long
#include <getopt.h>
// gettext
#include <libintl.h>
// printf
#include <stdio.h>
// bool
#include <stdbool.h>
// waitpid
#include <sys/types.h>
// waitpid
#include <sys/wait.h>
// close, sleep
#include <unistd.h>

#include <libstoriqone/json.h>
#include <libstoriqone/process.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/value.h>

#include "auth.h"
#include "common.h"
#include "config.h"

enum {
	OPT_HELP    = 'h',
	OPT_VERBOSE = 'v',
};

static int soctl_daemon_init_socket(struct so_value * config);
static bool soctl_daemon_try_connect(void);


static int soctl_daemon_init_socket(struct so_value * config) {
	struct so_value * admin = so_value_hashtable_get2(config, "admin", false, false);
	if (admin == NULL || admin->type != so_value_hashtable || !so_value_hashtable_has_key2(admin, "password") || !so_value_hashtable_has_key2(admin, "socket")) {
		so_value_free(config);
		return false;
	}

	struct so_value * socket = so_value_hashtable_get2(admin, "socket", false, false);
	if (socket == NULL || socket->type != so_value_hashtable) {
		so_value_free(config);
		return false;
	}

	return so_socket(socket);
}

static bool soctl_daemon_try_connect() {
	struct so_value * config = so_json_parse_file(DAEMON_CONFIG_FILE);
	if (config == NULL || !so_value_hashtable_has_key2(config, "admin")) {
		if (config != NULL)
			so_value_free(config);
		return false;
	}

	struct so_value * admin = so_value_hashtable_get2(config, "admin", false, false);
	if (admin == NULL || admin->type != so_value_hashtable || !so_value_hashtable_has_key2(admin, "password") || !so_value_hashtable_has_key2(admin, "socket")) {
		so_value_free(config);
		return false;
	}

	int fd = soctl_daemon_init_socket(config);
	if (fd < 0) {
		so_value_free(config);
		return false;
	}

	struct so_value * password = so_value_hashtable_get2(admin, "password", false, false);
	if (password == NULL || password->type != so_value_string) {
		so_value_free(config);
		return false;
	}

	bool ok = soctl_auth_do_authentification(fd, so_value_string_get(password));

	so_value_free(config);
	close(fd);

	return ok;
}

int soctl_start_daemon(int argc, char ** argv) {
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
				printf(gettext("storiqonectl start : start storiqoned\n"));
				printf(gettext("  -h, --help    : show this and exit\n"));
				return 0;

			default:
				return 1;
		}
	} while (opt > -1);

	if (soctl_daemon_try_connect())
		return 1;

	struct so_process daemon;
	so_process_new(&daemon, "storiqoned", NULL, 0);
	so_process_start(&daemon, 1);

	bool ok = false;
	short retry;
	for (retry = 0; retry < 10 && !ok; retry++) {
		sleep(1);

		int status = 0;
		pid_t ret = waitpid(daemon.pid, &status, WNOHANG);
		if (ret != 0)
			break;

		if (soctl_daemon_try_connect())
			ok = true;
	}

	so_process_free(&daemon, 1);

	return ok ? 0 : 2;
}

int soctl_status_daemon(int argc, char ** argv) {
	int option_index = 0;
	static struct option long_options[] = {
		{ "help",     0, NULL, OPT_HELP },
		{ "verbose",  0, NULL, OPT_VERBOSE },

		{ NULL, 0, NULL, 0 },
	};

	// parse option
	int opt;
	bool verbose = false;
	optind = 0;
	do {
		opt = getopt_long(argc, argv, "h", long_options, &option_index);

		switch (opt) {
			case -1:
				break;

			case OPT_HELP:
				printf(gettext("storiqonectl status : get status of storiqoned\n"));
				printf(gettext("  -h, --help    : show this and exit\n"));
				printf(gettext("  -v, --verbose : increase verbosity\n"));
				return 0;

			case OPT_VERBOSE:
				verbose = true;
				break;

			default:
				return 1;
		}
	} while (opt > -1);

	if (soctl_daemon_try_connect()) {
		if (verbose)
			printf("%s%s", gettext("Status of daemon: "), gettext("alive"));
		return 0;
	} else {
		if (verbose)
			printf("%s%s", gettext("Status of daemon: "), gettext("unknown"));
		return 1;
	}
}

int soctl_stop_daemon(int argc, char ** argv) {
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
				printf(gettext("storiqonectl stop : stop storiqoned daemon\n"));
				printf(gettext("  -h, --help    : show this and exit\n"));
				return 0;

			default:
				return 1;
		}
	} while (opt > -1);

	if (!soctl_daemon_try_connect())
		return 0;

	struct so_value * config = so_json_parse_file(DAEMON_CONFIG_FILE);
	if (config == NULL || !so_value_hashtable_has_key2(config, "admin"))
		return 1;

	struct so_value * admin = so_value_hashtable_get2(config, "admin", false, false);
	if (admin == NULL || admin->type != so_value_hashtable || !so_value_hashtable_has_key2(admin, "password") || !so_value_hashtable_has_key2(admin, "socket")) {
		so_value_free(config);
		return 1;
	}

	int fd = soctl_daemon_init_socket(config);
	if (fd < 0)
		return 2;

	struct so_value * password = so_value_hashtable_get2(admin, "password", false, false);
	if (password == NULL || password->type != so_value_string) {
		so_value_free(config);
		return 2;
	}

	bool ok = soctl_auth_do_authentification(fd, so_value_string_get(password));

	if (!ok) {
		so_value_free(config);
		close(fd);
		return 3;
	}

	struct so_value * request = so_value_pack("{ss}", "method", "shutdown");
	so_json_encode_to_fd(request, fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(fd, 10000);
	if (response == NULL || !so_value_hashtable_has_key2(response, "pid")) {
		if (response != NULL)
			so_value_free(response);
		so_value_free(config);
		close(fd);
		return 4;
	}

	struct so_value * error = so_value_hashtable_get2(response, "error", false, false);
	if (error == NULL || error->type != so_value_boolean) {
		so_value_free(response);
		so_value_free(config);
		close(fd);
		return 4;
	}

	struct so_value * pid = so_value_hashtable_get2(response, "pid", false, false);
	if (pid == NULL || pid->type != so_value_integer) {
		so_value_free(response);
		so_value_free(config);
		close(fd);
		return 4;
	}

	pid_t process = so_value_integer_get(pid);

	ok = !so_value_boolean_get(error);

	so_value_free(response);
	so_value_free(config);
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
