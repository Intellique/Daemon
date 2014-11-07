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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// CU_basic_run_tests
#include <CUnit/Basic.h>
// CU_console_run_tests
#include <CUnit/Console.h>
// CU_curses_run_tests
#include <CUnit/CUCurses.h>
// CU_get_error_msg, CU_initialize_registry
#include <CUnit/CUnit.h>
// getopt_long
#include <getopt.h>
// strcmp
#include <string.h>
// printf
#include <stdio.h>

#include "libstoriqone/test.h"

int main(int argc, char * argv[]) {
	enum {
		OPT_INTERFACE_BASIC   = 'b',
		OPT_INTERFACE_CONSOLE = 'c',
		OPT_INTERFACE_NCURSES = 'C',
		OPT_HELP              = 'h',
		OPT_INTERFACE         = 'i',
	};

	static int option_index = 0;
	static struct option long_options[] = {
		{ "help",      0, 0, OPT_HELP },
		{ "interface", 1, 0, OPT_INTERFACE },

		{0, 0, 0, 0},
	};

	enum interface {
		interface_basic,
		interface_console,
		interface_ncurses,
	} interface = interface_basic;

	struct {
		enum interface interface;
		const char * name;
	} interfaces[] = {
		{ interface_basic, "basic" },
		{ interface_console, "console" },
		{ interface_ncurses, "ncurses" },

		{ 0, 0 },
	};

	int opt;
	do {
		opt = getopt_long(argc, argv, "bcChi:", long_options, &option_index);

		switch (opt) {
			case -1:
				break;

			case OPT_INTERFACE: {
					int i, found = 0;
					for (i = 0; interfaces[i].name && !found; i++) {
						if (!strcmp(interfaces[i].name, optarg)) {
							interface = interfaces[i].interface;
							found = 1;
						}
					}

					if (!found) {
						printf("Interface should be one of ");

						for (i = 0; interfaces[i].name; i++)
							printf("'%s'%s", interfaces[i].name, interfaces[i + 1].name ? ", " : "\n");

						return 1;
					}
				}
				break;

			case OPT_INTERFACE_BASIC:
				interface = interface_basic;
				break;

			case OPT_INTERFACE_CONSOLE:
				interface = interface_console;
				break;

			case OPT_INTERFACE_NCURSES:
				interface = interface_ncurses;
				break;

			case OPT_HELP:
				return 0;

			default:
				return 1;
		}
	} while (opt > -1);

	if (CU_initialize_registry() != CUE_SUCCESS) {
		printf("Failed to initialize CUnit becase %s\n", CU_get_error_msg());
		return 2;
	}

    test_libstoriqone_add_suite();

	switch (interface) {
		case interface_basic:
			CU_basic_run_tests();
			break;

		case interface_console:
			CU_console_run_tests();
			break;

		case interface_ncurses:
			CU_curses_run_tests();
			break;
	}

	CU_cleanup_registry();
	return CU_get_error();
}

