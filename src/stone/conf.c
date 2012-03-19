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
*  Last modified: Mon, 19 Mar 2012 17:55:15 +0100                         *
\*************************************************************************/

// strerror
#include <errno.h>
// open
#include <fcntl.h>
// free, malloc
#include <malloc.h>
// snprintf, sscanf
#include <stdio.h>
// strchp, strcmp, strerror, strlen, strncmp, strrchr
#include <string.h>
// fstat, open
#include <sys/stat.h>
// fstat, open
#include <sys/types.h>
// access, close, fstat, read, readlink, unlink, write
#include <unistd.h>

#include <stone/database.h>
#include <stone/util/hashtable.h>

#include "conf.h"
#include "log.h"
#include "util/util.h"

enum st_conf_section {
	st_conf_section_db,
	st_conf_section_log,
	st_conf_section_unknown,
};

static void st_conf_load_db(struct st_hashtable * params);
static void st_conf_load_log(struct st_hashtable * params);


int st_conf_check_pid(int pid) {
	if (pid < 1) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: check_pid: pid contains a wrong value (pid=%d)", pid);
		return 0;
	}

	char path[64];
	snprintf(path, 64, "/proc/%d/exe", pid);

	if (access(path, F_OK)) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "Conf: check_pid: there is no process with pid=%d", pid);
		return 0;
	}

	char link[128];
	if (readlink(path, link, 128) < 0) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: check_pid: readlink failed (%s) => %s", link, strerror(errno));
		return 0;
	}

	char * ptr = strrchr(link, '/');
	if (ptr)
		ptr++;
	else
		ptr = link;

	int failed = strcmp(link, "stone");
	st_log_write_all(failed ? st_log_level_warning : st_log_level_info, st_log_type_daemon, "Conf: check_pid: process 'stone' %s", failed ? "not found" : "found");
	return failed ? -1 : 1;
}

int st_conf_delete_pid(const char * pid_file) {
	if (!pid_file) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: delete_pid: pid_file is null");
		return 1;
	}

	int failed = unlink(pid_file);
	if (failed)
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: delete_pid: delete pid file => failed");
	else
		st_log_write_all(st_log_level_debug, st_log_type_daemon, "Conf: delete_pid: delete pid file => ok");
	return failed;
}

int st_conf_read_pid(const char * pid_file) {
	if (!pid_file) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: read_pid: pid_file is null");
		return -1;
	}

	if (access(pid_file, R_OK)) {
		st_log_write_all(st_log_level_warning, st_log_type_daemon, "Conf: read_pid: read pid failed because cannot access to '%s'", pid_file);
		return -1;
	}

	// TODO: do some check
	int fd = open(pid_file, O_RDONLY);
	char buffer[16];
	read(fd, buffer, 16);
	close(fd);

	int pid = 0;
	if (sscanf(buffer, "%d", &pid) == 1) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "Conf: read_pid: pid found (%d)", pid);
		return pid;
	}

	st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: read_pid: failed to parse pid");
	return -1;
}

int st_conf_write_pid(const char * pid_file, int pid) {
	if (!pid_file || pid < 1) {
		if (!pid_file)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: write_pid: pid_file is null");
		if (pid < 1)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: write_pid: pid should be greater than 0 (pid=%d)", pid);
		return 1;
	}

	int fd = open(pid_file, O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (fd < 0) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: write_pid: failed to open '%s' => %s", pid_file, strerror(errno));
		return 1;
	}

	dprintf(fd, "%d\n", pid);
	close(fd);

	st_log_write_all(st_log_level_info, st_log_type_daemon, "Conf: write_pid: write ok (pid=%d)", pid);

	return 0;
}


void st_conf_load_db(struct st_hashtable * params) {
	if (!params)
		return;

	char * driver = st_hashtable_value(params, "driver");
	if (!driver) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "conf: load_db: there is no driver in config file");
		return;
	}

	struct st_database * db = st_db_get_db(driver);
	if (db) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "Conf: load_db: driver (%s) => ok", driver);
		short setup_ok = !db->ops->setup(db, params);
		short ping_ok = db->ops->ping(db) > 0;
		st_log_write_all(setup_ok || ping_ok ? st_log_level_info : st_log_level_error, st_log_type_daemon, "Conf: load_db: setup %s, ping %s", setup_ok ? "ok" : "failed", ping_ok ? "ok" : "failed");

		if (!st_db_get_default_db())
			st_db_set_default_db(db);
	} else
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: load_db: no driver (%s) found", driver);
}

void st_conf_load_log(struct st_hashtable * params) {
	if (!params)
		return;

	char * alias = st_hashtable_value(params, "alias");
	char * type = st_hashtable_value(params, "type");
	enum st_log_level verbosity = st_log_string_to_level(st_hashtable_value(params, "verbosity"));

	if (!alias || !type || verbosity == st_log_level_unknown) {
		if (!alias)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: load_log: alias required for log");
		if (!type)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: load_log: type required for log");
		if (verbosity == st_log_level_unknown)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: load_log: verbosity required for log");
		return;
	}

	struct st_log_driver * dr = st_log_get_driver(type);
	if (dr) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "Conf: load_log: using module='%s', alias='%s', verbosity='%s'", type, alias, st_log_level_to_string(verbosity));
		dr->add(dr, alias, verbosity, params);
	} else
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: load_log: module='%s' not found", type);
}

int st_conf_read_config(const char * confFile) {
	if (access(confFile, R_OK)) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: read_config: Can't access to '%s'", confFile);
		return -1;
	}

	int fd = open(confFile, O_RDONLY);

	struct stat st;
	fstat(fd, &st);

	char * buffer = malloc(st.st_size + 1);
	read(fd, buffer, st.st_size);
	close(fd);
	buffer[st.st_size] = '\0';

	char * ptr = buffer;
	enum st_conf_section section = st_conf_section_unknown;
	struct st_hashtable * params = st_hashtable_new2(st_util_hash_string, st_util_free_key_value);
	while (*ptr) {
		switch (*ptr) {
			case ';':
				ptr = strchr(ptr, '\n');
				continue;

			case '\n':
				ptr++;
				if (*ptr == '\n') {
					switch (section) {
						case st_conf_section_db:
							st_conf_load_db(params);
							break;

						case st_conf_section_log:
							st_conf_load_log(params);
							break;

						default:
							break;
					}

					section = st_conf_section_unknown;
					st_hashtable_clear(params);
				}
				continue;

			case '[':
				if (strlen(ptr + 1) > 8 && !strncmp(ptr + 1, "database", 8))
					section = st_conf_section_db;
				else if (strlen(ptr + 1) > 3 && !strncmp(ptr + 1, "log", 3))
					section = st_conf_section_log;
				else
					section = st_conf_section_unknown;
				ptr = strchr(ptr, '\n');
				continue;

			default:
				if (section == st_conf_section_unknown)
					continue;

				if (strchr(ptr, '=') < strchr(ptr, '\n')) {
					char key[32];
					char value[64];
					int val = sscanf(ptr, "%s = %s", key, value);
					if (val == 2)
						st_hashtable_put(params, strdup(key), strdup(value));
				}
				ptr = strchr(ptr, '\n');
		}
	}

	if (params->nb_elements > 0) {
		switch (section) {
			case st_conf_section_db:
				st_conf_load_db(params);
				break;

			case st_conf_section_log:
				st_conf_load_log(params);
				break;

			default:
				break;
		}
	}

	st_hashtable_free(params);
	free(buffer);

	st_log_start_logger();

	return 0;
}

