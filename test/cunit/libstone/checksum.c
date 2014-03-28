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

#include "checksum.h"

// CU_add_suite, CU_cleanup_registry, CU_cleanup_registry
#include <CUnit/CUnit.h>
// printf
#include <stdio.h>
// free
#include <stdlib.h>
// _exit
#include <unistd.h>

#include <libstone/checksum.h>

static void test_libstone_checksum_compute_0(void);
static void test_libstone_checksum_compute_1(void);
static void test_libstone_checksum_compute_2(void);
static void test_libstone_checksum_compute_3(void);
static void test_libstone_checksum_compute_4(void);

static void test_libstone_checksum_convert_0(void);
static void test_libstone_checksum_convert_1(void);

static void test_libstone_checksum_get_driver_0(void);
static void test_libstone_checksum_get_driver_1(void);

static struct {
	void (*function)(void);
	char * name;
} test_functions[] = {
    { test_libstone_checksum_compute_0, "libstone: checksum compute: md5 #0" },
    { test_libstone_checksum_compute_1, "libstone: checksum compute: sha1 #0" },
    { test_libstone_checksum_compute_2, "libstone: checksum compute: data is null" },
    { test_libstone_checksum_compute_3, "libstone: checksum compute: length is null" },
    { test_libstone_checksum_compute_4, "libstone: checksum compute: length is lower than 0" },

    { test_libstone_checksum_convert_0, "libstone: checksum convert #0" },
    { test_libstone_checksum_convert_1, "libstone: checksum convert #2" },

    { test_libstone_checksum_get_driver_0, "libstone: checksum get driver #0" },
    { test_libstone_checksum_get_driver_1, "libstone: checksum get driver #1" },

	{ 0, 0 },
};


void test_libstone_checksum_add_suite() {
	CU_pSuite suite = CU_add_suite("libstone: checksum", 0, 0);
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


void test_libstone_checksum_compute_0() {
    char * digest = st_checksum_compute("md5", "Hello, world!!!", 15);
    CU_ASSERT_PTR_NOT_NULL_FATAL(digest);
    CU_ASSERT_STRING_EQUAL(digest, "9fe77772b085e3533101d59d33a51f19");
    free(digest);
}

void test_libstone_checksum_compute_1() {
    char * digest = st_checksum_compute("sha1", "Hello, world!!!", 15);
    CU_ASSERT_PTR_NOT_NULL_FATAL(digest);
    CU_ASSERT_STRING_EQUAL(digest, "91a93333a234aa14b2386dee4f644579c64c29a1");
    free(digest);
}

void test_libstone_checksum_compute_2() {
    char * digest = st_checksum_compute("sha256", 0, 15);
    CU_ASSERT_PTR_NULL(digest);
    free(digest);
}

void test_libstone_checksum_compute_3() {
    char * digest = st_checksum_compute("sha256", "", 0);
    CU_ASSERT_PTR_NOT_NULL_FATAL(digest);
    CU_ASSERT_STRING_EQUAL(digest, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    free(digest);
}

void test_libstone_checksum_compute_4() {
    char * digest = st_checksum_compute("sha256", 0, 15);
    CU_ASSERT_PTR_NULL(digest);
    free(digest);
}

void test_libstone_checksum_convert_0() {
    unsigned char digest[] = "abc";
    char hex_digest[7];

    st_checksum_convert_to_hex(digest, 3, hex_digest);
    CU_ASSERT_STRING_EQUAL(hex_digest, "616263");
}

void test_libstone_checksum_convert_1() {
    unsigned char digest[] = "abc";
    char hex_digest[7] = "";

    st_checksum_convert_to_hex(digest, 0, hex_digest);
    CU_ASSERT_STRING_EQUAL(hex_digest, "");
}

void test_libstone_checksum_get_driver_0() {
    struct st_checksum_driver * driver = st_checksum_get_driver("sha1");
    CU_ASSERT_PTR_NOT_NULL(driver);
}

void test_libstone_checksum_get_driver_1() {
    struct st_checksum_driver * driver = st_checksum_get_driver("foo");
    CU_ASSERT_PTR_NULL(driver);
}

