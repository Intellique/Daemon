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
*  Copyright (C) 2013-2018, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// dgettext
#include <libintl.h>
// NULL
#include <stddef.h>
// struct mtget
#include <sys/mtio.h>

#include <libstoriqone/database.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/slot.h>
#include <libstoriqone-drive/log.h>
#include <libstoriqone-drive/time.h>

#include "st.h"

int sodr_tape_drive_st_get_position(struct so_drive * drive, int fd, struct so_database_connection * db) {
	struct mtget status;
	int failed = sodr_tape_drive_st_get_status(drive, fd, &status, db);
	if (failed < 0)
		return -1;

	return status.mt_fileno;
}

int sodr_tape_drive_st_get_status(struct so_drive * drive, int fd, struct mtget * status, struct so_database_connection * db) {
	sodr_time_start();
	int failed = ioctl(fd, MTIOCGET, status);
	sodr_time_stop(drive);

	if (failed != 0) {
		struct so_media * media = drive->slot->media;

		sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Failed to get information of media '%s'"),
			drive->vendor, drive->model, drive->index, media->name);
	}

	return failed;
}

int sodr_tape_drive_st_mk_1_partition(struct so_drive * drive, int fd) {
	struct so_media * media = drive->slot->media;
	if (media == NULL)
		return -1;

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

		return failed;
	} else
		so_log_write(so_log_level_debug,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Succeed to format media '%s' with one partition"),
			drive->vendor, drive->model, drive->index, media->name);

	return failed;
}

int sodr_tape_drive_st_rewind(struct so_drive * drive, int fd, struct so_database_connection * db) {
	drive->status = so_drive_status_rewinding;
	if (db != NULL)
		db->ops->sync_drive(db, drive, true, so_database_sync_default);

	static struct mtop rewind = { MTREW, 1 };
	sodr_time_start();
	int failed = ioctl(fd, MTIOCTOP, &rewind);
	sodr_time_stop(drive);

	drive->status = failed != 0 ? so_drive_status_error : so_drive_status_reading;
	if (db != NULL)
		db->ops->sync_drive(db, drive, true, so_database_sync_default);

	return failed;
}

int sodr_tape_drive_st_set_can_partition(struct so_drive * drive, int fd, struct so_database_connection * db) {
	const struct mtop set_can_parition = { MTSETDRVBUFFER, MT_ST_CAN_PARTITIONS | MT_ST_SETBOOLEANS };

	sodr_log_add_record(so_job_status_running, db, so_log_level_info, so_job_record_notif_normal,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Set option to 'st' driver 'can-partition'"),
		drive->vendor, drive->model, drive->index);

	sodr_time_start();
	int failed = ioctl(fd, MTIOCTOP, &set_can_parition);
	sodr_time_stop(drive);

	if (failed != 0)
		sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Failed to set option to 'st' driver 'can-partition'"),
			drive->vendor, drive->model, drive->index);
	else
		sodr_log_add_record(so_job_status_running, db, so_log_level_info, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Set option to 'st' driver 'can-partition' with success"),
			drive->vendor, drive->model, drive->index);

	return failed;
}

int sodr_tape_drive_st_set_position(struct so_drive * drive, int fd, unsigned int partition, int file_number, bool force, struct so_database_connection * db) {
	struct so_media * media = drive->slot->media;

	struct mtget status;
	int failed = sodr_tape_drive_st_get_status(drive, fd, &status, db);

	if (force || partition != status.mt_resid) {
		if (media != NULL)
			sodr_log_add_record(so_job_status_running, db, so_log_level_info, so_job_record_notif_normal,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Changing partition from %lu to %u on media '%s'"),
				drive->vendor, drive->model, drive->index, status.mt_resid, partition, media->name);
		else
			sodr_log_add_record(so_job_status_running, db, so_log_level_info, so_job_record_notif_normal,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Changing partition from %lu to %u"),
				drive->vendor, drive->model, drive->index, status.mt_resid, partition);

		drive->status = so_drive_status_positioning;
		if (db != NULL)
			db->ops->sync_drive(db, drive, true, so_database_sync_default);

		struct mtop change_partition = { MTSETPART, partition };
		sodr_time_start();
		failed = ioctl(fd, MTIOCTOP, &change_partition);
		sodr_time_stop(drive);

		drive->status = failed ? so_drive_status_error : so_drive_status_loaded_idle;
		if (db != NULL)
			db->ops->sync_drive(db, drive, true, so_database_sync_default);

		if (failed != 0) {
			if (media != NULL)
				sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Failed to change partition from %lu to %u on media '%s' because %m"),
					drive->vendor, drive->model, drive->index, status.mt_resid, partition, media->name);
			else
				sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Failed to change partition from %lu to %u because %m"),
					drive->vendor, drive->model, drive->index, status.mt_resid, partition);

			return failed;
		} else if (media != NULL)
			sodr_log_add_record(so_job_status_running, db, so_log_level_info, so_job_record_notif_normal,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Partition changed from %lu to %u on media '%s'"),
				drive->vendor, drive->model, drive->index, status.mt_resid, partition, media->name);
		else
			sodr_log_add_record(so_job_status_running, db, so_log_level_info, so_job_record_notif_normal,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Partition changed from %lu to %u"),
				drive->vendor, drive->model, drive->index, status.mt_resid, partition);

		failed = sodr_tape_drive_st_rewind(drive, fd, db);
		if (failed != 0)
			return failed;

		failed = sodr_tape_drive_st_get_status(drive, fd, &status, db);
		if (failed != 0)
			return failed;
	}

	if (status.mt_fileno < 0) {
		if (media != NULL)
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Unkown media position '%s', force rewinding"),
				drive->vendor, drive->model, drive->index, drive->slot->media->name);
		else
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Unkown media position, force rewinding"),
				drive->vendor, drive->model, drive->index);

		failed = sodr_tape_drive_st_rewind(drive, fd, db);
		if (failed != 0)
			return failed;

		failed = sodr_tape_drive_st_get_status(drive, fd, &status, db);
		if (failed != 0)
			return failed;
	}

	if (file_number < 0) {
		if (media != NULL)
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Fast forwarding to end of media '%s'"),
				drive->vendor, drive->model, drive->index, drive->slot->media->name);
		else
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Fast forwarding to end"),
				drive->vendor, drive->model, drive->index);

		drive->status = so_drive_status_positioning;
		if (db != NULL)
			db->ops->sync_drive(db, drive, true, so_database_sync_default);

		static struct mtop eod = { MTEOM, 1 };
		sodr_time_start();
		failed = ioctl(fd, MTIOCTOP, &eod);
		sodr_time_stop(drive);

		drive->status = failed ? so_drive_status_error : so_drive_status_loaded_idle;
		if (db != NULL)
			db->ops->sync_drive(db, drive, true, so_database_sync_default);

		if (failed != 0) {
			if (media != NULL)
				so_log_write(so_log_level_error,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Fast forwarding to end of media '%s' encountered an error '%m'"),
					drive->vendor, drive->model, drive->index, drive->slot->media->name);
			else
				so_log_write(so_log_level_error,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Fast forwarding to end encountered an error '%m'"),
					drive->vendor, drive->model, drive->index);
		} else if (media != NULL)
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Fast forwarding to end of media '%s' completed with status = OK"),
				drive->vendor, drive->model, drive->index, drive->slot->media->name);
		else
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Fast forwarding to end completed with status = OK"),
				drive->vendor, drive->model, drive->index);
	} else if (status.mt_fileno < file_number) {
		if (media != NULL)
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' from #%u to position #%d"),
				drive->vendor, drive->model, drive->index, drive->slot->media->name, status.mt_fileno, file_number);
		else
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning from #%u to position #%d"),
				drive->vendor, drive->model, drive->index, status.mt_fileno, file_number);

		drive->status = so_drive_status_positioning;
		if (db != NULL)
			db->ops->sync_drive(db, drive, true, so_database_sync_default);

		struct mtop fsr = { MTFSF, file_number - status.mt_fileno };
		sodr_time_start();
		failed = ioctl(fd, MTIOCTOP, &fsr);
		sodr_time_stop(drive);

		drive->status = failed ? so_drive_status_error : so_drive_status_loaded_idle;
		if (db != NULL)
			db->ops->sync_drive(db, drive, true, so_database_sync_default);

		if (failed != 0) {
			if (media != NULL)
				so_log_write(so_log_level_error,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' from #%u to position #%d encountered an error '%m'"),
					drive->vendor, drive->model, drive->index, drive->slot->media->name, status.mt_fileno, file_number);
			else
				so_log_write(so_log_level_error,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning from #%u to position #%d encountered an error '%m'"),
					drive->vendor, drive->model, drive->index, status.mt_fileno, file_number);
		} else if (media != NULL)
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' from #%u to position #%d completed with status = OK"),
				drive->vendor, drive->model, drive->index, drive->slot->media->name, status.mt_fileno, file_number);
		else
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning from #%u to position #%d completed with status = OK"),
				drive->vendor, drive->model, drive->index, status.mt_fileno, file_number);
	} else if (status.mt_fileno > file_number) {
		if (file_number == 0) {
			failed = sodr_tape_drive_st_rewind(drive, fd, db);
		} else {
			if (media != NULL)
				so_log_write(so_log_level_info,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' from #%u to position #%d"),
					drive->vendor, drive->model, drive->index, drive->slot->media->name, status.mt_fileno, file_number);
			else
				so_log_write(so_log_level_info,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning from #%u to position #%d"),
					drive->vendor, drive->model, drive->index, status.mt_fileno, file_number);

			drive->status = so_drive_status_positioning;
			if (db != NULL)
				db->ops->sync_drive(db, drive, true, so_database_sync_default);

			struct mtop bsfm = { MTBSFM, status.mt_fileno - file_number + 1 };
			sodr_time_start();
			failed = ioctl(fd, MTIOCTOP, &bsfm);
			sodr_time_stop(drive);

			drive->status = failed ? so_drive_status_error : so_drive_status_loaded_idle;
			if (db != NULL)
				db->ops->sync_drive(db, drive, true, so_database_sync_default);

			if (failed != 0) {
				if (media != NULL)
					so_log_write(so_log_level_error,
						dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' from #%u to position #%d encountered an error '%m'"),
						drive->vendor, drive->model, drive->index, drive->slot->media->name, status.mt_fileno, file_number);
				else
					so_log_write(so_log_level_error,
						dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning from #%u to position #%d encountered an error '%m'"),
						drive->vendor, drive->model, drive->index, status.mt_fileno, file_number);
			} else if (media != NULL)
				so_log_write(so_log_level_info,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' from #%u to position #%d completed with status = OK"),
					drive->vendor, drive->model, drive->index, drive->slot->media->name, status.mt_fileno, file_number);
			else
				so_log_write(so_log_level_info,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning from #%u to position #%d completed with status = OK"),
					drive->vendor, drive->model, drive->index, status.mt_fileno, file_number);
		}
	}

	return failed;
}

int sodr_tape_drive_st_write_end_of_file(struct so_drive * drive, int fd) {
	static struct mtop eof = { MTWEOF, 1 };

	sodr_time_start();
	int failed = ioctl(fd, MTIOCTOP, &eof);
	sodr_time_stop(drive);

	if (failed != 0) {
		so_log_write(so_log_level_error,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Error while closing file because '%m'"),
			drive->vendor, drive->model, drive->index);
	}

	return failed;
}
