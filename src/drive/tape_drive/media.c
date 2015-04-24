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

// dgettext
#include <libintl.h>
// sscanf
#include <stdio.h>

#include <libstoriqone/log.h>

#include "media.h"

static bool sodr_tape_drive_media_check_ltfs_header(struct so_media * media, const char * buffer);

bool sodr_tape_drive_media_check_header(struct so_media * media, const char * buffer) {
	return sodr_tape_drive_media_check_ltfs_header(media, buffer);
}

static bool sodr_tape_drive_media_check_ltfs_header(struct so_media * media, const char * buffer) {
	char vol_id[7];
	int nb_params = sscanf(buffer, "VOL1%6sL             LTFS", vol_id);
	if (nb_params < 1)
		return false;

	so_log_write(so_log_level_debug,
		dgettext("storiqone-drive-tape", "Found LTFS header in media with (volume id: %s)"),
		vol_id);

	// TODO: add media to special LTFS pool

	return true;
}

