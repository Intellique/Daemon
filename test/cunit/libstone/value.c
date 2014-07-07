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

static void test_libstone_value_hashtable_iter_0(void);
static void test_libstone_value_hashtable_iter_1(void);
static void test_libstone_value_list_slice_0(void);
static void test_libstone_value_pack_0(void);
static void test_libstone_value_ref_cycle_0(void);
static void test_libstone_value_ref_cycle_1(void);
static void test_libstone_value_share_0(void);
static void test_libstone_value_unpack_0(void);

static struct {
	void (*function)(void);
	char * name;
} test_functions[] = {
	{ test_libstone_value_hashtable_iter_0, "libstone: hashtable iter: #0" },
	{ test_libstone_value_hashtable_iter_1, "libstone: hashtable iter: #1" },

    { test_libstone_value_list_slice_0, "libstone: list slice: #0" },

    { test_libstone_value_pack_0, "libstone: value pack: #0" },

    { test_libstone_value_ref_cycle_0, "libstone: value ref cycle: #0" },
    { test_libstone_value_ref_cycle_1, "libstone: value ref cycle: #1" },

    { test_libstone_value_share_0, "libstone: value share: #0" },

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


static void test_libstone_value_hashtable_iter_0() {
	struct st_value * hash = st_value_pack("{}");
	CU_ASSERT_PTR_NOT_NULL_FATAL(hash);

	struct st_value_iterator * iter = st_value_hashtable_get_iterator(hash);
	CU_ASSERT_PTR_NOT_NULL_FATAL(iter);

	unsigned int i = 0;
	while (st_value_iterator_has_next(iter)) {
		struct st_value * key = st_value_iterator_get_key(iter, false, true);
		struct st_value * elt = st_value_iterator_get_value(iter, true);

		st_value_free(key);
		st_value_free(elt);

		i++;
	}

	st_value_iterator_free(iter);
	st_value_free(hash);

	CU_ASSERT_EQUAL(i, 0);
}

static void test_libstone_value_hashtable_iter_1() {
	struct st_value * hash = st_value_pack("{sb}", "barcode", false);
	CU_ASSERT_PTR_NOT_NULL_FATAL(hash);

	struct st_value_iterator * iter = st_value_hashtable_get_iterator(hash);
	CU_ASSERT_PTR_NOT_NULL_FATAL(iter);

	unsigned int i = 0;
	while (st_value_iterator_has_next(iter)) {
		struct st_value * key = st_value_iterator_get_key(iter, false, true);
		struct st_value * elt = st_value_iterator_get_value(iter, true);

		st_value_free(key);
		st_value_free(elt);

		i++;
	}

	st_value_iterator_free(iter);
	st_value_free(hash);

	CU_ASSERT_EQUAL(i, 1);
}

static void test_libstone_value_list_slice_0() {
	struct st_value * list = st_value_pack("[iii]", 1, 2, 3);
	CU_ASSERT_PTR_NOT_NULL_FATAL(list);

	struct st_value * l2 = st_value_list_slice(list, 2);
	CU_ASSERT_PTR_NOT_NULL_FATAL(l2);

	long long i2;
	st_value_unpack(l2, "[i]", &i2);
	CU_ASSERT_EQUAL(i2, 3);

	st_value_free(list);
	st_value_free(l2);
}

static void test_libstone_value_pack_0() {
	struct st_value * pack = st_value_pack("{s[]}", "foo");
	CU_ASSERT_PTR_NOT_NULL_FATAL(pack);

	CU_ASSERT_EQUAL(pack->shared, 1);
	st_value_free(pack);
}

static void test_libstone_value_ref_cycle_0() {
	struct st_value * objA = st_value_pack("{s{}}", "link");
	CU_ASSERT_PTR_NOT_NULL_FATAL(objA);

	struct st_value * objB;
	st_value_unpack(objA, "{so}", "link", &objB);
	CU_ASSERT_PTR_NOT_NULL_FATAL(objB);

	CU_ASSERT_EQUAL(objA->shared, 1);
	CU_ASSERT_EQUAL(objB->shared, 1);

	st_value_hashtable_put2(objB, "ref", objA, false);
	CU_ASSERT_EQUAL(objA->shared, 2);

	st_value_free(objA);
}

static void test_libstone_value_ref_cycle_1() {
	struct st_value * objA = st_value_pack("{s{s{}}}", "parent", "link");
	CU_ASSERT_PTR_NOT_NULL_FATAL(objA);

	struct st_value * objB;
	st_value_unpack(objA, "{so}", "parent", &objB);
	CU_ASSERT_PTR_NOT_NULL_FATAL(objB);

	struct st_value * objC;
	st_value_unpack(objB, "{so}", "link", &objC);
	CU_ASSERT_PTR_NOT_NULL_FATAL(objC);

	CU_ASSERT_EQUAL(objA->shared, 1);
	CU_ASSERT_EQUAL(objB->shared, 1);
	CU_ASSERT_EQUAL(objC->shared, 1);

	st_value_hashtable_put2(objC, "ref", objB, false);
	CU_ASSERT_EQUAL(objB->shared, 2);

	st_value_free(objA);
}

static void test_libstone_value_share_0() {
	struct st_value * val = st_value_new_boolean(true);
	CU_ASSERT_PTR_NOT_NULL_FATAL(val);

	CU_ASSERT_EQUAL(val->shared, 1);

	struct st_value * val_shared = st_value_share(val);
	CU_ASSERT_EQUAL(val->shared, 2);
	CU_ASSERT_EQUAL(val_shared->shared, 2);

	st_value_free(val_shared);
	CU_ASSERT_EQUAL(val->shared, 1);
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

