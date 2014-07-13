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

#include "process.h"

// CU_add_suite, CU_cleanup_registry, CU_cleanup_registry
#include <CUnit/CUnit.h>
// printf
#include <stdio.h>
// free
#include <stdlib.h>
// _exit
#include <unistd.h>

#include <libstone/process.h>

static void test_libstone_process_0(void);

static struct {
	void (*function)(void);
	char * name;
} test_functions[] = {
    { test_libstone_process_0, "libstone: process run date -R" },

	{ 0, 0 },
};


void test_libstone_process_add_suite() {
	CU_pSuite suite = CU_add_suite("libstone: process", 0, 0);
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


static void test_libstone_process_0() {
	struct st_process date;
	static const char * params[] = { "-R" };

	st_process_new(&date, "date", params, 1);
	int fd = st_process_pipe_from(&date, st_process_stdout);
	st_process_start(&date, 1);

	char buf[64];
	ssize_t nb_read = read(fd, buf, 64);
	close(fd);

	st_process_wait(&date, 1);
	st_process_free(&date, 1);

	CU_ASSERT_NOT_EQUAL(nb_read, 0);
}

