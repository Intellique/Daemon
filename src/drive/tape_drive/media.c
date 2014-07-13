/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

#include <stddef.h>

#include <libstone/media.h>
#include <libstone/slot.h>
#include <libstone-drive/drive.h>

#include "media.h"

bool tape_drive_media_read_header(struct st_drive * drive, struct st_database_connection * db) {
	struct st_media * media = drive->slot->media;
	if (media == NULL)
		return false;

	/*
	struct st_stream_reader * reader = drive->ops->get_raw_reader(drive, 0);
	if (reader == NULL) {
		st_log_write_all(st_log_level_info, st_log_type_daemon, "[%s | %s | #%td]: failed to read media", drive->vendor, drive->model, drive - drive->changer->drives);
		return 1;
	}

	char buffer[512];
	ssize_t nb_read = reader->ops->read(reader, buffer, 512);
	reader->ops->close(reader);
	reader->ops->free(reader);

	if (nb_read <= 0) {
		st_log_write_all(st_log_level_warning, st_log_type_daemon, "[%s | %s | #%td]: media has no header", drive->vendor, drive->model, drive - drive->changer->drives);
		return false;
	}

	char stone_version[65];
	int tape_format_version = 0;
	int nb_parsed = 0;
	bool ok = false;

	if (sscanf(buffer, "STone (%64[^)])\nTape format: version=%d\n%n", stone_version, &tape_format_version, &nb_parsed) == 2) {
		switch (tape_format_version) {
			case 1:
				ok = st_media_read_header_v1(media, buffer, nb_parsed, true);
				break;

			case 2:
				ok = st_media_read_header_v2(media, buffer, nb_parsed, true);
				break;
		}
	}

	return ok;
	*/

	return false;
}

