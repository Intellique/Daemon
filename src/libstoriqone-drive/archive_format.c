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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#include <libstoriqone/database.h>
#include <libstoriqone-drive/archive_format.h>

static struct so_archive_format sodr_format_storiq_one = {
	.name     = "Storiq One",
	.readable = true,
	.writable = true,
};

int sodr_archive_format_sync(struct so_archive_format * formats, unsigned int nb_formats, struct so_database_connection * db_connect) {
	int failed = db_connect->ops->sync_archive_format(db_connect, &sodr_format_storiq_one, 1);
	if (failed != 0)
		return failed;

	if (nb_formats > 0)
		return db_connect->ops->sync_archive_format(db_connect, formats, nb_formats);

	return 0;
}

