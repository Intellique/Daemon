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

#include "value.h"

// CU_add_suite, CU_cleanup_registry, CU_cleanup_registry
#include <CUnit/CUnit.h>
// printf
#include <stdio.h>
// free
#include <stdlib.h>
// _exit
#include <unistd.h>

#include <libstone/value.h>

static void test_libstone_value_pack_0(void);
static void test_libstone_value_unpack_0(void);

static struct {
	void (*function)(void);
	char * name;
} test_functions[] = {
    { test_libstone_value_pack_0, "libstone: value pack: #0" },

    { test_libstone_value_unpack_0, "libstone: value unpack: #0" },

	{ 0, 0 },
};

void test_libstone_value_add_suite() {
	CU_pSuite suite = CU_add_suite("libstone: value", 0, 0);
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


static void test_libstone_value_pack_0() {
	struct st_value * pack = st_value_pack("{s[]}", "foo");
	CU_ASSERT_PTR_NOT_NULL_FATAL(pack);

	st_value_free(pack);
}

void test_libstone_value_unpack_0() {
	struct st_value * pack = st_value_pack("{ss}", "foo", "bar");
	CU_ASSERT_PTR_NOT_NULL_FATAL(pack);

	char * value = NULL;
	int ret = st_value_unpack(pack, "{ss}", "foo", &value);

	CU_ASSERT_EQUAL(ret, 1);
	CU_ASSERT_PTR_NOT_NULL(value);
	CU_ASSERT_STRING_EQUAL(value, "bar");

	free(value);
	st_value_free(pack);
}
