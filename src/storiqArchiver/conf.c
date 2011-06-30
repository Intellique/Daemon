/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Thu, 30 Jun 2011 22:37:39 +0200                       *
\***********************************************************************/

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

#include <storiqArchiver/database.h>
#include <storiqArchiver/log.h>
#include <storiqArchiver/util/hashtable.h>

#include "conf.h"
#include "util/util.h"

enum _sa_conf_section {
	_sa_conf_section_db,
	_sa_conf_section_log,
	_sa_conf_section_unknown,
};

static void _sa_conf_load_db(struct sa_hashtable * params);
static void _sa_conf_load_log(struct sa_hashtable * params);


int sa_conf_check_pid(int pid) {
	if (pid < 1) {
		sa_log_write_all(sa_log_level_error, "Conf: check_pid: pid contains a wrong value (pid=%d)", pid);
		return 0;
	}

	char path[64];
	snprintf(path, 64, "/proc/%d/exe", pid);

	if (access(path, F_OK)) {
		sa_log_write_all(sa_log_level_debug, "Conf: check_pid: there is no process with pid=%d", pid);
		return 0;
	}

	char link[128];
	if (readlink(path, link, 128) < 0) {
		sa_log_write_all(sa_log_level_error, "Conf: check_pid: readlink failed (%s) => %s", link, strerror(errno));
		return 0;
	}

	char * ptr = strrchr(link, '/');
	if (ptr)
		ptr++;
	else
		ptr = link;

	int ok = strcmp(link, "storiqArchiver");
	sa_log_write_all(sa_log_level_info, "Conf: check_pid: process 'storiqArchiver' %s", ok ? "not found" : "found");
	return ok ? -1 : 1;
}

int sa_conf_delete_pid(const char * pidFile) {
	if (!pidFile) {
		sa_log_write_all(sa_log_level_error, "Conf: delete_pid: pidFile is null");
		return 1;
	}

	int ok = unlink(pidFile);
	if (ok)
		sa_log_write_all(sa_log_level_error, "Conf: delete_pid: delete pid file => failed");
	else
		sa_log_write_all(sa_log_level_debug, "Conf: delete_pid: delete pid file => ok");
	return ok;
}

int sa_conf_read_pid(const char * pidFile) {
	if (!pidFile) {
		sa_log_write_all(sa_log_level_error, "Conf: read_pid: pidFile is null");
		return -1;
	}

	if (access(pidFile, R_OK)) {
		sa_log_write_all(sa_log_level_warning, "Conf: read_pid: read pid failed because we can read '%s'", pidFile);
		return -1;
	}

	// TODO: do some check
	int fd = open(pidFile, O_RDONLY);
	char buffer[16];
	read(fd, buffer, 16);
	close(fd);

	int pid = 0;
	if (sscanf(buffer, "%d", &pid) == 1) {
		sa_log_write_all(sa_log_level_info, "Conf: read_pid: pid found (%d)", pid);
		return pid;
	}

	sa_log_write_all(sa_log_level_warning, "Conf: read_pid: failed to parse pid");
	return -1;
}

int sa_conf_write_pid(const char * pidFile, int pid) {
	if (!pidFile || pid < 1) {
		if (!pidFile)
			sa_log_write_all(sa_log_level_debug, "Conf: write_pid: pidFile is null");
		if (pid < 1)
			sa_log_write_all(sa_log_level_debug, "Conf: write_pid: pid should be greater than 0 (pid=%d)", pid);
		return 1;
	}

	int fd = open(pidFile, O_RDONLY | O_TRUNC | O_CREAT, 0644);
	if (fd < 0)
		return 1;

	char buffer[16];
	int length = 0;
	snprintf(buffer, 16, "%d\n%n", pid, &length);

	int nbWrite = write(fd, buffer, length);
	close(fd);

	return nbWrite != length;
}


void _sa_conf_load_db(struct sa_hashtable * params) {
	if (!params)
		return;

	char * driver = sa_hashtable_value(params, "driver");
	if (!driver) {
		sa_log_write_all(sa_log_level_error, "conf: loadDB: driver not found");
		return;
	}

	struct database * db = db_getDb(driver);
	if (db) {
		sa_log_write_all(sa_log_level_info, "Conf: loadDb: loading driver (%s) => ok", driver);
		short setup_ok = !db->ops->setup(db, params);
		short ping_ok = db->ops->ping(db) > 0;
		sa_log_write_all(sa_log_level_debug, "Conf: loadDb: setup %s, ping %s", setup_ok ? "ok" : "failed", ping_ok ? "ok" : "failed");

		if (!db_getDefaultDB())
			db_setDefaultDB(db);
	} else
		sa_log_write_all(sa_log_level_error, "Conf: loadDb: loading driver (%s) => failed", driver);
}

void _sa_conf_load_log(struct sa_hashtable * params) {
	if (!params)
		return;

	char * alias = sa_hashtable_value(params, "alias");
	char * type = sa_hashtable_value(params, "type");
	enum sa_log_level verbosity = sa_log_string_to_level(sa_hashtable_value(params, "verbosity"));

	if (!alias || !type || verbosity == sa_log_level_unknown) {
		if (!alias)
			sa_log_write_all(sa_log_level_error, "Conf: loadLog: alias required for log");
		if (!type)
			sa_log_write_all(sa_log_level_error, "Conf: loadLog: type required for log");
		if (verbosity == sa_log_level_unknown)
			sa_log_write_all(sa_log_level_error, "Conf: loadLog: verbosity required for log");
		return;
	}

	struct sa_log_driver * dr = sa_log_get_driver(type);
	if (dr) {
		sa_log_write_all(sa_log_level_info, "Conf: loadLog: using module='%s', alias='%s', verbosity='%s'", type, alias, sa_log_level_to_string(verbosity));
		dr->add(dr, alias, verbosity, params);
	} else
		sa_log_write_all(sa_log_level_error, "Conf: loadLog: module='%s' not found", type);
}

int sa_conf_read_config(const char * confFile) {
	if (access(confFile, R_OK)) {
		sa_log_write_all(sa_log_level_error, "Conf: readConfig: Can't access to '%s'", confFile);
		return -1;
	}

	int fd = open(confFile, O_RDONLY);

	struct stat st;
	fstat(fd, &st);

	char * buffer = malloc(st.st_size);
	read(fd, buffer, st.st_size);
	close(fd);

	char * ptr = buffer;
	enum _sa_conf_section section = _sa_conf_section_unknown;
	struct sa_hashtable * params = sa_hashtable_new2(util_hashString, util_freeKeyValue);
	while (ptr) {
		switch (*ptr) {
			case ';':
				ptr = strchr(ptr, '\n');
				continue;

			case '\n':
				ptr++;
				if (*ptr == '\n') {
					switch (section) {
						case _sa_conf_section_db:
							_sa_conf_load_db(params);
							break;

						case _sa_conf_section_log:
							_sa_conf_load_log(params);
							break;

						default:
							break;
					}

					sa_hashtable_clear(params);
				}
				continue;

			case '[':
				if (strlen(ptr + 1) > 8 && !strncmp(ptr + 1, "database", 8))
					section = _sa_conf_section_db;
				else if (strlen(ptr + 1) > 3 && !strncmp(ptr + 1, "log", 3))
					section = _sa_conf_section_log;
				else
					section = _sa_conf_section_unknown;
				ptr = strchr(ptr, '\n');
				continue;

			default:
				if (section == _sa_conf_section_unknown)
					continue;

				if (strchr(ptr, '=') < strchr(ptr, '\n')) {
					char key[32];
					char value[64];
					int val = sscanf(ptr, "%s = %s", key, value);
					if (val == 2)
						sa_hashtable_put(params, strdup(key), strdup(value));
				}
				ptr = strchr(ptr, '\n');
		}
	}

	if (params->nbElements > 0) {
		switch (section) {
			case _sa_conf_section_db:
				_sa_conf_load_db(params);
				break;

			case _sa_conf_section_log:
				_sa_conf_load_log(params);
				break;

			default:
				break;
		}
	}

	sa_hashtable_free(params);
	free(buffer);

	return 0;
}

