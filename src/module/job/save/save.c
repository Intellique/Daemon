/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Thu, 06 Sep 2012 10:38:01 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// errno
#include <errno.h>
// open
#include <fcntl.h>
// scandir
#include <dirent.h>
// free, malloc
#include <stdlib.h>
// asprintf
#include <stdio.h>
// strcmp, strdup
#include <string.h>
// open, stat
#include <sys/stat.h>
// open, stat
#include <sys/types.h>
// time
#include <time.h>
// access, close, sleep, stat
#include <unistd.h>

#include <stone/io.h>
#include <stone/io/checksum.h>
#include <stone/io/json.h>
#include <stone/job.h>
#include <stone/library/archive.h>
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/ressource.h>
#include <stone/library/tape.h>
#include <stone/log.h>
#include <stone/tar.h>
#include <stone/user.h>
#include <stone/util.h>

struct st_job_save_private {
	struct st_tar_out * tar;
	char * buffer;
	ssize_t block_size;

	struct st_job_save_savepoint {
		struct st_changer * changer;
		struct st_drive * drive;
		struct st_slot * slot;
	} * savepoints;
	unsigned int nb_savepoints;

	struct st_drive * current_drive;
	struct st_archive_file * current_file;
	struct st_archive_volume * current_volume;

	struct st_stream_writer * current_tape_writer;
	struct st_stream_writer * current_checksum_writer;

	struct st_tape ** tapes;
	unsigned int nb_tapes;

	ssize_t total_size_done;
	ssize_t total_size;

	struct st_io_json * json;

	struct st_database_connection * db_con;

	int stop_request;
};

enum st_job_save_state {
	changer_got_tape,
	look_for_free_drive,
	look_in_first_changer,
	look_in_next_changer,
	select_new_tape,
};

static int st_job_save_archive_file(struct st_job * job, const char * path);
static int st_job_save_change_tape(struct st_job * job);
static int st_job_save_compute_filter(const struct dirent * file);
static ssize_t st_job_save_compute_total_size(struct st_job * job, const char * path);
static void st_job_save_free(struct st_job * job);
static void st_job_save_init(void) __attribute__((constructor));
static int st_job_save_manage_error(struct st_job * job, int error);
static void st_job_save_new_job(struct st_database_connection * db, struct st_job * job);
static int st_job_save_run(struct st_job * job);
static struct st_drive * st_job_save_select_tape(struct st_job * job, enum st_job_save_state state);
static void st_job_save_savepoint_save(struct st_job * job, struct st_changer * changer, struct st_drive * drive, struct st_slot * slot);
static int st_job_save_stop(struct st_job * job);
static int st_job_save_tape_in_list(struct st_job_save_private * jp, struct st_tape * tape);

struct st_job_ops st_job_save_ops = {
	.free = st_job_save_free,
	.run  = st_job_save_run,
	.stop = st_job_save_stop,
};

struct st_job_driver st_job_save_driver = {
	.name        = "save",
	.new_job     = st_job_save_new_job,
	.cookie      = 0,
	.api_version = STONE_JOB_APIVERSION,
};


int st_job_save_archive_file(struct st_job * job, const char * path) {
	struct st_job_save_private * jp = job->data;

	if (!st_util_check_valid_utf8(path)) {
		char * fixed_path = strdup(path);
		st_util_fix_invalid_utf8(fixed_path);

		job->db_ops->add_record(job, st_log_level_warning, "Path '%s' contains invalid utf8 characters", fixed_path);

		free(fixed_path);

		return 0;
	}

	struct stat st;
	if (lstat(path, &st)) {
		job->db_ops->add_record(job, st_log_level_error, "Error while getting information about: %s", path);
		return 1;
	}

	if (S_ISSOCK(st.st_mode))
		return 0;

	ssize_t block_number = jp->tar->ops->position(jp->tar) / jp->block_size;

	enum st_tar_out_status status = jp->tar->ops->add_file(jp->tar, path, path);
	int failed;
	switch (status) {
		case ST_TAR_OUT_END_OF_TAPE:
			failed = st_job_save_change_tape(job);
			if (failed)
				return failed;
			break;

		case ST_TAR_OUT_ERROR:
			failed = st_job_save_manage_error(job, jp->tar->ops->last_errno(jp->tar));
			if (failed)
				return failed;
			break;

		case ST_TAR_OUT_OK:
			block_number = jp->tar->ops->position(jp->tar) / jp->block_size;
			break;
	}

	jp->current_file = st_archive_file_new(job, &st, path, block_number);
	jp->db_con->ops->new_file(jp->db_con, jp->current_file);
	jp->db_con->ops->file_link_to_volume(jp->db_con, jp->current_file, jp->current_volume);

	if (S_ISREG(st.st_mode)) {
		int fd = open(path, O_RDONLY);
		struct st_stream_writer * file_checksum = st_checksum_get_steam_writer((const char **) job->checksums, job->nb_checksums, 0);

		for (;;) {
			ssize_t available_size = jp->tar->ops->get_available_size(jp->tar);

			if (available_size == 0) {
				ssize_t position = jp->tar->ops->get_file_position(jp->tar);

				jp->tar->ops->close(jp->tar);
				st_job_save_change_tape(job);
				available_size = jp->tar->ops->get_available_size(jp->tar);

				jp->tar->ops->restart_file(jp->tar, path, position);
			}

			ssize_t will_read = jp->block_size < available_size ? jp->block_size : available_size;
			ssize_t nb_read = read(fd, jp->buffer, will_read);
			if (nb_read == 0)
				break;

			if (nb_read < 0) {
				job->db_ops->add_record(job, st_log_level_error, "Unexpected error while reading from file '%s' because %m", path);
				return 3; // in the future, we will check errors
			}

			ssize_t nb_write;
			while ((nb_write = jp->tar->ops->write(jp->tar, jp->buffer, nb_read)) < 0) {
				if (st_job_save_manage_error(job, jp->tar->ops->last_errno(jp->tar)))
					return 4;
			}

			if (nb_write > 0) {
				file_checksum->ops->write(file_checksum, jp->buffer, nb_write);
				jp->total_size_done += nb_write;
			}

			job->done = (float) jp->total_size_done / jp->total_size;
			job->db_ops->update_status(job);
		}

		while (jp->tar->ops->end_of_file(jp->tar) < 0) {
			if (st_job_save_manage_error(job, jp->tar->ops->last_errno(jp->tar)))
				return 5;
		}
		file_checksum->ops->close(file_checksum);

		jp->current_file->digests = st_checksum_get_digest_from_writer(file_checksum);
		jp->current_file->nb_checksums = job->nb_checksums;
		jp->db_con->ops->file_add_checksum(jp->db_con, jp->current_file);

		st_io_json_add_file(jp->json, jp->current_file);

		close(fd);
		file_checksum->ops->free(file_checksum);
	} else if (S_ISDIR(st.st_mode)) {
		if (access(path, R_OK | X_OK)) {
			job->db_ops->add_record(job, st_log_level_error, "Can't read directory: %s", path);
			return 6;
		}

		st_io_json_add_file(jp->json, jp->current_file);

		struct dirent ** dl = 0;
		int nb_files = scandir(path, &dl, st_job_save_compute_filter, 0);

		int i, failed = 0;
		for (i = 0; i < nb_files; i++) {
			if (!failed && !jp->stop_request) {
				char * subpath = 0;
				asprintf(&subpath, "%s/%s", path, dl[i]->d_name);

				failed = st_job_save_archive_file(job, subpath);

				free(subpath);
			}

			free(dl[i]);
		}

		free(dl);

		return failed;
	}

	st_archive_file_free(jp->current_file);

	return 0;
}

int st_job_save_change_tape(struct st_job * job) {
	struct st_job_save_private * jp = job->data;

	struct st_drive * dr = jp->current_drive;
	struct st_changer * ch = dr->changer;
	struct st_stream_writer * tw = jp->current_tape_writer;
	struct st_stream_writer * cw = jp->current_checksum_writer;

	// update volume
	jp->current_volume->size = tw->ops->position(tw);
	jp->current_volume->endtime = time(0);
	jp->current_volume->digests = st_checksum_get_digest_from_writer(cw);
	jp->current_volume->nb_checksums = job->nb_checksums;
	jp->db_con->ops->update_volume(jp->db_con, jp->current_volume);
	st_io_json_update_volume(jp->json, jp->current_volume);

	// add tape into list to remember to not use it again
	jp->tapes = realloc(jp->tapes, (jp->nb_tapes + 1) * sizeof(struct st_tape *));
	jp->tapes[jp->nb_tapes] = dr->slot->tape;
	jp->nb_tapes++;

	unsigned int i;
	struct st_slot * sl = 0;
	for (i = ch->nb_drives; i < ch->nb_slots; i++) {
		sl = ch->slots + i;

		if (!sl->tape && sl->address == dr->slot->src_address && !sl->lock->ops->trylock(sl->lock))
			break;

		sl = 0;
	}

	// if not found, look for free slot
	for (i = ch->nb_drives; i < ch->nb_slots && !sl; i++) {
		sl = ch->slots + i;

		if (!sl->tape && !sl->lock->ops->trylock(sl->lock))
			break;

		sl = 0;
	}

	if (!sl) {
		// no slot for unloading tape
		job->sched_status = st_job_status_error;
		job->db_ops->add_record(job, st_log_level_error, "No slot free for unloading tape");
		return 1;
	}

	dr->ops->eject(dr);

	ch->lock->ops->lock(ch->lock);
	job->db_ops->add_record(job, st_log_level_info, "Unloading tape from drive #%td to slot #%td", dr - ch->drives, sl - ch->slots);
	ch->ops->unload(ch, dr, sl);
	ch->lock->ops->unlock(ch->lock);
	sl->lock->ops->unlock(sl->lock);

	struct st_drive * new_drive = st_job_save_select_tape(job, select_new_tape);

	if (dr != new_drive) {
		dr->lock->ops->unlock(dr->lock);
		dr = new_drive;
	}

	// create new volume
	jp->current_volume = st_archive_volume_new(job, dr);
	jp->db_con->ops->new_volume(jp->db_con, jp->current_volume);
	st_io_json_add_volume(jp->json, jp->current_volume);

	// link current file to new volume
	jp->current_file->position = 0;
	jp->db_con->ops->file_link_to_volume(jp->db_con, jp->current_file, jp->current_volume);

	tw = jp->current_tape_writer = dr->ops->get_writer(dr);
	cw = jp->current_checksum_writer = st_checksum_get_steam_writer((const char **) job->checksums, job->nb_checksums, tw);
	jp->tar->ops->new_volume(jp->tar, cw);

	// take care about block size of new tape
	ssize_t new_block_size = jp->tar->ops->get_block_size(jp->tar);
	if (new_block_size != jp->block_size) {
		jp->block_size = jp->tar->ops->get_block_size(jp->tar);
		jp->buffer = realloc(jp->buffer, jp->block_size);
	}

	return 0;
}

int st_job_save_compute_filter(const struct dirent * file) {
	if (!strcmp(file->d_name, "."))
		return 0;
	return strcmp(file->d_name, "..");
}

ssize_t st_job_save_compute_total_size(struct st_job * job, const char * path) {
	struct stat st;

	if (lstat(path, &st)) {
		job->db_ops->add_record(job, st_log_level_error, "Error while getting information about: %s", path);
		return 0;
	}

	if (S_ISREG(st.st_mode)) {
		if (access(path, R_OK))
			job->db_ops->add_record(job, st_log_level_error, "Can't read file: %s", path);
		return st.st_size;
	}

	ssize_t total = 0;
	if (S_ISDIR(st.st_mode)) {
		if (access(path, R_OK | X_OK)) {
			job->db_ops->add_record(job, st_log_level_error, "Can't read directory: %s", path);
			return 0;
		}

		struct dirent ** dl = 0;
		int nb_files = scandir(path, &dl, st_job_save_compute_filter, 0);

		int i;
		for (i = 0; i < nb_files; i++) {
			char * subpath = 0;
			asprintf(&subpath, "%s/%s", path, dl[i]->d_name);

			total += st_job_save_compute_total_size(job, subpath);

			free(subpath);
			free(dl[i]);
		}

		free(dl);
	}

	return total;
}

void st_job_save_free(struct st_job * job) {
	free(job->data);
}

void st_job_save_init() {
	st_job_register_driver(&st_job_save_driver);
}

int st_job_save_manage_error(struct st_job * job, int error) {
	switch (error) {
		case ENOSPC:
			return st_job_save_change_tape(job);

		default:
			job->sched_status = st_job_status_error;
			job->db_ops->add_record(job, st_log_level_error, "Unmanaged error: %m, job aborted");
			return 1;
	}
}

void st_job_save_new_job(struct st_database_connection * db __attribute__((unused)), struct st_job * job) {
	struct st_job_save_private * self = malloc(sizeof(struct st_job_save_private));
	self->tar = 0;
	self->buffer = 0;
	self->block_size = 0;

	self->savepoints = 0;
	self->nb_savepoints = 0;

	self->current_drive = 0;
	self->current_file = 0;
	self->current_volume = 0;

	self->tapes = 0;
	self->nb_tapes = 0;

	self->total_size_done = 0;
	self->total_size = 0;

	self->json = 0;

	self->db_con = 0;

	self->stop_request = 0;

	job->data = self;
	job->job_ops = &st_job_save_ops;
}

int st_job_save_run(struct st_job * job) {
	struct st_job_save_private * jp = job->data;

	job->db_ops->add_record(job, st_log_level_info, "Start archive job (job id: %ld), num runs %ld", job->id, job->num_runs);

	struct st_database * driver = st_db_get_default_db();
	jp->db_con = driver->ops->connect(driver, 0);

	// check permission
	struct st_user * user = job->user;
	if (!user->can_archive) {
		job->sched_status = st_job_status_error;
		job->db_ops->add_record(job, st_log_level_error, "User (%s) is not allowed to create archive", user->fullname);
		return 1;
	}

	// check tape and pool
	if (!job->pool)
		job->pool = job->user->pool;

	// compute total files size
	unsigned int i;
	for (i = 0; i < job->nb_paths; i++) {
		// remove double '/'
		st_util_string_delete_double_char(job->paths[i], '/');
		st_util_string_rtrim(job->paths[i], '/');

		jp->total_size += st_job_save_compute_total_size(job, job->paths[i]);
	}

	if (jp->total_size == 0) {
		job->db_ops->add_record(job, st_log_level_error, "There is no file to archive or total size of all files is null");
		return 3;
	}

	char bufsize[32];
	st_util_convert_size_to_string(jp->total_size, bufsize, 32);
	job->db_ops->add_record(job, st_log_level_info, "Will archive %s", bufsize);

	struct st_drive * drive = jp->current_drive = st_job_save_select_tape(job, look_in_first_changer);

	// start new transaction
	jp->db_con->ops->start_transaction(jp->db_con);

	// archive
	struct st_archive * archive = job->archive;
	if (!archive) {
		archive = job->archive = st_archive_new(job);
		jp->db_con->ops->new_archive(jp->db_con, archive);
	}
	jp->json = st_io_json_new(archive);
	st_io_json_add_metadata(jp->json, job->job_meta);
	// volume
	jp->current_volume = st_archive_volume_new(job, drive);
	jp->db_con->ops->new_volume(jp->db_con, jp->current_volume);
	st_io_json_add_volume(jp->json, jp->current_volume);

	int failed = 0;
	if (!jp->stop_request) {
		struct st_stream_writer * tape_writer = jp->current_tape_writer = drive->ops->get_writer(drive);
		struct st_stream_writer * checksum_writer = jp->current_checksum_writer = st_checksum_get_steam_writer((const char **) job->checksums, job->nb_checksums, tape_writer);
		jp->tar = st_tar_new_out(checksum_writer);

		jp->block_size = jp->tar->ops->get_block_size(jp->tar);
		jp->buffer = malloc(jp->block_size);

		for (i = 0; i < job->nb_paths && !failed && !jp->stop_request; i++) {
			job->db_ops->add_record(job, st_log_level_info, "Archive %s", job->paths[i]);
			failed = st_job_save_archive_file(job, job->paths[i]);
		}

		// because drive has maybe changed
		drive = jp->current_drive;
		tape_writer = jp->current_tape_writer;
		checksum_writer = jp->current_checksum_writer;

		jp->tar->ops->close(jp->tar);

		if (!failed) {
			// archive
			archive->endtime = jp->current_volume->endtime = time(0);
			jp->db_con->ops->update_archive(jp->db_con, archive);
			st_io_json_update_archive(jp->json, archive);
			// volume
			jp->current_volume->size = tape_writer->ops->position(tape_writer);
			jp->current_volume->endtime = time(0);
			jp->current_volume->digests = st_checksum_get_digest_from_writer(checksum_writer);
			jp->current_volume->nb_checksums = job->nb_checksums;
			jp->db_con->ops->update_volume(jp->db_con, jp->current_volume);
			st_io_json_update_volume(jp->json, jp->current_volume);

			// commit transaction
			jp->db_con->ops->finish_transaction(jp->db_con);
		} else {
			// rollback transaction
			jp->db_con->ops->cancel_transaction(jp->db_con);
		}

		// release some memory
		jp->tar->ops->free(jp->tar);
		st_archive_free(job->archive);
		//job->archive = 0;
		//

		if (!failed) {
			// write index file
			job->db_ops->add_record(job, st_log_level_info, "Writing index file");

			tape_writer = drive->ops->get_writer(drive);
			st_io_json_write_to(jp->json, tape_writer);
			tape_writer->ops->close(tape_writer);

			tape_writer->ops->free(tape_writer);
			st_io_json_free(jp->json);
		}
	} else {
		st_archive_free(job->archive);
	}

	drive->lock->ops->unlock(drive->lock);

	if (jp->stop_request)
		job->db_ops->add_record(job, st_log_level_error, "Job: save aborted (job id: %ld), num runs %ld", job->id, job->num_runs);
	else if (failed)
		job->db_ops->add_record(job, st_log_level_error, "Finish archive job (job id: %ld), with status = %d, num runs %ld", job->id, failed, job->num_runs);
	else
		job->db_ops->add_record(job, st_log_level_info, "Finish archive job (job id: %ld) with status = Ok, num runs %ld", job->id, job->num_runs);

	return 0;
}

struct st_drive * st_job_save_select_tape(struct st_job * job, enum st_job_save_state state) {
	struct st_job_save_private * jp = job->data;

	struct st_changer * changer = 0;
	struct st_drive * drive = 0;
	struct st_slot * slot = 0;

	short stop = 0;
	unsigned int i, j;
	while (!stop) {
		switch (state) {
			case changer_got_tape:
				slot = 0;
				for (i = 0; i < changer->nb_slots && !slot; i++) {
					slot = changer->slots + i;

					if (!slot->tape || slot->tape->pool != job->pool || st_job_save_tape_in_list(jp, slot->tape) || (drive && slot->drive == drive)) {
						slot = 0;
						continue;
					}

					if (slot->drive) {
						if (!slot->drive->lock->ops->trylock(slot->drive->lock)) {
							slot->drive->ops->eod(slot->drive);
							return slot->drive;
						} else {
							st_job_save_savepoint_save(job, changer, slot->drive, slot);
							slot = 0;
							continue;
						}
					}

					if (!slot->lock->ops->trylock(slot->lock)) {
						if (drive) {
							changer->lock->ops->lock(changer->lock);
							job->db_ops->add_record(job, st_log_level_info, "Loading tape from slot #%td to drive #%td", slot - changer->slots, drive - changer->drives);
							changer->ops->load(changer, slot, drive);
							changer->lock->ops->unlock(changer->lock);
							slot->lock->ops->unlock(slot->lock);

							drive->ops->reset(drive);
							drive->ops->eod(drive);
							return drive;
						}

						state = look_for_free_drive;
					} else {
						st_job_save_savepoint_save(job, changer, drive, slot);
						slot = 0;
					}
				}

				if (!slot)
					state = look_in_next_changer;

				break;

			case look_for_free_drive:
				for (i = 0; i < changer->nb_drives; i++) {
					drive = changer->drives + i;

					if (!drive->lock->ops->trylock(drive->lock))
						break;

					drive = 0;
				}

				if (drive) {
					changer->lock->ops->lock(changer->lock);
					job->db_ops->add_record(job, st_log_level_info, "Loading tape from slot #%td to drive #%td", slot - changer->slots, drive - changer->drives);
					changer->ops->load(changer, slot, drive);
					changer->lock->ops->unlock(changer->lock);
					slot->lock->ops->unlock(slot->lock);

					drive->ops->reset(drive);
					drive->ops->eod(drive);
					return drive;
				}
				st_job_save_savepoint_save(job, changer, drive, slot);
				slot->lock->ops->unlock(slot->lock);
				state = look_in_next_changer;
				break;

			case look_in_first_changer:
				changer = st_changer_get_first_changer();
				state = changer_got_tape;
				break;

			case look_in_next_changer:
				changer = st_changer_get_next_changer(changer);

				if (changer) {
					state = changer_got_tape;
				} else {
					while (jp->nb_savepoints > 0) {
						for (i = 0; i < jp->nb_savepoints; i++) {
							struct st_job_save_savepoint * sp = jp->savepoints + i;

							changer = sp->changer;
							drive = sp->drive;
							slot = sp->slot;

							if (drive && !drive->lock->ops->trylock(drive->lock)) {
								if (!drive->slot->tape || drive->slot->tape->pool != job->pool) {
									drive->lock->ops->unlock(drive->lock);

									if (i + 1 < jp->nb_savepoints)
										memmove(jp->savepoints + i, jp->savepoints + i + 1, (jp->nb_savepoints - i - 1) * sizeof(struct st_job_save_savepoint));
									jp->savepoints = realloc(jp->savepoints, (jp->nb_savepoints - 1) * sizeof(struct st_job_save_savepoint));
									jp->nb_savepoints--;

									i--;

									if (jp->nb_savepoints == 0)
										state = look_in_first_changer;

									continue;
								}

								drive->ops->eod(drive);
								return drive;
							} else if (drive) {
								continue;
							} else if (slot && !slot->lock->ops->trylock(slot->lock)) {
								if (!slot->tape || slot->tape->pool != job->pool) {
									slot->lock->ops->unlock(slot->lock);

									if (i + 1 < jp->nb_savepoints)
										memmove(jp->savepoints + i, jp->savepoints + i + 1, (jp->nb_savepoints - i - 1) * sizeof(struct st_job_save_savepoint));
									jp->savepoints = realloc(jp->savepoints, (jp->nb_savepoints - 1) * sizeof(struct st_job_save_savepoint));
									jp->nb_savepoints--;

									i--;

									if (jp->nb_savepoints == 0)
										state = look_in_first_changer;

									continue;
								}

								// look for free drive
								for (j = 0; j < changer->nb_drives; j++) {
									drive = changer->drives + j;

									if (!drive->lock->ops->trylock(drive->lock))
										break;

									drive = 0;
								}

								if (drive) {
									changer->lock->ops->lock(changer->lock);
									job->db_ops->add_record(job, st_log_level_info, "Loading tape from slot #%td to drive #%td", slot - changer->slots, drive - changer->drives);
									changer->ops->load(changer, slot, drive);
									changer->lock->ops->unlock(changer->lock);
									slot->lock->ops->unlock(slot->lock);

									drive->ops->reset(drive);
									drive->ops->eod(drive);
									return drive;
								} else {
									slot->lock->ops->unlock(slot->lock);
								}
							}
						}

						sleep(5);
					}

					if (jp->nb_savepoints == 0)
						sleep(5);
				}
				break;

			case select_new_tape:
				changer = jp->current_drive->changer;
				drive = jp->current_drive;
				state = changer_got_tape;
				break;
		}
	}
	return 0;
}

void st_job_save_savepoint_save(struct st_job * job, struct st_changer * changer, struct st_drive * drive, struct st_slot * slot) {
	struct st_job_save_private * jp = job->data;
	jp->savepoints = realloc(jp->savepoints, (jp->nb_savepoints + 1) * sizeof(struct st_job_save_savepoint));
	jp->savepoints[jp->nb_savepoints].changer = changer;
	jp->savepoints[jp->nb_savepoints].drive = drive;
	jp->savepoints[jp->nb_savepoints].slot = slot;
	jp->nb_savepoints++;
}

int st_job_save_stop(struct st_job * job) {
	struct st_job_save_private * self = job->data;
	if (self && !self->stop_request) {
		self->stop_request = 1;
		job->db_ops->add_record(job, st_log_level_warning, "Job: Stop requested");
		return 0;
	}
	return 1;
}

int st_job_save_tape_in_list(struct st_job_save_private * jp, struct st_tape * tape) {
	unsigned int i;
	for (i = 0; i < jp->nb_tapes; i++)
		if (jp->tapes[i] == tape)
			return 1;
	return 0;
}

