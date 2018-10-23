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

// bindtextdomain, gettext, textdomain
#include <libintl.h>
// bool
#include <stdbool.h>
// free
#include <stdlib.h>
// strcmp
#include <string.h>
// getpid, getppid, getsid
#include <unistd.h>

#include <libstoriqone/json.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/value.h>
#include <logger/log.h>

#include "listen.h"

#include "checksum/logger.chcksum"
#include "config.h"
#include "storiqone.version"

static bool stop = false;

static void daemon_request(int fd, short event, void * data);


static void daemon_request(int fd, short event, void * data __attribute__((unused))) {
	switch (event) {
		case POLLHUP:
			solgr_log_write2(so_log_level_alert, so_log_type_logger,
				gettext("Storiqoned has hang up"));
			stop = true;
			break;
	}

	struct so_value * request = so_json_parse_fd(fd, 1000);
	const char * command = NULL;
	if (request == NULL || so_value_unpack(request, "{sS}", "command", &command) < 0) {
		if (request != NULL)
			so_value_free(request);
		return;
	}

	if (!strcmp("stop", command))
		stop = true;

	so_value_free(request);
}

int main() {
	bindtextdomain("storiqone-logger", LOCALE_DIR);
	textdomain("storiqone-logger");

	solgr_log_write2(so_log_level_notice, so_log_type_logger,
		gettext("Starting logger process (pid: %d, ppid: %d, sid: %d)"),
		getpid(), getppid(), getsid(0));
	solgr_log_write2(so_log_level_debug, so_log_type_logger,
		gettext("Checksum: %s, last commit: %s"),
		LOGGER_SRCSUM, STORIQONE_GIT_COMMIT);

	struct so_value * config = so_json_parse_fd(0, 60000);
	if (config == NULL || !so_value_hashtable_has_key2(config, "modules")) {
		solgr_log_write2(so_log_level_emergencey, so_log_type_logger,
			gettext("No configuration received from daemon, will now quit"));
		return 1;
	}

	so_poll_register(0, POLLIN | POLLHUP, daemon_request, NULL, NULL);

	struct so_value * module;
	struct so_value * socket;
	so_value_unpack(config, "{soso}", "modules", &module, "socket", &socket);

	solgr_log_load(module);
	solgr_log_write2(so_log_level_debug, so_log_type_logger,
		gettext("Modules loaded"));

	solgr_listen_configure(socket);

	unsigned int nb_handlers = solgr_listen_nb_clients();
	while (!stop || nb_handlers > 1) {
		so_poll(-1);

		nb_handlers = solgr_listen_nb_clients();
	}

	so_value_free(config);

	solgr_log_write2(so_log_level_notice, so_log_type_logger,
		gettext("Logger process (pid: %d) exiting normally"),
		getpid());

	return 0;
}
