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

#include "log.h"

// CU_add_suite, CU_cleanup_registry, CU_cleanup_registry
#include <CUnit/CUnit.h>
// printf
#include <stdio.h>
// free
#include <stdlib.h>
// _exit
#include <unistd.h>

#include <libstoriqone/log.h>

static void test_libstoriqone_log_level_to_string_0(void);
static void test_libstoriqone_log_string_to_level_0(void);

static struct {
	void (*function)(void);
	char * name;
} test_functions[] = {
    { test_libstoriqone_log_level_to_string_0, "libstoriqone: log level to string: #0" },

    { test_libstoriqone_log_string_to_level_0, "libstoriqone: log string to level: #0" },

	{ 0, 0 },
};


void test_libstoriqone_log_add_suite() {
	CU_pSuite suite = CU_add_suite("libstoriqone: log", 0, 0);
	if (!suite) {
		CU_cleanup_registry();
		printf("Error while adding suite libstoriqone because %s\n", CU_get_error_msg());
		_exit(3);
	}

	int i;
	for (i = 0; test_functions[i].name; i++) {
		if (!CU_add_test(suite, test_functions[i].name, test_functions[i].function)) {
			CU_cleanup_registry();
			printf("Error while adding test function '%s' libstoriqone because %s\n", test_functions[i].name, CU_get_error_msg());
			_exit(3);
		}
	}
}

static void test_libstoriqone_log_level_to_string_0() {
	const char * lvl = so_log_level_to_string(so_log_level_alert, false);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Alert");

	lvl = so_log_level_to_string(so_log_level_critical, false);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Critical");

	lvl = so_log_level_to_string(so_log_level_debug, false);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Debug");

	lvl = so_log_level_to_string(so_log_level_emergencey, false);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Emergency");

	lvl = so_log_level_to_string(so_log_level_error, false);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Error");

	lvl = so_log_level_to_string(so_log_level_info, false);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Info");

	lvl = so_log_level_to_string(so_log_level_notice, false);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Notice");

	lvl = so_log_level_to_string(so_log_level_warning, false);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Warning");

	lvl = so_log_level_to_string(so_log_level_unknown, false);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Unknown level");
}

static void test_libstoriqone_log_string_to_level_0() {
	enum so_log_level lvl = so_log_string_to_level("Alert", false);
	CU_ASSERT_EQUAL(lvl, so_log_level_alert);

	lvl = so_log_string_to_level("Critical", false);
	CU_ASSERT_EQUAL(lvl, so_log_level_critical);

	lvl = so_log_string_to_level("Debug", false);
	CU_ASSERT_EQUAL(lvl, so_log_level_debug);

	lvl = so_log_string_to_level("Emergency", false);
	CU_ASSERT_EQUAL(lvl, so_log_level_emergencey);

	lvl = so_log_string_to_level("Error", false);
	CU_ASSERT_EQUAL(lvl, so_log_level_error);

	lvl = so_log_string_to_level("Info", false);
	CU_ASSERT_EQUAL(lvl, so_log_level_info);

	lvl = so_log_string_to_level("Notice", false);
	CU_ASSERT_EQUAL(lvl, so_log_level_notice);

	lvl = so_log_string_to_level("Warning", false);
	CU_ASSERT_EQUAL(lvl, so_log_level_warning);
}

