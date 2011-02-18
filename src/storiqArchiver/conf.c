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
*  Last modified: Sun, 06 Feb 2011 22:56:01 +0100                       *
\***********************************************************************/

// strerror
#include <errno.h>
// open
#include <fcntl.h>
// free, malloc
#include <malloc.h>
// snprintf, sscanf
#include <stdio.h>
// strcmp, strerror, strlen, strncmp, strrchr
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// access, close, read, readlink, unlink, write
#include <unistd.h>

#include <storiqArchiver/database.h>
#include <storiqArchiver/log.h>
#include <storiqArchiver/util/hashtable.h>

#include "conf.h"
#include "util/util.h"

enum conf_section {
	conf_section_db,
	conf_section_log,
	conf_section_unknown,
};

static void conf_loadDb(struct hashtable * params);
static void conf_loadLog(struct hashtable * params);


int conf_checkPid(int pid) {
	if (pid < 1) {
		log_writeAll(Log_level_error, "Conf: checkPid: pid contains a wrong value (pid=%d)", pid);
		return 0;
	}

	char path[64];
	snprintf(path, 64, "/proc/%d/exe", pid);

	if (access(path, F_OK)) {
		log_writeAll(Log_level_debug, "Conf: checkPid: there is no process with pid=%d", pid);
		return 0;
	}

	char link[128];
	if (readlink(path, link, 128) < 0) {
		log_writeAll(Log_level_error, "Conf: checkPid: readlink failed (%s) => %s", link, strerror(errno));
		return 0;
	}

	char * ptr = strrchr(link, '/');
	if (ptr)
		ptr++;
	else
		ptr = link;

	int ok = strcmp(link, "storiqArchiver");
	log_writeAll(Log_level_info, "Conf: checkPid: process 'storiqArchiver' %s", ok ? "not found" : "found");
	return ok ? -1 : 1;
}

int conf_deletePid(const char * pidFile) {
	if (!pidFile) {
		log_writeAll(Log_level_error, "Conf: deletePid: pidFile is null");
		return 1;
	}

	int ok = unlink(pidFile);
	if (ok)
		log_writeAll(Log_level_error, "Conf: deletePid: delete pid file => failed");
	else
		log_writeAll(Log_level_debug, "Conf: deletePid: delete pid file => ok");
	return ok;
}

int conf_readPid(const char * pidFile) {
	if (!pidFile) {
		log_writeAll(Log_level_error, "Conf: readPid: pidFile is null");
		return -1;
	}

	if (access(pidFile, R_OK)) {
		log_writeAll(Log_level_warning, "Conf: readPid: read pid failed because we can read '%s'", pidFile);
		return -1;
	}

	// TODO: do some check
	int fd = open(pidFile, O_RDONLY);
	char buffer[16];
	read(fd, buffer, 16);
	close(fd);

	int pid = 0;
	if (sscanf(buffer, "%d", &pid) == 1) {
		log_writeAll(Log_level_info, "Conf: readPid: pid found (%d)", pid);
		return pid;
	}

	log_writeAll(Log_level_warning, "Conf: readPid: failed to parse pid");
	return -1;
}

int conf_writePid(const char * pidFile, int pid) {
	if (!pidFile || pid < 1) {
		if (!pidFile)
			log_writeAll(Log_level_debug, "Conf: writePid: pidFile is null");
		if (pid < 1)
			log_writeAll(Log_level_debug, "Conf: writePid: pid should be greater than 0 (pid=%d)", pid);
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


void conf_loadDb(struct hashtable * params) {
	if (!params)
		return;

	char * driver = hashtable_value(params, "driver");
	if (!driver) {
		log_writeAll(Log_level_error, "conf: loadDB: driver not found");
		return;
	}

	struct database * db = db_getDb(driver);
	if (db) {
		log_writeAll(Log_level_info, "Conf: loadDb: loading driver (%s) => ok", driver);
		short setup_ok = !db->ops->setup(db, params);
		short ping_ok = db->ops->ping(db) > 0;
		log_writeAll(Log_level_debug, "Conf: loadDb: setup %s, ping %s", setup_ok ? "ok" : "failed", ping_ok ? "ok" : "failed");

		if (!db_getDefaultDB())
			db_setDefaultDB(db);
	} else
		log_writeAll(Log_level_error, "Conf: loadDb: loading driver (%s) => failed", driver);
}

void conf_loadLog(struct hashtable * params) {
	if (!params)
		return;

	char * alias = hashtable_value(params, "alias");
	char * type = hashtable_value(params, "type");
	enum Log_level verbosity = log_stringTolevel(hashtable_value(params, "verbosity"));

	if (!alias || !type || verbosity == Log_level_unknown) {
		if (!alias)
			log_writeAll(Log_level_error, "Conf: loadLog: alias required for log");
		if (!type)
			log_writeAll(Log_level_error, "Conf: loadLog: type required for log");
		if (verbosity == Log_level_unknown)
			log_writeAll(Log_level_error, "Conf: loadLog: verbosity required for log");
		return;
	}

	struct log_module * mod = log_getModule(type);
	if (mod) {
		log_writeAll(Log_level_info, "Conf: loadLog: using module='%s', alias='%s', verbosity='%s'", type, alias, log_levelToString(verbosity));
		mod->add(mod, alias, verbosity, params);
	} else
		log_writeAll(Log_level_error, "Conf: loadLog: module='%s' not found", type);
}

int conf_readConfig(const char * confFile) {
	if (access(confFile, R_OK)) {
		log_writeAll(Log_level_error, "Conf: readConfig: Can't access to '%s'", confFile);
		return -1;
	}

	int fd = open(confFile, O_RDONLY);

	struct stat st;
	fstat(fd, &st);

	char * buffer = malloc(st.st_size);
	read(fd, buffer, st.st_size);
	close(fd);

	char * ptr = buffer;
	enum conf_section section = conf_section_unknown;
	struct hashtable * params = hashtable_new2(util_hashString, util_freeKeyValue);
	while (ptr) {
		switch (*ptr) {
			case ';':
				ptr = strchr(ptr, '\n');
				continue;

			case '\n':
				ptr++;
				if (*ptr == '\n') {
					switch (section) {
						case conf_section_db:
							conf_loadDb(params);
							break;

						case conf_section_log:
							conf_loadLog(params);
							break;

						default:
							break;
					}

					hashtable_clear(params);
				}
				continue;

			case '[':
				if (strlen(ptr + 1) > 8 && !strncmp(ptr + 1, "database", 8))
					section = conf_section_db;
				else if (strlen(ptr + 1) > 3 && !strncmp(ptr + 1, "log", 3))
					section = conf_section_log;
				else
					section = conf_section_unknown;
				ptr = strchr(ptr, '\n');
				continue;

			default:
				if (section == conf_section_unknown)
					continue;

				if (strchr(ptr, '=') < strchr(ptr, '\n')) {
					char key[32];
					char value[64];
					int val = sscanf(ptr, "%s = %s", key, value);
					if (val == 2)
						hashtable_put(params, strdup(key), strdup(value));
				}
				ptr = strchr(ptr, '\n');
		}
	}

	if (params->nbElements > 0) {
		switch (section) {
			case conf_section_db:
				conf_loadDb(params);
				break;

			case conf_section_log:
				conf_loadLog(params);
				break;

			default:
				break;
		}
	}

	hashtable_free(params);
	free(buffer);

	return 0;
}

