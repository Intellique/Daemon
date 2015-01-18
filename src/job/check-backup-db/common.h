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

#ifndef __STORIQONE_JOB_CHECKBACKUP_COMMON_H__
#define __STORIQONE_JOB_CHECKBACKUP_COMMON_H__

// bool
#include <stdbool.h>
// size_t
#include <sys/types.h>

struct so_database_config;

struct soj_checkbackupdb_worker {
	struct so_backup * backup;
	struct so_backup_volume * volume;

	size_t position;
	size_t size;

	volatile bool working;

	struct so_database_config * db_config;

	struct soj_checkbackupdb_worker * next;
};

void soj_checkbackupdb_worker_do(void * arg);
void soj_checkbackupdb_worker_free(struct soj_checkbackupdb_worker * worker);
struct soj_checkbackupdb_worker * soj_checkbackupdb_worker_new(struct so_backup * backup, struct so_backup_volume * volume, size_t size, struct so_database_config * config, struct soj_checkbackupdb_worker * previous_worker);

#endif

