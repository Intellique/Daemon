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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// va_end, va_start
#include <stdarg.h>
// vasprintf
#include <stdio.h>
// free
#include <stdlib.h>

#include <libstoriqone/database.h>
#include <libstoriqone-drive/log.h>

#include "peer.h"

int sodr_log_add_record(enum so_job_status status, struct so_database_connection * db_connect, enum so_log_level level, enum so_job_record_notif notif, const char * format, ...) {
	char * message = NULL;
	struct sodr_peer * peer = sodr_peer_get();

	va_list va;
	va_start(va, format);
	int size = vasprintf(&message, format, va);
	va_end(va);

	if (size < 0)
		return -1;

	so_log_write(level, "%s", message);

	int failed = 0;

	if (peer != NULL && db_connect != NULL)
		failed = db_connect->ops->add_job_record2(db_connect, peer->job_id, peer->job_num_run, status, level, notif, message);

	free(message);

	return failed;
}
