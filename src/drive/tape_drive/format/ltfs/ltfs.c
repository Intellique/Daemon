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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// gettext
#include <libintl.h>
// snprintf
#include <stdio.h>
// struct mtget
#include <sys/mtio.h>
// write
#include <unistd.h>

#include <libstoriqone/database.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/file.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-drive/time.h>

#include "ltfs.h"
#include "../../st.h"

int sodr_tape_drive_format_ltfs_format_media(struct so_drive * drive, int fd, struct so_value * option, struct so_database_connection * db) {
	ssize_t block_size = 0, partition_size = 0;
	so_value_unpack(option, "{sz}", "block size", &block_size);
	so_value_unpack(option, "{sz}", "partition size", &partition_size);

	if (partition_size == 0)
		partition_size = 524288;

	static const struct default_parition_size {
		unsigned char density_code;
		unsigned long size;
	} default_parition_sizes[] = {
		{ 0x58, 1424000L << 20 },
		{ 0x5A, 2450000L << 20 },
	};
	static const unsigned int nb_default_parition_size = sizeof(default_parition_sizes) / sizeof(*default_parition_sizes);

	struct so_media * media = drive->slot->media;

	if (partition_size == 0) {
		unsigned int i;
		for (i = 0; i < nb_default_parition_size; i++)
			if (media->media_format->density_code == default_parition_sizes[i].density_code) {
				partition_size = default_parition_sizes[i].size;
				break;
			}
	}

	if (partition_size == 0)
		return -1;

	drive->status = so_drive_status_writing;
	db->ops->sync_drive(db, drive, true, so_database_sync_default);

	so_log_write(so_log_level_info,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Formatting media '%s' info LTFS format"),
		drive->vendor, drive->model, drive->index, media->name);

	so_log_write(so_log_level_debug,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Formatting media '%s' with one partition"),
		drive->vendor, drive->model, drive->index, media->name);

	static struct mtop mk1partition = { MTMKPART, 0 };
	sodr_time_start();
	int failed = ioctl(fd, MTIOCTOP, &mk1partition);
	sodr_time_stop(drive);

	if (failed != 0) {
		so_log_write(so_log_level_error,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Failed to Format media '%s' with one partition because %m"),
			drive->vendor, drive->model, drive->index, media->name);

			media->status = so_media_status_error;
			db->ops->sync_drive(db, drive, true, so_database_sync_default);

		return failed;
	} else
		so_log_write(so_log_level_debug,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Succeed to format media '%s' with one partition"),
			drive->vendor, drive->model, drive->index, media->name);


	char buf_size[16];
	so_file_convert_size_to_string(partition_size, buf_size, 16);

	so_log_write(so_log_level_notice,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Formatting media '%s' with two partitions (second partition size: %s)"),
		drive->vendor, drive->model, drive->index, media->name, buf_size);

	// second parameter is in MB
	struct mtop mk2partition = { MTMKPART, partition_size >> 20 };
	sodr_time_start();
	failed = ioctl(fd, MTIOCTOP, &mk2partition);
	sodr_time_stop(drive);

	if (failed != 0) {
		so_log_write(so_log_level_error,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Failed to Format media '%s' with two partition because %m"),
			drive->vendor, drive->model, drive->index, media->name);

			media->status = so_media_status_error;
			db->ops->sync_drive(db, drive, true, so_database_sync_default);

		return failed;
	} else
		so_log_write(so_log_level_debug,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Succeed to format media '%s' with two partition"),
			drive->vendor, drive->model, drive->index, media->name);

	/**
	 * Write volume label
	 */
	char label[81];
	snprintf(label, 81, "VOL1%6sL             LTFS                                                   4", media->label != NULL ? media->label : "");

	failed = sodr_tape_drive_st_set_position(drive, fd, 0, 0, NULL, db);

	ssize_t nb_write = write(fd, label, 80);
	if (nb_write < 0) {
		so_log_write(so_log_level_debug,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Error while writing label on first partition because %m"),
			drive->vendor, drive->model, drive->index);

		return nb_write;
	}

	return 0;
}

