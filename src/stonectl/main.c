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

// printf
#include <stdio.h>

#include <libstone/value.h>

#include "common.h"

static void stctl_exit(void) __attribute__((destructor));
static void stctl_init(void) __attribute__((constructor));

static struct st_value * commands = NULL;


static void stctl_exit() {
	st_value_free(commands);
	commands = NULL;
}

static void stctl_init() {
	commands = st_value_new_hashtable2();
	st_value_hashtable_put2(commands, "config", st_value_new_custom(stctl_config, NULL), true);
	st_value_hashtable_put2(commands, "start", st_value_new_custom(stctl_start_daemon, NULL), true);
	st_value_hashtable_put2(commands, "status", st_value_new_custom(stctl_status_daemon, NULL), true);
	st_value_hashtable_put2(commands, "stop", st_value_new_custom(stctl_stop_daemon, NULL), true);
}

int main(int argc, char ** argv) {
	if (argc < 1)
		return 1;

	if (!st_value_hashtable_has_key2(commands, argv[1])) {
		printf("Error: command not found (%s)\n", argv[1]);
		return 2;
	}

	struct st_value * com = st_value_hashtable_get2(commands, argv[1], false);
	command_f command = com->value.custom.data;

	return command(argc - 1, argv + 1);
}

