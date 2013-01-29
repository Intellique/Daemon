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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 24 Dec 2012 19:26:38 +0100                         *
\*************************************************************************/

// strerror
#include <errno.h>
// open
#include <fcntl.h>
// free, malloc
#include <malloc.h>
// bool
#include <stdbool.h>
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

#include <libstone/conf.h>
#include <libstone/database.h>
#include <libstone/util/hashtable.h>
#include <libstone/util/string.h>
#include <libstone/util/util.h>

#include "log.h"

static void st_conf_exit(void) __attribute__((destructor));
static void st_conf_free_key(void * key, void * value);
static void st_conf_init(void) __attribute__((constructor));
/**
 * \brief Load a database module
 *
 * \param[in] params : a hashtable which contains all parameters
 */
static void st_conf_load_db(const struct st_hashtable * params);
/**
 * \brief Load a log module
 *
 * \param[in] params : a hashtable which contains all parameters
 */
static void st_conf_load_log(const struct st_hashtable * params);

static struct st_hashtable * st_conf_callback = NULL;


int st_conf_check_pid(const char * prog_name, int pid) {
	if (prog_name == NULL || pid < 1) {
		if (prog_name == NULL)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: check_pid: prog_name should not be null");
		if (pid < 1)
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
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: check_pid: readlink failed '%s' => %s", link, strerror(errno));
		return 0;
	}

	char * ptr = strrchr(link, '/');
	if (ptr != NULL)
		ptr++;
	else
		ptr = link;

	int failed = strcmp(link, prog_name);
	st_log_write_all(failed ? st_log_level_warning : st_log_level_info, st_log_type_daemon, "Conf: check_pid: process 'stone' %s", failed ? "not found" : "found");
	return failed ? -1 : 1;
}

int st_conf_read_pid(const char * pid_file) {
	if (pid_file == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: read_pid: pid_file is null");
		return -1;
	}

	if (access(pid_file, R_OK)) {
		st_log_write_all(st_log_level_warning, st_log_type_daemon, "Conf: read_pid: read pid failed because cannot access to '%s'", pid_file);
		return -1;
	}

	int fd = open(pid_file, O_RDONLY);
	if (fd < 0) {
		st_log_write_all(st_log_level_warning, st_log_type_daemon, "Conf: read_pid: failed to open file '%s' because %s", pid_file, strerror(errno));
		return -1;
	}

	char buffer[16];
	ssize_t nb_read = read(fd, buffer, 15);
	close(fd);

	if (nb_read < 0) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: read_pid: error while reading file '%s' because %m", pid_file);
		return -1;
	}

	buffer[nb_read] = '\0';

	int pid = 0;
	if (sscanf(buffer, "%d", &pid) == 1) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "Conf: read_pid: pid found (%d)", pid);
		return pid;
	}

	st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: read_pid: failed to parse pid");
	return -1;
}

int st_conf_write_pid(const char * pid_file, int pid) {
	if (pid_file == NULL || pid < 1) {
		if (pid_file == NULL)
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

	ssize_t nb_write = dprintf(fd, "%d\n", pid);
	close(fd);

	st_log_write_all(nb_write > 0 ? st_log_level_info : st_log_level_warning, st_log_type_daemon, "Conf: write_pid: write %s (pid=%d)", nb_write > 0 ? "ok" : "failed", pid);

	return nb_write < 1;
}


static void st_conf_exit() {
	st_hashtable_free(st_conf_callback);
	st_conf_callback = NULL;
}

static void st_conf_free_key(void * key, void * value __attribute__((unused))) {
	free(key);
}

static void st_conf_init(void) {
	st_conf_callback = st_hashtable_new2(st_util_string_compute_hash, st_conf_free_key);

	st_hashtable_put(st_conf_callback, strdup("database"), st_hashtable_val_custom(st_conf_load_db));
	st_hashtable_put(st_conf_callback, strdup("log"), st_hashtable_val_custom( st_conf_load_log));
}

static void st_conf_load_db(const struct st_hashtable * params) {
	if (params == NULL)
		return;

	char * driver = st_hashtable_get(params, "driver").value.string;
	if (driver == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "conf: load_db: there is no driver in config file");
		return;
	}

	struct st_database * db = st_database_get_driver(driver);
	if (db != NULL) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "Conf: load_db: driver '%s' => ok", driver);

		bool setup_ok = false, ping_ok = false;
		struct st_database_config * config = db->ops->add(params);

		if (config != NULL) {
			setup_ok = true;
			ping_ok = config->ops->ping(config) >= 0 ? true : false;
		}

		st_log_write_all(setup_ok || ping_ok ? st_log_level_info : st_log_level_error, st_log_type_daemon, "Conf: load_db: setup %s, ping %s", setup_ok ? "ok" : "failed", ping_ok ? "ok" : "failed");
	} else
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: load_db: no driver '%s' found", driver);
}

static void st_conf_load_log(const struct st_hashtable * params) {
	if (params == NULL)
		return;

	char * alias = st_hashtable_get(params, "alias").value.string;
	char * type = st_hashtable_get(params, "type").value.string;
	enum st_log_level verbosity = st_log_string_to_level(st_hashtable_get(params, "verbosity").value.string);

	if (alias == NULL || type == NULL || verbosity == st_log_level_unknown) {
		if (alias == NULL)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: load_log: alias required for log");
		if (type == NULL)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: load_log: type required for log");
		if (verbosity == st_log_level_unknown)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: load_log: verbosity required for log");
		return;
	}

	struct st_log_driver * dr = st_log_get_driver(type);
	if (dr != NULL) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "Conf: load_log: using module='%s', alias='%s', verbosity='%s'", type, alias, st_log_level_to_string(verbosity));
		dr->add(alias, verbosity, params);
	} else
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: load_log: module='%s' not found", type);
}

int st_conf_read_config(const char * conf_file) {
	if (conf_file == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: read_config: conf_file is NULL");
		return -1;
	}

	if (access(conf_file, R_OK)) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: read_config: Can't access to '%s'", conf_file);
		return -1;
	}

	int fd = open(conf_file, O_RDONLY);

	struct stat st;
	fstat(fd, &st);

	char * buffer = malloc(st.st_size + 1);
	ssize_t nb_read = read(fd, buffer, st.st_size);
	close(fd);
	buffer[nb_read] = '\0';

	if (nb_read < 0) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Conf: read_config: error while reading from '%s'", conf_file);
		free(buffer);
		return 1;
	}

	char * ptr = buffer;
	char section[24] = { '\0' };
	struct st_hashtable * params = st_hashtable_new2(st_util_string_compute_hash, st_util_basic_free);

	while (ptr != NULL && *ptr) {
		switch (*ptr) {
			case ';':
				ptr = strchr(ptr, '\n');
				continue;

			case '\n':
				ptr++;
				if (*ptr == '\n') {
					if (*section) {
						st_conf_callback_f f = st_hashtable_get(st_conf_callback, section).value.custom;
						if (f != NULL)
							f(params);
					}

					*section = 0;
					st_hashtable_clear(params);
				}
				continue;

			case '[':
				sscanf(ptr, "[%23[^]]]", section);

				ptr = strchr(ptr, '\n');
				continue;

			default:
				if (!*section)
					continue;

				if (strchr(ptr, '=') < strchr(ptr, '\n')) {
					char key[32];
					char value[64];
					int val = sscanf(ptr, "%s = %s", key, value);
					if (val == 2)
						st_hashtable_put(params, strdup(key), st_hashtable_val_string(strdup(value)));
				}
				ptr = strchr(ptr, '\n');
		}
	}

	if (params->nb_elements > 0 && *section) {
		st_conf_callback_f f = st_hashtable_get(st_conf_callback, section).value.custom;
		if (f != NULL)
			f(params);
	}

	st_hashtable_free(params);
	free(buffer);

	st_log_start_logger();

	return 0;
}

void st_conf_register_callback(const char * section, st_conf_callback_f callback) {
	if (section == NULL || callback == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_daemon, "Register callback function: error because");
		if (section == NULL)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "section is NULL");
		if (callback == NULL)
			st_log_write_all(st_log_level_error, st_log_type_daemon, "callback function is NULL");
		return;
	}

	st_hashtable_put(st_conf_callback, strdup(section), st_hashtable_val_custom(callback));
}

