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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

#include "json.h"

// CU_add_suite, CU_cleanup_registry, CU_cleanup_registry
#include <CUnit/CUnit.h>
// printf
#include <stdio.h>
// free
#include <stdlib.h>
// _exit
#include <unistd.h>

#include <libstoriqone/json.h>
#include <libstoriqone/value.h>

static void test_libstoriqone_json_encode_to_string_0(void);
static void test_libstoriqone_json_encode_to_string_1(void);
static void test_libstoriqone_json_encode_to_string_2(void);
static void test_libstoriqone_json_encode_to_string_3(void);
static void test_libstoriqone_json_encode_to_string_4(void);
static void test_libstoriqone_json_parse_string_0(void);
static void test_libstoriqone_json_wtf_0(void);

static struct {
	void (*function)(void);
	char * name;
} test_functions[] = {
	{ test_libstoriqone_json_encode_to_string_0, "libstoriqone: json encode string: #0" },
	{ test_libstoriqone_json_encode_to_string_1, "libstoriqone: json encode string: #1" },
	{ test_libstoriqone_json_encode_to_string_2, "libstoriqone: json encode string: #2" },
	{ test_libstoriqone_json_encode_to_string_3, "libstoriqone: json encode string: #3" },
	{ test_libstoriqone_json_encode_to_string_4, "libstoriqone: json encode string: #4" },

	{ test_libstoriqone_json_parse_string_0, "libstoriqone: json parse string: #0" },

	{ test_libstoriqone_json_wtf_0, "libstoriqone: json wtf: #0" },

	{ 0, 0 },
};


void test_libstoriqone_json_add_suite() {
	CU_pSuite suite = CU_add_suite("libstoriqone: json", 0, 0);
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

static void test_libstoriqone_json_encode_to_string_0() {
	struct so_value * boolean = so_value_new_boolean(true);
	CU_ASSERT_EQUAL(boolean->shared, 1);

	char * string = so_json_encode_to_string(boolean);
	CU_ASSERT_PTR_NOT_NULL_FATAL(string);
	CU_ASSERT_EQUAL(boolean->shared, 1);

	free(string);

	so_value_free(boolean);
}

static void test_libstoriqone_json_encode_to_string_1() {
	struct so_value * list = so_value_pack("[bb]", false, true);
	CU_ASSERT_EQUAL(list->shared, 1);

	char * string = so_json_encode_to_string(list);
	CU_ASSERT_PTR_NOT_NULL_FATAL(string);
	CU_ASSERT_EQUAL(list->shared, 1);

	free(string);

	so_value_free(list);
}

static void test_libstoriqone_json_encode_to_string_2() {
	struct so_value * object = so_value_pack("{sb}", "foo", true);
	CU_ASSERT_EQUAL(object->shared, 1);

	char * string = so_json_encode_to_string(object);
	CU_ASSERT_PTR_NOT_NULL_FATAL(string);
	CU_ASSERT_EQUAL(object->shared, 1);

	free(string);

	so_value_free(object);
}

static void test_libstoriqone_json_encode_to_string_3() {
	struct so_value * object = so_value_pack("{sisisisi}",
		"returned", 1024L,
		"last errno", 0L,
		"position", 0L,
		"available size", 1024L
	);
	CU_ASSERT_EQUAL(object->shared, 1);

	char * string = so_json_encode_to_string(object);
	CU_ASSERT_PTR_NOT_NULL_FATAL(string);
	CU_ASSERT_EQUAL(object->shared, 1);

	free(string);

	so_value_free(object);
}

static void test_libstoriqone_json_encode_to_string_4() {
	struct so_value * object = so_value_pack("{sf}", "float", 0.25);
	CU_ASSERT_EQUAL(object->shared, 1);

	char * string = so_json_encode_to_string(object);
	CU_ASSERT_PTR_NOT_NULL_FATAL(string);
	CU_ASSERT_EQUAL(object->shared, 1);
	so_value_free(object);

	object = so_json_parse_string(string);
	CU_ASSERT_PTR_NOT_NULL_FATAL(object);
	CU_ASSERT_NOT_EQUAL(object->type, so_value_null);

	double val = 0;
	so_value_unpack(object, "{sf}", "float", &val);
	CU_ASSERT_EQUAL(val, 0.25);

	so_value_free(object);
	free(string);
}

static void test_libstoriqone_json_parse_string_0() {
	static const char * json = "{\"density code\":1,\"foo\":\"bar\"}";
	struct so_value * value = so_json_parse_string(json);
	CU_ASSERT_PTR_NOT_NULL_FATAL(value);
	so_value_free(value);
}

static void test_libstoriqone_json_wtf_0() {
	ssize_t nb_total_write = 9130;
	ssize_t available_size = 68205654;

	struct so_value * sent = so_value_pack("{szsisz}",
		"returned", nb_total_write,
		"last errno", 0,
		"available size", available_size
	);
	CU_ASSERT_PTR_NOT_NULL_FATAL(sent);

	char * json = so_json_encode_to_string(sent);
	CU_ASSERT_PTR_NOT_NULL_FATAL(json);

	struct so_value * received = so_json_parse_string(json);
	CU_ASSERT_PTR_NOT_NULL_FATAL(received);

	ssize_t nb_write = -1;
	ssize_t as = -1;

	so_value_unpack(received, "{szsz}",
		"returned", &nb_write,
		"available size", &as
	);

	CU_ASSERT_EQUAL(nb_total_write, nb_write);
	CU_ASSERT_EQUAL(available_size, as);

	so_value_free(sent);
	so_value_free(received);
	free(json);
}

