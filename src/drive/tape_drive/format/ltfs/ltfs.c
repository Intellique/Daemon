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
// strcpy, strcspn, strlen
#include <string.h>
// struct mtget
#include <sys/mtio.h>
// now
#include <time.h>
// write
#include <unistd.h>
// uuid
#include <uuid/uuid.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/file.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-drive/time.h>

#include "ltfs.h"
#include "../../media.h"
#include "../../io/io.h"
#include "../../util/scsi.h"
#include "../../util/st.h"
#include "../../util/xml.h"

static int sodr_tape_drive_format_ltfs_format_media_partition(struct so_drive * drive, struct so_stream_writer * writer, int fd, ssize_t block_size, const char * partition, time_t format_time, const char * uuid);

int sodr_tape_drive_format_ltfs_format_media(struct so_drive * drive, int fd, int scsi_fd, struct so_pool * pool, struct so_value * option, struct so_database_connection * db) {
	ssize_t block_size = 0, partition_size = 0;
	so_value_unpack(option, "{sz}", "block size", &block_size);
	so_value_unpack(option, "{sz}", "partition size", &partition_size);

	if (block_size == 0)
		block_size = 524288;

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


	time_t format_time = time(NULL);

	uuid_t raw_uuid;
	uuid_generate(raw_uuid);

	char uuid[37];
	uuid_unparse_lower(raw_uuid, uuid);

	struct so_stream_writer * writer = sodr_tape_drive_writer_get_raw_writer2(drive, fd, 1, 0, false, db);
	if (writer == NULL)
		return 1;

	/**
	 * Write volume label on second partition
	 */
	failed = sodr_tape_drive_format_ltfs_format_media_partition(drive, writer, fd, block_size, "b", format_time, uuid);
	if (failed != 0)
		return failed;

	failed = writer->ops->create_new_file(writer);
	if (failed != 0)
		return failed;

	struct sodr_tape_drive_scsi_position position;
	failed = sodr_tape_drive_scsi_read_position(scsi_fd, &position);

	struct so_value * empty_fs = sodr_tape_drive_format_ltfs_create_empty_fs(format_time, uuid, "b", position.block_position, NULL, 0);
	ssize_t nb_write = sodr_tape_drive_xml_encode_stream(writer, empty_fs);
	if (nb_write < 0)
		return failed;

	sodr_tape_drive_writer_close2(writer, false);
	writer->ops->free(writer);

	so_value_free(empty_fs);

	writer = sodr_tape_drive_writer_get_raw_writer2(drive, fd, 0, 0, false, db);

	/**
	 * Write volume label on first partition
	 */
	failed = sodr_tape_drive_format_ltfs_format_media_partition(drive, writer, fd, block_size, "a", format_time, uuid);

	long long int previous_position = position.block_position;
	failed = sodr_tape_drive_scsi_read_position(scsi_fd, &position);
	empty_fs = sodr_tape_drive_format_ltfs_create_empty_fs(format_time, uuid, "a", position.block_position, "b", previous_position);
	nb_write = sodr_tape_drive_xml_encode_stream(writer, empty_fs);
	if (nb_write < 0)
		return failed;

	sodr_tape_drive_writer_close2(writer, false);
	writer->ops->free(writer);

	drive->status = failed != 0 ? so_drive_status_error : so_drive_status_loaded_idle;
	db->ops->sync_drive(db, drive, true, so_database_sync_default);

	if (failed == 0)
		media->archive_format = db->ops->get_archive_format_by_name(db, pool->archive_format->name);

	/**
	 * Update Medium auxiliary memory
	 */
	failed = sodr_tape_drive_format_ltfs_update_mam(scsi_fd, drive, db);
	if (failed != 0)
		return failed;

	unsigned int vcr = 0;
	failed = sodr_tape_drive_scsi_read_volume_change_reference(scsi_fd, &vcr);
	if (failed != 0)
		return failed;

	struct sodr_tape_drive_media * mp = sodr_tape_drive_media_new(sodr_tape_drive_media_ltfs);
	struct sodr_tape_drive_format_ltfs * ltfs = &mp->data.ltfs;

	ltfs->highest_file_uid = 1;

	ltfs->root.file_uid = 1;

	ltfs->index.volume_change_reference = vcr;
	ltfs->index.generation_number = 1;
	ltfs->index.block_position_of_last_index = position.block_position;
	failed = sodr_tape_drive_format_ltfs_update_volume_coherency_info(scsi_fd, drive, uuid, 0, &ltfs->index, db);
	if (failed != 0) {
		sodr_tape_drive_media_free(mp);
		return failed;
	}

	ltfs->data.volume_change_reference = vcr;
	ltfs->data.generation_number = 1;
	ltfs->data.block_position_of_last_index = previous_position;
	failed = sodr_tape_drive_format_ltfs_update_volume_coherency_info(scsi_fd, drive, uuid, 1, &ltfs->data, db);
	if (failed != 0) {
		sodr_tape_drive_media_free(mp);
		return failed;
	}

	ltfs->up_to_date = true;


	strcpy(media->uuid, uuid);
	media->status = so_media_status_in_use;
	media->last_write = time(NULL);
	media->write_count++;
	media->operation_count++;
	media->nb_total_write++;
	media->block_size = block_size;
	media->pool = pool;
	media->private_data = mp;
	media->free_private_data = sodr_tape_drive_media_free2;

	return 0;
}

static int sodr_tape_drive_format_ltfs_format_media_partition(struct so_drive * drive, struct so_stream_writer * writer, int fd, ssize_t block_size, const char * partition, time_t format_time, const char * uuid) {
	struct so_media * media = drive->slot->media;

	char label[81];
	snprintf(label, 81, "VOL1%6sL             LTFS                                                   4", media->label != NULL ? media->label : "");

	ssize_t nb_write = writer->ops->write(writer, label, 80);
	if (nb_write < 0) {
		so_log_write(so_log_level_debug,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Error while writing label on partition '%s' because %m"),
			drive->vendor, drive->model, drive->index, partition);

		return nb_write;
	}

	int failed = sodr_tape_drive_writer_close2(writer, false);
	if (failed != 0)
		return failed;

	struct so_value * ltfs_label = sodr_tape_drive_format_ltfs_create_label(format_time, uuid, block_size, partition);

	failed = writer->ops->create_new_file(writer);
	if (failed != 0)
		return failed;

	nb_write = sodr_tape_drive_xml_encode_stream(writer, ltfs_label);
	if (nb_write < 0)
		return 1;

	failed = sodr_tape_drive_writer_close2(writer, false);

	if (failed != 0)
		return failed;

	failed = sodr_tape_drive_st_write_end_of_file(drive, fd);

	so_value_free(ltfs_label);

	return 0;
}

