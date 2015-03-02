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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

#include "value.h"

// CU_add_suite, CU_cleanup_registry, CU_cleanup_registry
#include <CUnit/CUnit.h>
// printf
#include <stdio.h>
// free
#include <stdlib.h>
// _exit
#include <unistd.h>

#include <libstoriqone/value.h>

static void test_libstoriqone_value_hashtable_iter_0(void);
static void test_libstoriqone_value_hashtable_iter_1(void);
static void test_libstoriqone_value_hashtable_put_0(void);
static void test_libstoriqone_value_list_slice_0(void);
static void test_libstoriqone_value_pack_0(void);
static void test_libstoriqone_value_ref_cycle_0(void);
static void test_libstoriqone_value_ref_cycle_1(void);
static void test_libstoriqone_value_share_0(void);
static void test_libstoriqone_value_unpack_0(void);
static void test_libstoriqone_value_unpack_1(void);

static struct {
	void (*function)(void);
	char * name;
} test_functions[] = {
	{ test_libstoriqone_value_hashtable_iter_0, "libstoriqone: hashtable iter: #0" },
	{ test_libstoriqone_value_hashtable_iter_1, "libstoriqone: hashtable iter: #1" },

	{ test_libstoriqone_value_hashtable_put_0, "libstoriqone: hashtable put: #0" },

	{ test_libstoriqone_value_list_slice_0, "libstoriqone: list slice: #0" },

	{ test_libstoriqone_value_pack_0, "libstoriqone: value pack: #0" },

	{ test_libstoriqone_value_ref_cycle_0, "libstoriqone: value ref cycle: #0" },
	{ test_libstoriqone_value_ref_cycle_1, "libstoriqone: value ref cycle: #1" },

	{ test_libstoriqone_value_share_0, "libstoriqone: value share: #0" },

	{ test_libstoriqone_value_unpack_0, "libstoriqone: value unpack: #0" },
	{ test_libstoriqone_value_unpack_1, "libstoriqone: value unpack: #1" },

	{ 0, 0 },
};

void test_libstoriqone_value_add_suite() {
	CU_pSuite suite = CU_add_suite("libstoriqone: value", 0, 0);
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


static void test_libstoriqone_value_hashtable_iter_0() {
	struct so_value * hash = so_value_pack("{}");
	CU_ASSERT_PTR_NOT_NULL_FATAL(hash);

	struct so_value_iterator * iter = so_value_hashtable_get_iterator(hash);
	CU_ASSERT_PTR_NOT_NULL_FATAL(iter);

	unsigned int i = 0;
	while (so_value_iterator_has_next(iter)) {
		struct so_value * key = so_value_iterator_get_key(iter, false, true);
		struct so_value * elt = so_value_iterator_get_value(iter, true);

		so_value_free(key);
		so_value_free(elt);

		i++;
	}

	so_value_iterator_free(iter);
	so_value_free(hash);

	CU_ASSERT_EQUAL(i, 0);
}

static void test_libstoriqone_value_hashtable_iter_1() {
	struct so_value * hash = so_value_pack("{sb}", "barcode", false);
	CU_ASSERT_PTR_NOT_NULL_FATAL(hash);

	struct so_value_iterator * iter = so_value_hashtable_get_iterator(hash);
	CU_ASSERT_PTR_NOT_NULL_FATAL(iter);

	unsigned int i = 0;
	while (so_value_iterator_has_next(iter)) {
		struct so_value * key = so_value_iterator_get_key(iter, false, true);
		struct so_value * elt = so_value_iterator_get_value(iter, true);

		so_value_free(key);
		so_value_free(elt);

		i++;
	}

	so_value_iterator_free(iter);
	so_value_free(hash);

	CU_ASSERT_EQUAL(i, 1);
}

static void test_libstoriqone_value_hashtable_put_0() {
	struct so_value * hash = so_value_new_hashtable2();

	unsigned int nb_elts = so_value_hashtable_get_length(hash);
	CU_ASSERT_EQUAL(nb_elts, 0);

	so_value_hashtable_put2(hash, "foo", so_value_new_boolean(false), true);
	nb_elts = so_value_hashtable_get_length(hash);
	CU_ASSERT_EQUAL(nb_elts, 1);

	so_value_hashtable_put2(hash, "bar", so_value_new_boolean(true), true);
	nb_elts = so_value_hashtable_get_length(hash);
	CU_ASSERT_EQUAL(nb_elts, 2);

	so_value_hashtable_put2(hash, "foo", so_value_new_boolean(false), true);
	nb_elts = so_value_hashtable_get_length(hash);
	CU_ASSERT_EQUAL(nb_elts, 2);

	so_value_free(hash);
}

static void test_libstoriqone_value_list_slice_0() {
	struct so_value * list = so_value_pack("[iii]", 1, 2, 3);
	CU_ASSERT_PTR_NOT_NULL_FATAL(list);

	struct so_value * l2 = so_value_list_slice(list, 2);
	CU_ASSERT_PTR_NOT_NULL_FATAL(l2);

	long long i2;
	so_value_unpack(l2, "[i]", &i2);
	CU_ASSERT_EQUAL(i2, 3);

	so_value_free(list);
	so_value_free(l2);
}

static void test_libstoriqone_value_pack_0() {
	struct so_value * pack = so_value_pack("{s[]}", "foo");
	CU_ASSERT_PTR_NOT_NULL_FATAL(pack);

	CU_ASSERT_EQUAL(pack->shared, 1);
	so_value_free(pack);
}

static void test_libstoriqone_value_ref_cycle_0() {
	struct so_value * objA = so_value_pack("{s{}}", "link");
	CU_ASSERT_PTR_NOT_NULL_FATAL(objA);

	struct so_value * objB;
	so_value_unpack(objA, "{so}", "link", &objB);
	CU_ASSERT_PTR_NOT_NULL_FATAL(objB);

	CU_ASSERT_EQUAL(objA->shared, 1);
	CU_ASSERT_EQUAL(objB->shared, 1);

	so_value_hashtable_put2(objB, "ref", objA, false);
	CU_ASSERT_EQUAL(objA->shared, 2);

	so_value_free(objA);
}

static void test_libstoriqone_value_ref_cycle_1() {
	struct so_value * objA = so_value_pack("{s{s{}}}", "parent", "link");
	CU_ASSERT_PTR_NOT_NULL_FATAL(objA);

	struct so_value * objB;
	so_value_unpack(objA, "{so}", "parent", &objB);
	CU_ASSERT_PTR_NOT_NULL_FATAL(objB);

	struct so_value * objC;
	so_value_unpack(objB, "{so}", "link", &objC);
	CU_ASSERT_PTR_NOT_NULL_FATAL(objC);

	CU_ASSERT_EQUAL(objA->shared, 1);
	CU_ASSERT_EQUAL(objB->shared, 1);
	CU_ASSERT_EQUAL(objC->shared, 1);

	so_value_hashtable_put2(objC, "ref", objB, false);
	CU_ASSERT_EQUAL(objB->shared, 2);

	so_value_free(objA);
}

static void test_libstoriqone_value_share_0() {
	struct so_value * val = so_value_new_boolean(true);
	CU_ASSERT_PTR_NOT_NULL_FATAL(val);

	CU_ASSERT_EQUAL(val->shared, 1);

	struct so_value * val_shared = so_value_share(val);
	CU_ASSERT_EQUAL(val->shared, 2);
	CU_ASSERT_EQUAL(val_shared->shared, 2);

	so_value_free(val_shared);
	CU_ASSERT_EQUAL(val->shared, 1);

	so_value_free(val);
}

void test_libstoriqone_value_unpack_0() {
	struct so_value * pack = so_value_pack("{ss}", "foo", "bar");
	CU_ASSERT_PTR_NOT_NULL_FATAL(pack);

	char * value = NULL;
	int ret = so_value_unpack(pack, "{ss}", "foo", &value);

	CU_ASSERT_EQUAL(ret, 1);
	CU_ASSERT_PTR_NOT_NULL(value);
	CU_ASSERT_STRING_EQUAL(value, "bar");

	free(value);
	so_value_free(pack);
}

static void test_libstoriqone_value_unpack_1() {
	struct so_value * pack = so_value_pack("{s{si}sb}", "foo", "bar", 5L, "baz", false);
	CU_ASSERT_PTR_NOT_NULL_FATAL(pack);

	long i = 0;
	bool baz = true;
	int ret = so_value_unpack(pack, "{s{si}sb}", "foo", "bar", &i, "baz", &baz);

	CU_ASSERT_EQUAL(ret, 2);
	CU_ASSERT_EQUAL(i, 5);
	CU_ASSERT_EQUAL(baz, false);

	so_value_free(pack);
}

