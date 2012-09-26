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
*  Last modified: Mon, 04 Jun 2012 18:43:30 +0200                         *
\*************************************************************************/

#include "test.h"

// CU_add_suite, CU_cleanup_registry, CU_cleanup_registry
#include <CUnit/CUnit.h>
// errno
#include <errno.h>
// open
#include <fcntl.h>
// printf
#include <stdio.h>
// open, stat
#include <sys/stat.h>
// open, stat
#include <sys/types.h>
// access, close, _exit, stat
#include <unistd.h>

#include <stone/conf.h>

static void test_libstone_conf_delete_pid_0(void);
static void test_libstone_conf_delete_pid_1(void);

static void test_libstone_conf_read_pid_0(void);
static void test_libstone_conf_read_pid_1(void);
static void test_libstone_conf_read_pid_2(void);
static void test_libstone_conf_read_pid_3(void);

static void test_libstone_conf_write_pid_0(void);
static void test_libstone_conf_write_pid_1(void);
static void test_libstone_conf_write_pid_2(void);

static struct {
	void (*function)(void);
	char * name;
} test_functions[] = {
	{ test_libstone_conf_delete_pid_0, "libstone: conf delete_pid: delete null file" },
	{ test_libstone_conf_delete_pid_1, "libstone: conf delete_pid: delete file 'test-data/'" },

	{ test_libstone_conf_read_pid_0, "libstone: conf read_pid: read pid of null file" },
	{ test_libstone_conf_read_pid_1, "libstone: conf read_pid: read pid of an unexisted file" },
	{ test_libstone_conf_read_pid_2, "libstone: conf read_pid: read pid of file with no data" },
	{ test_libstone_conf_read_pid_3, "libstone: conf read_pid: read pid of file with a pid" },

	{ test_libstone_conf_write_pid_0, "libstone: conf write_pid: write pid to null file and/or pid lower than 0" },
	{ test_libstone_conf_write_pid_1, "libstone: conf write_pid: write pid to directory" },
	{ test_libstone_conf_write_pid_2, "libstone: conf write_pid: write pid to file" },

	{ 0, 0 },
};


void test_libstone_conf_add_suite() {
	CU_pSuite suite = CU_add_suite("libstone: conf", 0, 0);
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


void test_libstone_conf_delete_pid_0() {
	int failed = st_conf_delete_pid(NULL);
	CU_ASSERT_EQUAL(failed, 1);
}

void test_libstone_conf_delete_pid_1() {
	const char * path = "test-data/foo";

	struct stat buf;
	int failed = stat(path, &buf);

	if (failed) {
		CU_ASSERT_EQUAL_FATAL(errno, ENOENT);

		int fd = open(path, O_WRONLY | O_CREAT, 0644);
		CU_ASSERT_TRUE_FATAL(fd >= 0);
		close(fd);
	}

	failed = st_conf_delete_pid(path);
	CU_ASSERT_EQUAL_FATAL(failed, 0);

	failed = st_conf_delete_pid(path);
	CU_ASSERT_NOT_EQUAL_FATAL(failed, 0);
}

void test_libstone_conf_read_pid_0() {
	int failed = st_conf_read_pid(NULL);
	CU_ASSERT_EQUAL_FATAL(failed, -1);
}

void test_libstone_conf_read_pid_1() {
	const char * path = "test-data/pid.file";

	if (!access(path, F_OK))
		unlink(path);

	int failed = st_conf_read_pid(path);
	CU_ASSERT_EQUAL_FATAL(failed, -1);
}

void test_libstone_conf_read_pid_2() {
	const char * path = "test-data/pid.file";

	if (access(path, F_OK)) {
		int fd = open(path, O_WRONLY | O_CREAT, 0644);
		CU_ASSERT_TRUE_FATAL(fd >= 0);
		close(fd);
	} else {
		int failed = truncate(path, 0);
		CU_ASSERT_EQUAL_FATAL(failed, 0);
	}

	int pid = st_conf_read_pid(path);
	CU_ASSERT_EQUAL(pid, -1);
}

void test_libstone_conf_read_pid_3() {
	const char * path = "test-data/pid.file";

	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	CU_ASSERT_TRUE_FATAL(fd >= 0);

	ssize_t nb_write = write(fd, "123", 3);
	CU_ASSERT_EQUAL_FATAL(nb_write, 3);

	close(fd);

	int pid = st_conf_read_pid(path);
	CU_ASSERT_EQUAL(pid, 123);
}

void test_libstone_conf_write_pid_0() {
	int failed = st_conf_write_pid(NULL, -123);
	CU_ASSERT_EQUAL(failed, 1);

	failed = st_conf_write_pid("test-data/pid.file", -123);
	CU_ASSERT_EQUAL(failed, 1);

	failed = st_conf_write_pid(NULL, 123);
	CU_ASSERT_EQUAL(failed, 1);
}

void test_libstone_conf_write_pid_1() {
	int failed = st_conf_write_pid("test-data/", 123);
	CU_ASSERT_EQUAL(failed, 1);
}

void test_libstone_conf_write_pid_2() {
	const char * path = "test-data/pid.file";

	int failed = st_conf_write_pid(path, 123);
	CU_ASSERT_EQUAL_FATAL(failed, 0);

	int pid = st_conf_read_pid(path);
	CU_ASSERT_EQUAL(pid, 123);
}

