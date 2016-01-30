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

int sodr_tape_drive_st_get_status(struct so_drive * drive, int fd, struct mtget * status, struct sodr_peer * peer, struct so_database_connection * db) {
	sodr_time_start();
	int failed = ioctl(fd, MTIOCGET, status);
	sodr_time_stop(drive);

	if (failed != 0) {
		struct so_media * media = drive->slot->media;

		sodr_log_add_record(peer, so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Failed to get information of media '%s'"),
			drive->vendor, drive->model, drive->index, media->name);
	}

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

int sodr_tape_drive_st_set_position(struct so_drive * drive, int fd, unsigned int partition, int file_number, struct sodr_peer * peer, struct so_database_connection * db) {
	struct so_media * media = drive->slot->media;

	struct mtget status;
	int failed = sodr_tape_drive_st_get_status(drive, fd, &status, peer, db);

	if (partition != status.mt_resid) {
		sodr_log_add_record(peer, so_job_status_running, db, so_log_level_info, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Changing partion from %lu to %u on media '%s'"),
			drive->vendor, drive->model, drive->index, status.mt_resid, partition, media->name);
		drive->status = so_drive_status_positioning;
		db->ops->sync_drive(db, drive, true, so_database_sync_default);

		struct mtop change_partition = { MTSETPART, partition };
		sodr_time_start();
		failed = ioctl(fd, MTIOCTOP, &change_partition);
		sodr_time_stop(drive);

		drive->status = failed ? so_drive_status_error : so_drive_status_loaded_idle;
		db->ops->sync_drive(db, drive, true, so_database_sync_default);

		if (failed != 0) {
			sodr_log_add_record(peer, so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Failed to change partion from %lu to %u on media '%s' because %m"),
				drive->vendor, drive->model, drive->index, status.mt_resid, partition, media->name);

			return failed;
		} else
			sodr_log_add_record(peer, so_job_status_running, db, so_log_level_info, so_job_record_notif_normal,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Partition changed from %lu to %u on media '%s'"),
				drive->vendor, drive->model, drive->index, status.mt_resid, partition, media->name);

		if (file_number >= 0) {
			failed = sodr_tape_drive_st_rewind(drive, fd, db);
			if (failed != 0)
				return failed;

			failed = sodr_tape_drive_st_get_status(drive, fd, &status, peer, db);
			if (failed != 0)
				return failed;
		}
	}

	if (file_number < 0) {
		so_log_write(so_log_level_info,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Fast forwarding to end of media '%s'"),
			drive->vendor, drive->model, drive->index, drive->slot->media->name);
		drive->status = so_drive_status_positioning;
		db->ops->sync_drive(db, drive, true, so_database_sync_default);

		static struct mtop eod = { MTEOM, 1 };
		sodr_time_start();
		failed = ioctl(fd, MTIOCTOP, &eod);
		sodr_time_stop(drive);

		drive->status = failed ? so_drive_status_error : so_drive_status_loaded_idle;
		db->ops->sync_drive(db, drive, true, so_database_sync_default);

		if (failed != 0)
			so_log_write(so_log_level_error,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Fast forwarding to end of media '%s' encountered an error '%m'"),
				drive->vendor, drive->model, drive->index, drive->slot->media->name);
		else
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Fast forwarding to end of media '%s' completed with status = OK"),
				drive->vendor, drive->model, drive->index, drive->slot->media->name);
	} else if (status.mt_fileno < file_number) {
		so_log_write(so_log_level_info,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' from #%u to position #%d"),
			drive->vendor, drive->model, drive->index, drive->slot->media->name, status.mt_fileno, file_number);
		drive->status = so_drive_status_positioning;
		db->ops->sync_drive(db, drive, true, so_database_sync_default);

		struct mtop fsr = { MTFSF, file_number - status.mt_fileno };
		sodr_time_start();
		failed = ioctl(fd, MTIOCTOP, &fsr);
		sodr_time_stop(drive);

		drive->status = failed ? so_drive_status_error : so_drive_status_loaded_idle;
		db->ops->sync_drive(db, drive, true, so_database_sync_default);

		if (failed != 0)
			so_log_write(so_log_level_error,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' from #%u to position #%d encountered an error '%m'"),
				drive->vendor, drive->model, drive->index, drive->slot->media->name, status.mt_fileno, file_number);
		else
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' from #%u to position #%d completed with status = OK"),
				drive->vendor, drive->model, drive->index, drive->slot->media->name, status.mt_fileno, file_number);
	} else if (status.mt_fileno > file_number) {
		if (file_number == 0) {
			failed = sodr_tape_drive_st_rewind(drive, fd, db);
		} else {
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' from #%u to position #%d"),
				drive->vendor, drive->model, drive->index, drive->slot->media->name, status.mt_fileno, file_number);
			drive->status = so_drive_status_positioning;
			db->ops->sync_drive(db, drive, true, so_database_sync_default);

			struct mtop bsfm = { MTBSFM, status.mt_fileno - file_number + 1 };
			sodr_time_start();
			failed = ioctl(fd, MTIOCTOP, &bsfm);
			sodr_time_stop(drive);

			drive->status = failed ? so_drive_status_error : so_drive_status_loaded_idle;
			db->ops->sync_drive(db, drive, true, so_database_sync_default);

			if (failed != 0)
				so_log_write(so_log_level_error,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' from #%u to position #%d encountered an error '%m'"),
					drive->vendor, drive->model, drive->index, drive->slot->media->name, status.mt_fileno, file_number);
			else
				so_log_write(so_log_level_info,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' from #%u to position #%d completed with status = OK"),
					drive->vendor, drive->model, drive->index, drive->slot->media->name, status.mt_fileno, file_number);
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

