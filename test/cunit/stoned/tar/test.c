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
*  Last modified: Mon, 01 Oct 2012 16:59:36 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE

#include "test.h"

// CU_add_suite, CU_cleanup_registry, CU_cleanup_registry
#include <CUnit/CUnit.h>
// scandir
#include <dirent.h>
// open
#include <fcntl.h>
// asprintf, printf
#include <stdio.h>
// free
#include <stdlib.h>
// lstat, open
#include <sys/stat.h>
// lstat, open
#include <sys/types.h>
// _exit, lstat, system
#include <unistd.h>

#include <stone/tar.h>

#include "pipe_io.h"
#include "temp_io.h"

static void test_stoned_tar_reader_parse_0(void);
static void test_stoned_tar_reader_parse_1(void);
static void test_stoned_tar_reader_parse_2(void);

static void test_stoned_tar_writer_single_label(void);
static void test_stoned_tar_writer_add_single_file(void);
static void test_stoned_tar_writer_add_directory_0(void);
static void test_stoned_tar_writer_add_directory_1(void);
static void test_stoned_tar_writer_add_directory_2(void);

static int test_stoned_tar_writer_add_file(struct st_tar_out * tar, const char * filename);
static int test_stoned_tar_writer_filter(const struct dirent * file);

static struct {
	void (*function)(void);
	char * name;
} test_functions[] = {
	{ test_stoned_tar_reader_parse_0, "stoned: tar reader: parse #0"},
	{ test_stoned_tar_reader_parse_1, "stoned: tar reader: parse #1"},
	{ test_stoned_tar_reader_parse_2, "stoned: tar reader: parse #2"},

	{ test_stoned_tar_writer_single_label, "stoned: tar writer: single label, #0" },

	{ test_stoned_tar_writer_add_single_file, "stoned: tar writer: add single file #0" },
	{ test_stoned_tar_writer_add_directory_0, "stoned: tar writer: add directory #0" },
	{ test_stoned_tar_writer_add_directory_1, "stoned: tar writer: add directory #1" },
	{ test_stoned_tar_writer_add_directory_2, "stoned: tar writer: add directory #2" },

	{ 0, 0 },
};

void test_stoned_tar_add_suite() {
	CU_pSuite suite = CU_add_suite("stoned: tar", 0, 0);
	if (!suite) {
		CU_cleanup_registry();
		printf("Error while adding suite stoned tar because %s\n", CU_get_error_msg());
		_exit(3);
	}

	int i;
	for (i = 0; test_functions[i].name; i++) {
		if (!CU_add_test(suite, test_functions[i].name, test_functions[i].function)) {
			CU_cleanup_registry();
			printf("Error while adding test function '%s' stoned tar because %s\n", test_functions[i].name, CU_get_error_msg());
			_exit(3);
		}
	}
}


void test_stoned_tar_reader_parse_0() {
	static char * tar_params[] = { "tar", "cf", "-", "test", NULL };
	struct st_stream_reader * file = test_stoned_tar_get_pipe_reader(tar_params, NULL);
	CU_ASSERT_PTR_NOT_NULL_FATAL(file);

	struct st_tar_in * tar = st_tar_new_in(file);
	CU_ASSERT_PTR_NOT_NULL_FATAL(tar);

	struct st_tar_header header;
	enum st_tar_header_status hdr_status;

	while ((hdr_status = tar->ops->get_header(tar, &header)) == ST_TAR_HEADER_OK) {
		struct stat fs;
		int failed = lstat(header.path, &fs);

		CU_ASSERT_EQUAL(failed, 0);

		if (header.link && S_ISREG(header.mode)) {
			CU_ASSERT_TRUE(S_ISREG(fs.st_mode));
			CU_ASSERT_EQUAL(header.size, 0);
		} else if (S_ISFIFO(header.mode)) {
			CU_ASSERT_TRUE(S_ISFIFO(fs.st_mode));
		} else if (S_ISCHR(header.mode)) {
			CU_ASSERT_TRUE(S_ISCHR(fs.st_mode));
			CU_ASSERT_EQUAL(header.dev, fs.st_dev);
		} else if (S_ISDIR(header.mode)) {
			CU_ASSERT_TRUE(S_ISDIR(fs.st_mode));
		} else if (S_ISBLK(header.mode)) {
			CU_ASSERT_TRUE(S_ISBLK(fs.st_mode));
			CU_ASSERT_EQUAL(header.dev, fs.st_dev);
		} else if (S_ISREG(header.mode)) {
			CU_ASSERT_TRUE(S_ISREG(fs.st_mode));
			CU_ASSERT_EQUAL(header.size, fs.st_size);

			tar->ops->skip_file(tar);
		} else if (S_ISLNK(header.mode)) {
			CU_ASSERT_TRUE(S_ISLNK(fs.st_mode));

			char * link = malloc(256);
			ssize_t link_length = readlink(header.path, link, 256);
			CU_ASSERT_NOT_EQUAL(link_length, -1);

			if (link_length > 0) {
				link[link_length] = '\0';

				CU_ASSERT_STRING_EQUAL(header.link, link);
			}

			free(link);
		}

		st_tar_free_header(&header);
	}

	CU_ASSERT_EQUAL(hdr_status, ST_TAR_HEADER_NOT_FOUND);

	int failed = tar->ops->close(tar);
	CU_ASSERT_EQUAL(failed, 0);

	tar->ops->free(tar);
}

void test_stoned_tar_reader_parse_1() {
	static char * tar_params[] = { "tar", "cf", "-", "usr", NULL };
	struct st_stream_reader * file = test_stoned_tar_get_pipe_reader(tar_params, "/");
	CU_ASSERT_PTR_NOT_NULL_FATAL(file);

	struct st_tar_in * tar = st_tar_new_in(file);
	CU_ASSERT_PTR_NOT_NULL_FATAL(tar);

	struct st_tar_header header;
	enum st_tar_header_status hdr_status;

	while ((hdr_status = tar->ops->get_header(tar, &header)) == ST_TAR_HEADER_OK) {
		char * path = malloc(strlen(header.path) + 2);
		path[0] = '/';
		strcpy(path + 1, header.path);

		struct stat fs;
		int failed = lstat(path, &fs);

		CU_ASSERT_EQUAL(failed, 0);

		if (header.link && S_ISREG(header.mode)) {
			CU_ASSERT_TRUE(S_ISREG(fs.st_mode));
			CU_ASSERT_EQUAL(header.size, 0);
		} else if (S_ISFIFO(header.mode)) {
			CU_ASSERT_TRUE(S_ISFIFO(fs.st_mode));
		} else if (S_ISCHR(header.mode)) {
			CU_ASSERT_TRUE(S_ISCHR(fs.st_mode));
			CU_ASSERT_EQUAL(header.dev, fs.st_dev);
		} else if (S_ISDIR(header.mode)) {
			CU_ASSERT_TRUE(S_ISDIR(fs.st_mode));
		} else if (S_ISBLK(header.mode)) {
			CU_ASSERT_TRUE(S_ISBLK(fs.st_mode));
			CU_ASSERT_EQUAL(header.dev, fs.st_dev);
		} else if (S_ISREG(header.mode)) {
			CU_ASSERT_TRUE(S_ISREG(fs.st_mode));
			CU_ASSERT_EQUAL(header.size, fs.st_size);

			tar->ops->skip_file(tar);
		} else if (S_ISLNK(header.mode)) {
			CU_ASSERT_TRUE(S_ISLNK(fs.st_mode));

			char * link = malloc(256);
			ssize_t link_length = readlink(path, link, 256);
			CU_ASSERT_NOT_EQUAL(link_length, -1);

			if (link_length > 0) {
				link[link_length] = '\0';

				CU_ASSERT_STRING_EQUAL(header.link, link);
			}

			free(link);
		}

		st_tar_free_header(&header);
		free(path);
	}

	CU_ASSERT_EQUAL(hdr_status, ST_TAR_HEADER_NOT_FOUND);

	int failed = tar->ops->close(tar);
	CU_ASSERT_EQUAL(failed, 0);

	tar->ops->free(tar);
}

void test_stoned_tar_reader_parse_2() {
	static char * tar_params[] = { "tar", "cf", "-", "test-data", NULL };
	struct st_stream_reader * file = test_stoned_tar_get_pipe_reader(tar_params, NULL);
	CU_ASSERT_PTR_NOT_NULL_FATAL(file);

	struct st_tar_in * tar = st_tar_new_in(file);
	CU_ASSERT_PTR_NOT_NULL_FATAL(tar);

	struct st_tar_header header;
	enum st_tar_header_status hdr_status;

	while ((hdr_status = tar->ops->get_header(tar, &header)) == ST_TAR_HEADER_OK) {
		struct stat fs;
		int failed = lstat(header.path, &fs);

		CU_ASSERT_EQUAL(failed, 0);

		if (header.link && S_ISREG(header.mode)) {
			CU_ASSERT_TRUE(S_ISREG(fs.st_mode));
			CU_ASSERT_EQUAL(header.size, 0);
		} else if (S_ISFIFO(header.mode)) {
			CU_ASSERT_TRUE(S_ISFIFO(fs.st_mode));
		} else if (S_ISCHR(header.mode)) {
			CU_ASSERT_TRUE(S_ISCHR(fs.st_mode));
			CU_ASSERT_EQUAL(header.dev, fs.st_dev);
		} else if (S_ISDIR(header.mode)) {
			CU_ASSERT_TRUE(S_ISDIR(fs.st_mode));
		} else if (S_ISBLK(header.mode)) {
			CU_ASSERT_TRUE(S_ISBLK(fs.st_mode));
			CU_ASSERT_EQUAL(header.dev, fs.st_dev);
		} else if (S_ISREG(header.mode)) {
			CU_ASSERT_TRUE(S_ISREG(fs.st_mode));
			CU_ASSERT_EQUAL(header.size, fs.st_size);

			tar->ops->skip_file(tar);
		} else if (S_ISLNK(header.mode)) {
			CU_ASSERT_TRUE(S_ISLNK(fs.st_mode));

			char * link = malloc(256);
			ssize_t link_length = readlink(header.path, link, 256);
			CU_ASSERT_NOT_EQUAL(link_length, -1);

			if (link_length > 0) {
				link[link_length] = '\0';

				CU_ASSERT_STRING_EQUAL(header.link, link);
			}

			free(link);
		}

		st_tar_free_header(&header);
	}

	CU_ASSERT_EQUAL(hdr_status, ST_TAR_HEADER_NOT_FOUND);

	int failed = tar->ops->close(tar);
	CU_ASSERT_EQUAL(failed, 0);

	tar->ops->free(tar);
}


void test_stoned_tar_writer_single_label() {
	struct st_stream_writer * file = st_stream_get_tmp_writer();
	CU_ASSERT_PTR_NOT_NULL_FATAL(file);

	struct st_tar_out * tar = st_tar_new_out(file);
	CU_ASSERT_PTR_NOT_NULL_FATAL(tar);

	enum st_tar_out_status status = tar->ops->add_label(tar, "foo");
	CU_ASSERT_PTR_EQUAL(status, ST_TAR_OUT_OK);

	int failed = tar->ops->close(tar);
	CU_ASSERT_EQUAL(failed, 0);

	char * command;
	asprintf(&command, "tar tf %s > /dev/null", test_stoned_tar_get_filename(file));

	failed = system(command);
	CU_ASSERT_EQUAL(failed, 0);

	free(command);
	tar->ops->free(tar);
}


int test_stoned_tar_writer_add_file(struct st_tar_out * tar, const char * filename) {
	struct stat st;
	if (lstat(filename, &st))
		return 1;

	if (S_ISSOCK(st.st_mode))
		return 0;

	enum st_tar_out_status status = tar->ops->add_file(tar, filename, filename);
	CU_ASSERT_PTR_EQUAL(status, ST_TAR_OUT_OK);

	if (S_ISREG(st.st_mode)) {
		int fd = open(filename, O_RDONLY);
		CU_ASSERT_TRUE(fd >= 0);

		ssize_t nb_read;
		static char buffer[4096];
		while ((nb_read = read(fd, buffer, 4096)) > 0) {
			ssize_t nb_write = tar->ops->write(tar, buffer, nb_read);
			CU_ASSERT_EQUAL(nb_read, nb_write);
		}

		CU_ASSERT_EQUAL(nb_read, 0);

		int failed = close(fd);
		CU_ASSERT_EQUAL(failed, 0);

		failed = tar->ops->end_of_file(tar);
		CU_ASSERT_EQUAL(failed, 0);
	} else if (S_ISDIR(st.st_mode)) {
		if (access(filename, R_OK | X_OK)) {
			return 6;
		}

		struct dirent ** dl = 0;
		int nb_files = scandir(filename, &dl, test_stoned_tar_writer_filter, 0);

		int i, failed = 0;
		for (i = 0; i < nb_files; i++) {
			if (!failed) {
				char * subpath = 0;
				asprintf(&subpath, "%s/%s", filename, dl[i]->d_name);

				failed = test_stoned_tar_writer_add_file(tar, subpath);

				free(subpath);
			}

			free(dl[i]);
		}

		free(dl);

		return failed;
	}

	return 0;
}

int test_stoned_tar_writer_filter(const struct dirent * file) {
	if (!strcmp(file->d_name, "."))
		return 0;
	return strcmp(file->d_name, "..");
}

void test_stoned_tar_writer_add_single_file() {
	static char * tar_params[] = { "tar", "df", "-", NULL };
	struct st_stream_writer * file = test_stoned_tar_get_pipe_writer(tar_params, NULL);
	CU_ASSERT_PTR_NOT_NULL_FATAL(file);

	struct st_tar_out * tar = st_tar_new_out(file);
	CU_ASSERT_PTR_NOT_NULL_FATAL(tar);

	int failed = test_stoned_tar_writer_add_file(tar, __FILE__);
	CU_ASSERT_EQUAL(failed, 0);

	failed = tar->ops->close(tar);
	CU_ASSERT_EQUAL(failed, 0);

	tar->ops->free(tar);
}

void test_stoned_tar_writer_add_directory_0() {
	static char * tar_params[] = { "tar", "df", "-", NULL };
	struct st_stream_writer * file = test_stoned_tar_get_pipe_writer(tar_params, NULL);
	CU_ASSERT_PTR_NOT_NULL_FATAL(file);

	struct st_tar_out * tar = st_tar_new_out(file);
	CU_ASSERT_PTR_NOT_NULL_FATAL(tar);

	int failed = test_stoned_tar_writer_add_file(tar, "test");
	CU_ASSERT_EQUAL(failed, 0);

	failed = tar->ops->close(tar);
	CU_ASSERT_EQUAL(failed, 0);

	tar->ops->free(tar);
}

void test_stoned_tar_writer_add_directory_1() {
	static char * tar_params[] = { "tar", "df", "-", NULL };
	struct st_stream_writer * file = test_stoned_tar_get_pipe_writer(tar_params, "/");
	CU_ASSERT_PTR_NOT_NULL_FATAL(file);

	struct st_tar_out * tar = st_tar_new_out(file);
	CU_ASSERT_PTR_NOT_NULL_FATAL(tar);

	int failed = test_stoned_tar_writer_add_file(tar, "/usr/bin");
	CU_ASSERT_EQUAL(failed, 0);

	failed = tar->ops->close(tar);
	CU_ASSERT_EQUAL(failed, 0);

	tar->ops->free(tar);
}

void test_stoned_tar_writer_add_directory_2() {
	static char * tar_params[] = { "tar", "df", "-", NULL };
	struct st_stream_writer * file = test_stoned_tar_get_pipe_writer(tar_params, "/");
	CU_ASSERT_PTR_NOT_NULL_FATAL(file);

	struct st_tar_out * tar = st_tar_new_out(file);
	CU_ASSERT_PTR_NOT_NULL_FATAL(tar);

	int failed = test_stoned_tar_writer_add_file(tar, "/bin");
	CU_ASSERT_EQUAL(failed, 0);

	failed = tar->ops->close(tar);
	CU_ASSERT_EQUAL(failed, 0);

	tar->ops->free(tar);
}

