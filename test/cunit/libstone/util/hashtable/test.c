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
*  Last modified: Wed, 26 Sep 2012 14:56:31 +0200                         *
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

#include <stone/util/hashtable.h>
#include <stone/util.h>

static void test_libstone_util_hashtable_new_0(void);
static void test_libstone_util_hashtable_new_1(void);

static void test_libstone_util_hashtable_put_0(void);
static void test_libstone_util_hashtable_put_1(void);
static void test_libstone_util_hashtable_put_2(void);

static void test_libstone_util_hashtable_get_0(void);
static void test_libstone_util_hashtable_get_1(void);

static void test_libstone_util_hashtable_clear_0(void);
static void test_libstone_util_hashtable_clear_1(void);

static void test_libstone_util_hashtable_has_key_0(void);
static void test_libstone_util_hashtable_has_key_1(void);

static void test_libstone_util_hashtable_remove_0(void);


static struct {
	void (*function)(void);
	char * name;
} test_functions[] = {
	{ test_libstone_util_hashtable_new_0, "libstone: util/hashtable new #0" },
	{ test_libstone_util_hashtable_new_1, "libstone: util/hashtable new #1" },

	{ test_libstone_util_hashtable_put_0, "libstone: util/hashtable put #0" },
	{ test_libstone_util_hashtable_put_1, "libstone: util/hashtable put #1" },
	{ test_libstone_util_hashtable_put_2, "libstone: util/hashtable put #2" },

	{ test_libstone_util_hashtable_get_0, "libstone: util/hashtable get #0" },
	{ test_libstone_util_hashtable_get_1, "libstone: util/hashtable get #1" },

	{ test_libstone_util_hashtable_clear_0, "libstone: util/hashtable clear #0" },
	{ test_libstone_util_hashtable_clear_1, "libstone: util/hashtable clear #1" },

	{ test_libstone_util_hashtable_has_key_0, "libstone: util/hashtable has_key #0" },
	{ test_libstone_util_hashtable_has_key_1, "libstone: util/hashtable has_key #1" },

	{ test_libstone_util_hashtable_remove_0, "libstone: util/hashtable remove #0" },

	{ 0, 0 },
};


void test_libstone_util_hashtable_add_suite() {
	CU_pSuite suite = CU_add_suite("libstone: util/hashtable", 0, 0);
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


void test_libstone_util_hashtable_new_0() {
	struct st_hashtable * table = st_hashtable_new(st_util_compute_hash_string);

	CU_ASSERT_EQUAL(table->nb_elements, 0);
	CU_ASSERT_EQUAL(table->compute_hash, st_util_compute_hash_string);

	st_hashtable_free(table);
}

void test_libstone_util_hashtable_new_1() {
	struct st_hashtable * table = st_hashtable_new2(st_util_compute_hash_string, st_util_basic_free);

	CU_ASSERT_EQUAL(table->nb_elements, 0);
	CU_ASSERT_EQUAL(table->compute_hash, st_util_compute_hash_string);
	CU_ASSERT_EQUAL(table->release_key_value, st_util_basic_free);

	st_hashtable_free(table);
}


void test_libstone_util_hashtable_put_0() {
	struct st_hashtable * table = st_hashtable_new(st_util_compute_hash_string);

	CU_ASSERT_EQUAL(table->nb_elements, 0);

	st_hashtable_put(table, "foo", "bar");
	CU_ASSERT_EQUAL(table->nb_elements, 1);

	st_hashtable_free(table);
}

void test_libstone_util_hashtable_put_1() {
	struct st_hashtable * table = st_hashtable_new(st_util_compute_hash_string);

	CU_ASSERT_EQUAL(table->nb_elements, 0);

	st_hashtable_put(table, "foo", "bar");
	CU_ASSERT_EQUAL(table->nb_elements, 1);

	st_hashtable_put(table, "bar", "foo");
	CU_ASSERT_EQUAL(table->nb_elements, 2);

	st_hashtable_free(table);
}

void test_libstone_util_hashtable_put_2() {
	struct st_hashtable * table = st_hashtable_new(st_util_compute_hash_string);

	CU_ASSERT_EQUAL(table->nb_elements, 0);

	st_hashtable_put(table, "foo", "bar");
	CU_ASSERT_EQUAL(table->nb_elements, 1);

	st_hashtable_put(table, "bar", "foo");
	CU_ASSERT_EQUAL(table->nb_elements, 2);

	st_hashtable_put(table, "foo", "foo");
	CU_ASSERT_EQUAL(table->nb_elements, 2);

	st_hashtable_free(table);
}


void test_libstone_util_hashtable_get_0() {
	struct st_hashtable * table = st_hashtable_new(st_util_compute_hash_string);

	st_hashtable_put(table, "foo", "bar");

	char * value = st_hashtable_value(table, "bar");
	CU_ASSERT_PTR_NULL(value);

	value = st_hashtable_value(table, "foo");
	CU_ASSERT_STRING_EQUAL(value, "bar");

	st_hashtable_free(table);
}

void test_libstone_util_hashtable_get_1() {
	struct st_hashtable * table = st_hashtable_new(st_util_compute_hash_string);

	st_hashtable_put(table, "foo", "bar");

	char * value = st_hashtable_value(table, "bar");
	CU_ASSERT_PTR_NULL(value);

	value = st_hashtable_value(table, "foo");
	CU_ASSERT_STRING_EQUAL(value, "bar");


	st_hashtable_put(table, "foo", "foo");

	value = st_hashtable_value(table, "foo");
	CU_ASSERT_STRING_EQUAL(value, "foo");

	st_hashtable_free(table);
}


void test_libstone_util_hashtable_clear_0() {
	struct st_hashtable * table = st_hashtable_new(st_util_compute_hash_string);
	CU_ASSERT_EQUAL(table->nb_elements, 0);

	st_hashtable_put(table, "foo", "bar");
	CU_ASSERT_EQUAL(table->nb_elements, 1);

	st_hashtable_clear(table);
	CU_ASSERT_EQUAL(table->nb_elements, 0);

	st_hashtable_free(table);
}

void test_libstone_util_hashtable_clear_1() {
	struct st_hashtable * table = st_hashtable_new(st_util_compute_hash_string);
	CU_ASSERT_EQUAL(table->nb_elements, 0);

	st_hashtable_put(table, "foo", "bar");
	CU_ASSERT_EQUAL(table->nb_elements, 1);

	st_hashtable_put(table, "bar", "foo");
	CU_ASSERT_EQUAL(table->nb_elements, 2);

	st_hashtable_clear(table);
	CU_ASSERT_EQUAL(table->nb_elements, 0);

	st_hashtable_free(table);
}


void test_libstone_util_hashtable_has_key_0() {
	struct st_hashtable * table = st_hashtable_new(st_util_compute_hash_string);

	st_hashtable_put(table, "foo", "bar");

	short value = st_hashtable_has_key(table, "bar");
	CU_ASSERT_EQUAL(value, 0);

	value = st_hashtable_has_key(table, "foo");
	CU_ASSERT_NOT_EQUAL(value, 0);

	st_hashtable_free(table);
}

void test_libstone_util_hashtable_has_key_1() {
	struct st_hashtable * table = st_hashtable_new(st_util_compute_hash_string);

	st_hashtable_put(table, "foo", "bar");

	short value = st_hashtable_has_key(table, "bar");
	CU_ASSERT_EQUAL(value, 0);

	value = st_hashtable_has_key(table, "foo");
	CU_ASSERT_NOT_EQUAL(value, 0);


	st_hashtable_put(table, "foo", "foo");

	value = st_hashtable_has_key(table, "foo");
	CU_ASSERT_NOT_EQUAL(value, 0);

	st_hashtable_free(table);
}


void test_libstone_util_hashtable_remove_0() {
	struct st_hashtable * table = st_hashtable_new(st_util_compute_hash_string);
	CU_ASSERT_EQUAL(table->nb_elements, 0);

	st_hashtable_put(table, "foo", "bar");
	CU_ASSERT_EQUAL(table->nb_elements, 1);

	short value = st_hashtable_has_key(table, "foo");
	CU_ASSERT_NOT_EQUAL(value, 0);

	st_hashtable_remove(table, "bar");
	CU_ASSERT_EQUAL(table->nb_elements, 1);

	st_hashtable_remove(table, "foo");
	CU_ASSERT_EQUAL(table->nb_elements, 0);

	value = st_hashtable_has_key(table, "foo");
	CU_ASSERT_EQUAL(value, 0);

	st_hashtable_free(table);
}

