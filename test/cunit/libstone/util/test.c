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
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Wed, 26 Sep 2012 11:34:22 +0200                         *
\*************************************************************************/

#include "test.h"

// CU_add_suite, CU_cleanup_registry, CU_cleanup_registry
#include <CUnit/CUnit.h>
// printf
#include <stdio.h>
// free
#include <stdlib.h>
// _exit
#include <unistd.h>

#include <stone/util.h>

static void test_libstone_util_check_valid_utf8_0(void);
static void test_libstone_util_check_valid_utf8_1(void);
static void test_libstone_util_check_valid_utf8_2(void);

static void test_libstone_util_string_delete_double_char_0(void);
static void test_libstone_util_string_delete_double_char_1(void);
static void test_libstone_util_string_delete_double_char_2(void);
static void test_libstone_util_string_delete_double_char_3(void);

static void test_libstone_util_fix_invalid_utf8_0(void);
static void test_libstone_util_fix_invalid_utf8_1(void);

static void test_libstone_util_string_trim_0(void);
static void test_libstone_util_string_trim_1(void);
static void test_libstone_util_string_trim_2(void);
static void test_libstone_util_string_trim_3(void);
static void test_libstone_util_string_trim_4(void);
static void test_libstone_util_string_trim_5(void);

static void test_libstone_util_string_rtrim_0(void);
static void test_libstone_util_string_rtrim_1(void);
static void test_libstone_util_string_rtrim_2(void);
static void test_libstone_util_string_rtrim_3(void);

static void test_libstone_util_trunc_path_0(void);
static void test_libstone_util_trunc_path_1(void);
static void test_libstone_util_trunc_path_2(void);
static void test_libstone_util_trunc_path_3(void);
static void test_libstone_util_trunc_path_4(void);

static struct {
	void (*function)(void);
	char * name;
} test_functions[] = {
    { test_libstone_util_check_valid_utf8_0, "libstone: util check valid utf8: good one" },
    { test_libstone_util_check_valid_utf8_1, "libstone: util check valid utf8: bad one" },
    { test_libstone_util_check_valid_utf8_2, "libstone: util check valid utf8: null one" },

	{ test_libstone_util_string_delete_double_char_0, "libstone: util delete double char #0" },
	{ test_libstone_util_string_delete_double_char_1, "libstone: util delete double char #1" },
	{ test_libstone_util_string_delete_double_char_2, "libstone: util delete double char #2" },
	{ test_libstone_util_string_delete_double_char_3, "libstone: util delete double char #3" },

	{ test_libstone_util_fix_invalid_utf8_0, "libstone: util fix invalid utf8: with valid utf8 string" },
	{ test_libstone_util_fix_invalid_utf8_1, "libstone: util fix invalid utf8: with invalid utf8 string" },

	{ test_libstone_util_string_trim_0, "libstone: util trim #0" },
	{ test_libstone_util_string_trim_1, "libstone: util trim #1" },
	{ test_libstone_util_string_trim_2, "libstone: util trim #2" },
	{ test_libstone_util_string_trim_3, "libstone: util trim #3" },
	{ test_libstone_util_string_trim_4, "libstone: util trim #4" },
	{ test_libstone_util_string_trim_5, "libstone: util trim #5" },

	{ test_libstone_util_string_rtrim_0, "libstone: util rtrim #0" },
	{ test_libstone_util_string_rtrim_1, "libstone: util rtrim #1" },
	{ test_libstone_util_string_rtrim_2, "libstone: util rtrim #2" },
	{ test_libstone_util_string_rtrim_3, "libstone: util rtrim #3" },

	{ test_libstone_util_trunc_path_0, "libstone: util trunc path: single file, 0" },
	{ test_libstone_util_trunc_path_1, "libstone: util trunc path: single file, 2" },
	{ test_libstone_util_trunc_path_2, "libstone: util trunc path: path with '/', 0" },
	{ test_libstone_util_trunc_path_3, "libstone: util trunc path: path with '/', 2" },
	{ test_libstone_util_trunc_path_4, "libstone: util trunc path: path with '/', 4" },

	{ 0, 0 },
};


void test_libstone_util_add_suite() {
	CU_pSuite suite = CU_add_suite("libstone: util", 0, 0);
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


void test_libstone_util_check_valid_utf8_0() {
	int ok = st_util_check_valid_utf8("123 €");
	CU_ASSERT_EQUAL(ok, 1);
}

void test_libstone_util_check_valid_utf8_1() {
	const char s0[] = { ' ', 0xBF, 0x23 };
	int ok = st_util_check_valid_utf8(s0);
	CU_ASSERT_EQUAL(ok, 0);

	const char s1[] = { 0xEF, 0x4 };
	ok = st_util_check_valid_utf8(s1);
	CU_ASSERT_EQUAL(ok, 0);
}

void test_libstone_util_check_valid_utf8_2() {
	int ok = st_util_check_valid_utf8(NULL);
	CU_ASSERT_EQUAL(ok, 0);
}


void test_libstone_util_string_delete_double_char_0() {
	char * str_0 = strdup("aab");

	st_util_string_delete_double_char(str_0, '/');
	CU_ASSERT_STRING_EQUAL(str_0, "aab");

	st_util_string_delete_double_char(str_0, 'a');
	CU_ASSERT_STRING_EQUAL(str_0, "ab");

	free(str_0);
}

void test_libstone_util_string_delete_double_char_1() {
	char * str_0 = strdup("aab/");

	st_util_string_delete_double_char(str_0, '/');
	CU_ASSERT_STRING_EQUAL(str_0, "aab/");

	free(str_0);
}

void test_libstone_util_string_delete_double_char_2() {
	char * str_0 = strdup("aab//bba");

	st_util_string_delete_double_char(str_0, '/');
	CU_ASSERT_STRING_EQUAL(str_0, "aab/bba");

	free(str_0);
}

void test_libstone_util_string_delete_double_char_3() {
	char * str_0 = strdup("aab///bba");

	st_util_string_delete_double_char(str_0, '/');
	CU_ASSERT_STRING_EQUAL(str_0, "aab/bba");

	free(str_0);
}


void test_libstone_util_fix_invalid_utf8_0() {
	char * str = strdup("123 €");

	st_util_fix_invalid_utf8(str);

	CU_ASSERT_STRING_EQUAL(str, "123 €");
}

void test_libstone_util_fix_invalid_utf8_1() {
	char str[] = { '1', '2', '3', 0x9F, ' ', 0 };

	st_util_fix_invalid_utf8(str);

	CU_ASSERT_STRING_EQUAL(str, "123 ");
}


void test_libstone_util_string_trim_0() {
	char * str_0 = strdup("abc");

	st_util_string_trim(str_0, ' ');
	CU_ASSERT_STRING_EQUAL(str_0, "abc");

	free(str_0);
}

void test_libstone_util_string_trim_1() {
	char * str_0 = strdup("  abc");

	st_util_string_trim(str_0, ' ');
	CU_ASSERT_STRING_EQUAL(str_0, "abc");

	free(str_0);
}

void test_libstone_util_string_trim_2() {
	char * str_0 = strdup("abc   ");

	st_util_string_trim(str_0, ' ');
	CU_ASSERT_STRING_EQUAL(str_0, "abc");

	free(str_0);
}

void test_libstone_util_string_trim_3() {
	char * str_0 = strdup("   abc   ");

	st_util_string_trim(str_0, ' ');
	CU_ASSERT_STRING_EQUAL(str_0, "abc");

	free(str_0);
}

void test_libstone_util_string_trim_4() {
	char * str_0 = strdup("abc");

	st_util_string_trim(str_0, 'a');
	CU_ASSERT_STRING_EQUAL(str_0, "bc");

	free(str_0);
}

void test_libstone_util_string_trim_5() {
	char * str_0 = strdup("abc");

	st_util_string_trim(str_0, 'd');
	CU_ASSERT_STRING_EQUAL(str_0, "abc");

	free(str_0);
}


void test_libstone_util_string_rtrim_0() {
	char * str_0 = strdup("abc");

	st_util_string_rtrim(str_0, ' ');
	CU_ASSERT_STRING_EQUAL(str_0, "abc");

	free(str_0);
}

void test_libstone_util_string_rtrim_1() {
	char * str_0 = strdup("  abc");

	st_util_string_rtrim(str_0, ' ');
	CU_ASSERT_STRING_EQUAL(str_0, "  abc");

	free(str_0);
}

void test_libstone_util_string_rtrim_2() {
	char * str_0 = strdup("abc   ");

	st_util_string_rtrim(str_0, ' ');
	CU_ASSERT_STRING_EQUAL(str_0, "abc");

	free(str_0);
}

void test_libstone_util_string_rtrim_3() {
	char * str_0 = strdup("abc--");

	st_util_string_rtrim(str_0, '-');
	CU_ASSERT_STRING_EQUAL(str_0, "abc");

	free(str_0);
}


void test_libstone_util_trunc_path_0() {
	char * path = "file";

	char * result = st_util_trunc_path(path, 0);

	CU_ASSERT_STRING_EQUAL(result, "file");
}

void test_libstone_util_trunc_path_1() {
	char * path = "file";

	char * result = st_util_trunc_path(path, 2);

	CU_ASSERT_PTR_NULL(result);
}

void test_libstone_util_trunc_path_2() {
	char * path = "root/foo/bar/file";

	char * result = st_util_trunc_path(path, 0);

	CU_ASSERT_STRING_EQUAL(result, "root/foo/bar/file");
}

void test_libstone_util_trunc_path_3() {
	char * path = "root/foo/bar/file";

	char * result = st_util_trunc_path(path, 2);

	CU_ASSERT_STRING_EQUAL(result, "bar/file");
}

void test_libstone_util_trunc_path_4() {
	char * path = "root/foo/bar/file";

	char * result = st_util_trunc_path(path, 4);

	CU_ASSERT_PTR_NULL(result);
}

