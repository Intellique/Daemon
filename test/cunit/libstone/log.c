/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>        *
\*************************************************************************/

#include "log.h"

// CU_add_suite, CU_cleanup_registry, CU_cleanup_registry
#include <CUnit/CUnit.h>
// printf
#include <stdio.h>
// free
#include <stdlib.h>
// _exit
#include <unistd.h>

#include <libstone/log.h>

static void test_libstone_log_level_to_string_0(void);
static void test_libstone_log_string_to_level_0(void);

static struct {
	void (*function)(void);
	char * name;
} test_functions[] = {
    { test_libstone_log_level_to_string_0, "libstone: log level to string: #0" },

    { test_libstone_log_string_to_level_0, "libstone: log string to level: #0" },

	{ 0, 0 },
};


void test_libstone_log_add_suite() {
	CU_pSuite suite = CU_add_suite("libstone: log", 0, 0);
	if (!suite) {
		CU_cleanup_registry();
		printf("Error while adding suite libstone because %s\n", CU_get_error_msg());
		_exit(3);
	}

	int i;
	for (i = 0; test_functions[i].name; i++) {
		if (!CU_add_test(suite, test_functions[i].name, test_functions[i].function)) {
			CU_cleanup_registry();
			printf("Error while adding test function '%s' libstone because %s\n", test_functions[i].name, CU_get_error_msg());
			_exit(3);
		}
	}
}

static void test_libstone_log_level_to_string_0() {
	const char * lvl = st_log_level_to_string(st_log_level_alert);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Alert");

	lvl = st_log_level_to_string(st_log_level_critical);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Critical");

	lvl = st_log_level_to_string(st_log_level_debug);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Debug");

	lvl = st_log_level_to_string(st_log_level_emergencey);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Emergency");

	lvl = st_log_level_to_string(st_log_level_error);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Error");

	lvl = st_log_level_to_string(st_log_level_info);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Info");

	lvl = st_log_level_to_string(st_log_level_notice);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Notice");

	lvl = st_log_level_to_string(st_log_level_warning);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Warning");

	lvl = st_log_level_to_string(st_log_level_unknown);
	CU_ASSERT_PTR_NOT_NULL_FATAL(lvl);
	CU_ASSERT_STRING_EQUAL(lvl, "Unknown level");
}

static void test_libstone_log_string_to_level_0() {
	enum st_log_level lvl = st_log_string_to_level("Alert");
	CU_ASSERT_EQUAL(lvl, st_log_level_alert);

	lvl = st_log_string_to_level("Critical");
	CU_ASSERT_EQUAL(lvl, st_log_level_critical);

	lvl = st_log_string_to_level("Debug");
	CU_ASSERT_EQUAL(lvl, st_log_level_debug);

	lvl = st_log_string_to_level("Emergency");
	CU_ASSERT_EQUAL(lvl, st_log_level_emergencey);

	lvl = st_log_string_to_level("Error");
	CU_ASSERT_EQUAL(lvl, st_log_level_error);

	lvl = st_log_string_to_level("Info");
	CU_ASSERT_EQUAL(lvl, st_log_level_info);

	lvl = st_log_string_to_level("Notice");
	CU_ASSERT_EQUAL(lvl, st_log_level_notice);

	lvl = st_log_string_to_level("Warning");
	CU_ASSERT_EQUAL(lvl, st_log_level_warning);
}

