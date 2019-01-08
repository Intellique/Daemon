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
*  Copyright (C) 2013-2019, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// bindtextdomain, gettext, textdomain
#include <libintl.h>
// printf
#include <stdio.h>

#include <libstoriqone/value.h>

#include "common.h"
#include "config.h"

static void soctl_exit(void) __attribute__((destructor));
static void soctl_init(void) __attribute__((constructor));

static struct so_value * commands = NULL;


static void soctl_exit() {
	so_value_free(commands);
	commands = NULL;
}

static void soctl_init() {
	commands = so_value_new_hashtable2();
	so_value_hashtable_put2(commands, "api", so_value_new_custom(soctl_api, NULL), true);
	so_value_hashtable_put2(commands, "config", so_value_new_custom(soctl_config, NULL), true);
	so_value_hashtable_put2(commands, "start", so_value_new_custom(soctl_start_daemon, NULL), true);
	so_value_hashtable_put2(commands, "status", so_value_new_custom(soctl_status_daemon, NULL), true);
	so_value_hashtable_put2(commands, "stop", so_value_new_custom(soctl_stop_daemon, NULL), true);

	bindtextdomain("storiqonectl", LOCALE_DIR);
	textdomain("storiqonectl");
}

int main(int argc, char ** argv) {
	if (argc < 2)
		return 1;

	if (!so_value_hashtable_has_key2(commands, argv[1])) {
		printf(gettext("Error: command not found (%s)\n"), argv[1]);
		return 2;
	}

	struct so_value * com = so_value_hashtable_get2(commands, argv[1], false, false);
	command_f command = so_value_custom_get(com);

	return command(argc - 1, argv + 1);
}
