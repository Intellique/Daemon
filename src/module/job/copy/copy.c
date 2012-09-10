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
*  Last modified: Mon, 10 Sep 2012 18:52:14 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// scandir
#include <dirent.h>
// errno
#include <errno.h>
// open
#include <fcntl.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strchr
#include <string.h>
// S_*
#include <sys/stat.h>
// S_*
#include <sys/types.h>
// time
#include <time.h>
// rmdir, sleep, unlink
#include <unistd.h>


#include <stone/io.h>
#include <stone/io/checksum.h>
#include <stone/io/json.h>
#include <stone/library/archive.h>
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/ressource.h>
#include <stone/library/tape.h>
#include <stone/job.h>
#include <stone/log.h>
#include <stone/tar.h>
#include <stone/user.h>
#include <stone/util.h>

struct st_job_copy_private {
	struct st_tar_out * tar_out;

	char * buffer;
	ssize_t block_size;

	char * first_file;
	size_t restore_path_length;

	ssize_t position;
	ssize_t total_done;
	ssize_t total_size;

	struct st_changer * changer;
	struct st_drive * drive;
	struct st_slot * slot;

	struct st_job_copy_savepoint {
		struct st_changer * changer;
		struct st_drive * drive;
		struct st_slot * slot;
	} * savepoints;
	unsigned int nb_savepoints;

	struct st_stream_writer * tape_writer;
	struct st_stream_writer * checksum_writer;

	struct st_archive_file * current_file;
	struct st_archive_volume * current_volume;

	struct st_tape ** tapes;
	unsigned int nb_tapes;

	struct st_io_json * json;

	struct st_database_connection * db_con;

	int stop_request;
};

enum st_job_copy_state {
	changer_got_tape,
	look_for_free_drive,
	look_in_first_changer,
	look_in_next_changer,
	select_new_tape,
};

static int st_job_copy_archive_file(struct st_job * job, const char * path);
static int st_job_copy_change_tape(struct st_job * job);
static int st_job_copy_filter(const struct dirent * file);
static void st_job_copy_free(struct st_job * job);
static void st_job_copy_init(void) __attribute__((constructor));
static int st_job_copy_load_tape(struct st_job * job, struct st_tape * tape);
static int st_job_copy_manage_error(struct st_job * job, int error);
static int st_job_copy_mkdir(const char * path);
static void st_job_copy_new_job(struct st_database_connection * db, struct st_job * job);
static int st_job_copy_restore_archive(struct st_job * job);
static int st_job_copy_restore_file(struct st_job * job, struct st_tar_in * tar, struct st_tar_header * header, const char * path);
static void st_job_copy_rmdir(const char * path);
static int st_job_copy_run(struct st_job * job);
static struct st_drive * st_job_copy_select_tape(struct st_job * job, enum st_job_copy_state state);
static void st_job_copy_savepoint_save(struct st_job * job, struct st_changer * changer, struct st_drive * drive, struct st_slot * slot);
static int st_job_copy_stop(struct st_job * job);
static int st_job_copy_tape_in_list(struct st_job_copy_private * jp, struct st_tape * tape);

struct st_job_ops st_job_copy_ops = {
	.free = st_job_copy_free,
	.run  = st_job_copy_run,
	.stop = st_job_copy_stop,
};

struct st_job_driver st_job_copy_driver = {
	.name        = "copy",
	.new_job     = st_job_copy_new_job,
	.cookie      = 0,
	.api_version = STONE_JOB_APIVERSION,
};


int st_job_copy_archive_file(struct st_job * job, const char * path) {
	struct st_job_copy_private * jp = job->data;

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

	ssize_t block_number = jp->tar_out->ops->position(jp->tar_out) / jp->block_size;

	enum st_tar_out_status status = jp->tar_out->ops->add_file(jp->tar_out, path, path + jp->restore_path_length);

	jp->current_file = st_archive_file_new(job, &st, path + jp->restore_path_length, block_number);
	st_archive_volume_add_file(jp->current_volume, jp->current_file, block_number);

	int failed = 0;
	switch (status) {
		case ST_TAR_OUT_END_OF_TAPE:
			failed = st_job_copy_change_tape(job);
			if (failed)
				return failed;
			break;

		case ST_TAR_OUT_ERROR:
			failed = st_job_copy_manage_error(job, jp->tar_out->ops->last_errno(jp->tar_out));
			if (failed)
				return failed;
			break;

		default:
			break;
	}

	if (S_ISREG(st.st_mode)) {
		int fd = open(path, O_RDONLY);
		struct st_stream_writer * file_checksum = st_checksum_get_steam_writer((const char **) job->checksums, job->nb_checksums, 0);

		for (;;) {
			ssize_t available_size = jp->tar_out->ops->get_available_size(jp->tar_out);

			if (available_size == 0) {
				ssize_t position = jp->tar_out->ops->get_file_position(jp->tar_out);

				jp->tar_out->ops->close(jp->tar_out);
				st_job_copy_change_tape(job);
				available_size = jp->tar_out->ops->get_available_size(jp->tar_out);

				jp->tar_out->ops->restart_file(jp->tar_out, path, position);
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
			while ((nb_write = jp->tar_out->ops->write(jp->tar_out, jp->buffer, nb_read)) < 0) {
				if (st_job_copy_manage_error(job, jp->tar_out->ops->last_errno(jp->tar_out)))
					return 4;
			}

			if (nb_write > 0) {
				file_checksum->ops->write(file_checksum, jp->buffer, nb_write);
				jp->total_done += nb_write;
			}

			ssize_t position = jp->tar_out->ops->position(jp->tar_out);
			job->done = 0.5 + (float) position / jp->total_size;
			job->db_ops->update_status(job);
		}

		while (jp->tar_out->ops->end_of_file(jp->tar_out) < 0) {
			if (st_job_copy_manage_error(job, jp->tar_out->ops->last_errno(jp->tar_out)))
				return 5;
		}
		file_checksum->ops->close(file_checksum);

		jp->current_file->digests = st_checksum_get_digest_from_writer(file_checksum);
		jp->current_file->nb_checksums = job->nb_checksums;

		st_io_json_add_file(jp->json, jp->current_file);

		close(fd);
		file_checksum->ops->free(file_checksum);
	} else if (S_ISDIR(st.st_mode)) {
		if (access(path, R_OK | X_OK)) {
			job->db_ops->add_record(job, st_log_level_error, "Can't read directory: %s", path);
			return 6;
		}

		struct dirent ** dl = 0;
		int nb_files = scandir(path, &dl, st_job_copy_filter, 0);

		int i;
		for (i = 0; i < nb_files; i++) {
			if (!failed && !jp->stop_request) {
				char * subpath = 0;
				asprintf(&subpath, "%s/%s", path, dl[i]->d_name);

				failed = st_job_copy_archive_file(job, subpath);

				free(subpath);
			}

			free(dl[i]);
		}

		free(dl);
	}

	ssize_t position = jp->tar_out->ops->position(jp->tar_out);
	job->done = 0.5 + (float) position / jp->total_size;
	job->db_ops->update_status(job);

	// delete file or directory
	if (S_ISDIR(st.st_mode)) {
		rmdir(path);
	} else {
		unlink(path);
	}

	return failed;
}

int st_job_copy_change_tape(struct st_job * job) {
	struct st_job_copy_private * jp = job->data;

	struct st_drive * dr = jp->drive;
	struct st_changer * ch = dr->changer;
	struct st_stream_writer * tw = jp->tape_writer;
	struct st_stream_writer * cw = jp->checksum_writer;

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

	dr->ops->eject(dr);

	job->db_ops->add_record(job, st_log_level_info, "Unloading tape from drive #%td", dr - ch->drives);
	if (ch->ops->unload(ch, dr)) {
		job->db_ops->add_record(job, st_log_level_error, "An error occured while unloading tape, job existing");
		return 1;
	}

	struct st_drive * new_drive = st_job_copy_select_tape(job, select_new_tape);

	if (dr != new_drive) {
		dr->lock->ops->unlock(dr->lock);
		dr = new_drive;
	}

	// add new volume
	jp->current_volume = st_archive_volume_new(job, dr);
	jp->db_con->ops->new_volume(jp->db_con, jp->current_volume);
	st_io_json_add_volume(jp->json, jp->current_volume);

	// link current file to new volume
	// jp->current_file->position = 0;
	jp->db_con->ops->file_link_to_volume(jp->db_con, jp->current_file, jp->current_volume);

	tw = jp->tape_writer = dr->ops->get_writer(dr);
	cw = jp->checksum_writer = st_checksum_get_steam_writer((const char **) job->checksums, job->nb_checksums, tw);
	jp->tar_out->ops->new_volume(jp->tar_out, cw);

	// take care about block size of new tape
	ssize_t new_block_size = jp->tar_out->ops->get_block_size(jp->tar_out);
	if (new_block_size != jp->block_size) {
		jp->block_size = jp->tar_out->ops->get_block_size(jp->tar_out);
		jp->buffer = realloc(jp->buffer, jp->block_size);
	}

	return 0;
}

int st_job_copy_filter(const struct dirent * file) {
	if (!strcmp(file->d_name, "."))
		return 0;
	return strcmp(file->d_name, "..");
}

void st_job_copy_free(struct st_job * job) {
	free(job->data);
}

void st_job_copy_init() {
	st_job_register_driver(&st_job_copy_driver);
}

int st_job_copy_load_tape(struct st_job * job, struct st_tape * tape) {
	struct st_job_copy_private * jp = job->data;
	short has_alert_user = 0;
	enum {
		alert_user,
		drive_is_free,
		look_for_changer,
		look_for_free_drive,
	} state = look_for_changer;

	unsigned int i;
	for (;;) {
		switch (state) {
			case alert_user:
				job->sched_status = st_job_status_pause;
				sleep(1);
				job->db_ops->update_status(job);

				if (!has_alert_user) {
					job->db_ops->add_record(job, st_log_level_warning, "Tape not found (named: %s)", tape->name);
					st_log_write_all(st_log_level_error, st_log_type_user_message, "Job: copy (id:%ld) request you to put a tape (named: %s) in your changer or standalone drive", job->id, tape->name);
				}
				has_alert_user = 1;
				sleep(5);

				job->sched_status = st_job_status_running;
				job->db_ops->update_status(job);

				state = look_for_changer;
				break;

			case drive_is_free:
				while (jp->drive->lock->ops->trylock(jp->drive->lock)) {
					sleep(5);

					if (jp->drive->slot->tape != tape) {
						state = look_for_changer;
						break;
					}
				}

				if (jp->drive->slot->tape == tape)
					return 0;

				break;

			case look_for_changer:
				jp->changer = st_changer_get_by_tape(tape);

				if (jp->changer) {
					jp->slot = jp->changer->ops->get_tape(jp->changer, tape);
					jp->drive = jp->slot->drive;
					state = jp->drive ? drive_is_free : look_for_free_drive;
				} else {
					state = alert_user;
				}
				break;

			case look_for_free_drive:
				jp->slot->lock->ops->lock(jp->slot->lock);

				if (jp->slot->tape != tape) {
					// it seem that someone has load tape before
					jp->slot->lock->ops->unlock(jp->slot->lock);

					state = look_for_changer;
					break;
				}

				// look for only unloaded free drive
				jp->drive = 0;
				for (i = 0; i < jp->changer->nb_drives && !jp->drive; i++) {
					jp->drive = jp->changer->drives + i;

					if (jp->drive->lock->ops->trylock(jp->drive->lock))
						jp->drive = 0;

					if (jp->drive->slot->tape) {
						jp->drive->lock->ops->unlock(jp->drive->lock);
						jp->drive = 0;
					}
				}

				// second attempt, look for free drive
				for (i = 0; i < jp->changer->nb_drives && !jp->drive; i++) {
					jp->drive = jp->changer->drives + i;

					if (jp->drive->lock->ops->trylock(jp->drive->lock))
						jp->drive = 0;
				}

				if (jp->drive) {
					if (jp->drive->slot->tape) {
						jp->drive->ops->eject(jp->drive);

						if (jp->changer->ops->unload(jp->changer, jp->drive)) {
							job->db_ops->add_record(job, st_log_level_error, "An error occured while unloading tape, job existing");
							return 1;
						}
					}

					jp->changer->lock->ops->lock(jp->changer->lock);
					jp->changer->ops->load(jp->changer, jp->slot, jp->drive);
					jp->changer->lock->ops->unlock(jp->changer->lock);
					jp->slot->lock->ops->unlock(jp->slot->lock);
					jp->drive->ops->reset(jp->drive, false, false);

					return 0;
				} else {
					jp->slot->lock->ops->unlock(jp->slot->lock);
					sleep(5);
					state = look_for_changer;
				}
				break;
		}
	}

	return 1;
}

int st_job_copy_manage_error(struct st_job * job, int error) {
	switch (error) {
		case ENOSPC:
			return st_job_copy_change_tape(job);

		default:
			job->sched_status = st_job_status_error;
			job->db_ops->add_record(job, st_log_level_error, "Unmanaged error: %m, job aborted");
			return 1;
	}
}

int st_job_copy_mkdir(const char * path) {
	if (!access(path, F_OK))
		return 0;

	char * path2 = strdup(path);
	ssize_t length = strlen(path2);
	while (path2[length - 1] == '/') {
		length--;
		path2[length] = '\0';
	}

	int failed = 0;
	short i = 0;
	do {
		i++;
		char * last = strrchr(path2, '/');

		if (last == path2) {
			failed = mkdir(path2, 0777);
			if (failed)
				i = 0;
			break;
		}

		*last = '\0';
	} while (access(path2, F_OK));

	for (; i > 0; i--) {
		path2[strlen(path2)] = '/';
		failed = mkdir(path2, 0777);
		if (failed)
			break;
	}

	free(path2);
	return failed;
}

void st_job_copy_new_job(struct st_database_connection * db __attribute__((unused)), struct st_job * job) {
	struct st_job_copy_private * self = malloc(sizeof(struct st_job_copy_private));
	self->buffer = 0;
	self->block_size = 0;

	self->first_file = 0;
	self->restore_path_length = 0;

	self->position = 0;
	self->total_done = 0;
	self->total_size = 0;

	self->changer = 0;
	self->drive = 0;
	self->slot = 0;

	self->tar_out = 0;
	self->savepoints = 0;
	self->nb_savepoints = 0;

	self->tapes = 0;
	self->nb_tapes = 0;

	self->json = 0;

	self->db_con = 0;

	self->stop_request = 0;

	job->data = self;
	job->job_ops = &st_job_copy_ops;
}

int st_job_copy_restore_archive(struct st_job * job) {
	struct st_job_copy_private * jp = job->data;

	unsigned int i;
	int status = 0;
	for (i = 0; i < job->nb_block_numbers && !status; i++) {
		struct st_job_block_number * bn = job->block_numbers + i;

		status = st_job_copy_load_tape(job, bn->tape);
		if (status) {
			job->db_ops->add_record(job, st_log_level_error, "Failed to load tape: %s", bn->tape->name);
			return status;
		}

		jp->drive->ops->set_file_position(jp->drive, bn->tape_position);

		struct st_stream_reader * tape_reader = jp->drive->ops->get_reader(jp->drive);
		struct st_tar_in * tar = st_tar_new_in(tape_reader);

		jp->block_size = tar->ops->get_block_size(tar);
		jp->buffer = malloc(jp->block_size);

		struct st_tar_header header;
		enum st_tar_header_status hdr_status;
		ssize_t restore_path_length = strlen(job->restore_to->path);
		int failed = 0;

		while ((hdr_status = tar->ops->get_header(tar, &header)) == ST_TAR_HEADER_OK && !jp->stop_request) {
			char * path = header.path;
			if (job->restore_to->nb_trunc_path > 0)
				path = st_util_trunc_path(path, job->restore_to->nb_trunc_path);

			if (!path)
				continue;

			if (!jp->first_file)
				jp->first_file = strdup(path);

			ssize_t length = restore_path_length + strlen(path);
			char * restore_path = malloc(length + 2);

			strcpy(restore_path, job->restore_to->path);

			if (restore_path[restore_path_length - 1] == '/')
				strcpy(restore_path + restore_path_length, path);
			else {
				strcpy(restore_path + restore_path_length, "/");
				strcpy(restore_path + restore_path_length + 1, path);
			}

			char * pos = strrchr(restore_path, '/');
			if (pos) {
				*pos = '\0';
				st_job_copy_mkdir(restore_path);
				*pos = '/';
			}

			failed = st_job_copy_restore_file(job, tar, &header, restore_path);

			free(restore_path);

			st_tar_free_header(&header);

			if (failed)
				break;
		}

		jp->block_size = 0;
		free(jp->buffer);
		jp->buffer = 0;

		tar->ops->close(tar);
		tar->ops->free(tar);

		status = (hdr_status != ST_TAR_HEADER_NOT_FOUND || failed);

		if (!status)
			job->done = 0.5;
		job->db_ops->update_status(job);
	}

	return status;
}

int st_job_copy_restore_file(struct st_job * job, struct st_tar_in * tar, struct st_tar_header * header, const char * path) {
	if (header->is_label)
		return 0;

	struct st_job_copy_private * jp = job->data;
	int status = 0;

	if (header->link && !(header->mode & S_IFMT)) {
		if (!access(path, F_OK)) {
			status = unlink(path);

			if (status) {
				switch (errno) {
					default:
						job->db_ops->add_record(job, st_log_level_error, "Failed to delete file before restoring hardlink %s because %m", path);
						return status;
				}
			}
		}
		status = link(header->link, path);
		if (status) {
			switch (errno) {
				default:
					job->db_ops->add_record(job, st_log_level_warning, "Failed to create an hardlink from %s to %s because %m", path, header->link);
					break;
			}
		}
	} else if (S_ISFIFO(header->mode)) {
		if (!access(path, F_OK)) {
			status = unlink(path);
			if (status) {
				switch (errno) {
					default:
						job->db_ops->add_record(job, st_log_level_error, "failed to delete file before restoring fifo named %s because %m", path);
						return status;
				}
			}
		}
		status = mknod(path, S_IFIFO, 0);
		if (status) {
			switch (errno) {
				default:
					job->db_ops->add_record(job, st_log_level_warning, "Failed to create a fifo named %s because %m", path);
					break;
			}
		}
	} else if (S_ISCHR(header->mode)) {
		if (!access(path, F_OK)) {
			status = unlink(path);
			if (status) {
				switch (errno) {
					default:
						job->db_ops->add_record(job, st_log_level_error, "Failed to delete file before restoring character device (major:%d,minor:%d) named %s because %m", (int) header->dev >> 8, (int) header->dev & 0xFF, path);
						return status;
				}
			}
		}
		status = mknod(path, S_IFCHR, header->dev);
		if (status) {
			switch (errno) {
				default:
					job->db_ops->add_record(job, st_log_level_error, "Failed to create a character device (major:%d,minor:%d) named %s because %m", (int) header->dev >> 8, (int) header->dev & 0xFF, path);
					break;
			}
		}
	} else if (S_ISDIR(header->mode)) {
		if (!access(path, F_OK))
			return 0;
		status = mkdir(path, header->mode & 0777);
		if (status) {
			switch (errno) {
				default:
					job->db_ops->add_record(job, st_log_level_warning, "Failed to create a directory named %s because %m", path);
					break;
			}
		}
	} else if (S_ISBLK(header->mode)) {
		if (!access(path, F_OK)) {
			status = unlink(path);
			if (status) {
				switch (errno) {
					default:
						job->db_ops->add_record(job, st_log_level_error, "Failed to delete file before restoring block device (major:%d,minor:%d) named %s because %m", (int) header->dev >> 8, (int) header->dev & 0xFF, path);
						return status;
				}
			}
		}
		status = mknod(path, S_IFBLK, header->dev);
		if (status) {
			switch (errno) {
				default:
					job->db_ops->add_record(job, st_log_level_warning, "Failed to create a block device (major:%d,minor:%d) named %s because %m", (int) header->dev >> 8, (int) header->dev & 0xFF, path);
					break;
			}
		}
	} else if (S_ISREG(header->mode)) {
		if (!access(path, F_OK) && header->offset == 0) {
			status = unlink(path);
			if (status) {
				switch (errno) {
					default:
						job->db_ops->add_record(job, st_log_level_error, "Failed to delete file before restoring regular file named %s because %m", path);
						return status;
				}
			}
		}
		int fd = open(path, O_CREAT | O_WRONLY, header->mode);
		if (fd < 0) {
			job->db_ops->add_record(job, st_log_level_warning, "Failed to create a file named %s because %m", path);
			return status;
		}

		if (header->offset > 0) {
			if (lseek(fd, header->offset, SEEK_SET) == (off_t) -1) {
				job->db_ops->add_record(job, st_log_level_warning, "Failed to seek position into file named %s because %m", path);
				close(fd);
				return status;
			}
		}

		struct st_job_copy_private * jp = job->data;

		ssize_t nb_read, nb_total_read = 0;
		while ((nb_read = tar->ops->read(tar, jp->buffer, jp->block_size)) > 0) {
			if (jp->stop_request)
				break;

			ssize_t nb_write = write(fd, jp->buffer, nb_read);
			if (nb_write < 0) {
				job->db_ops->add_record(job, st_log_level_warning, "An error occured while writing to file named %s because %m", path);
			}

			nb_total_read += nb_read;

			jp->total_done = jp->position + tar->ops->position(tar);

			job->done = (float) jp->total_done / jp->total_size;
			job->db_ops->update_status(job);
		}

		if (nb_read < 0)
			job->db_ops->add_record(job, st_log_level_warning, "An error occured while reading from tape because %m");

		close(fd);

	} else if (S_ISLNK(header->mode)) {
		if (!access(path, F_OK)) {
			status = unlink(path);

			if (status) {
				switch (errno) {
					default:
						job->db_ops->add_record(job, st_log_level_error, "Failed to delete file before restoring symbolic %s because %m", path);
						return status;
				}
			}
		}
		status = symlink(header->link, header->path);
	}

	jp->total_done = jp->position + tar->ops->position(tar);
	job->done = (float) jp->total_done / jp->total_size;
	job->db_ops->update_status(job);

	return status;
}

void st_job_copy_rmdir(const char * path) {
	struct stat st;
	int failed = lstat(path, &st);
	if (failed)
		return;

	if (S_ISDIR(st.st_mode)) {
		struct dirent ** dl = 0;
		int nb_files = scandir(path, &dl, st_job_copy_filter, 0);

		int i;
		for (i = 0; i < nb_files; i++) {
			if (!failed) {
				char * subpath = 0;
				asprintf(&subpath, "%s/%s", path, dl[i]->d_name);

				st_job_copy_rmdir(subpath);

				free(subpath);
			}

			free(dl[i]);
		}

		free(dl);

		rmdir(path);
	} else {
		unlink(path);
	}
}

int st_job_copy_run(struct st_job * job) {
	struct st_job_copy_private * jp = job->data;

	job->db_ops->add_record(job, st_log_level_info, "Start copy job (job id: %ld), num runs %ld", job->id, job->num_runs);

	int status = st_job_copy_mkdir(job->restore_to->path);
	if (status) {
		job->sched_status = st_job_status_error;
		job->db_ops->add_record(job, st_log_level_error, "Fatal error: failed to create directory (%s)", job->restore_to->path);
		return status;
	}

	// create temporary directory
	jp->restore_path_length = strlen(job->restore_to->path);
	job->restore_to->path = realloc(job->restore_to->path, jp->restore_path_length + 14);
	st_util_string_rtrim(job->restore_to->path, '/');
	strcat(job->restore_to->path, "/stone_XXXXXX");
	mkdtemp(job->restore_to->path);
	jp->restore_path_length += 13;

	unsigned int i;
	for (i = 0; i < job->nb_block_numbers; i++)
		jp->total_size += job->block_numbers[i].size;
	jp->total_size <<= 1;

	if (!jp->stop_request)
		status = st_job_copy_restore_archive(job);

	if (!status && !jp->stop_request) {
		jp->drive->ops->eject(jp->drive);

		if (jp->changer->ops->unload(jp->changer, jp->drive)) {
			job->db_ops->add_record(job, st_log_level_error, "An error occured while unloading tape, job existing");
			status = 1;
		}
	}

	if (!status && !jp->stop_request) {
		struct st_drive * drive = st_job_copy_select_tape(job, select_new_tape);

		struct st_stream_writer * tape_writer = jp->tape_writer = drive->ops->get_writer(drive);
		struct st_stream_writer * checksum_writer = jp->checksum_writer = st_checksum_get_steam_writer((const char **) job->checksums, job->nb_checksums, tape_writer);
		jp->tar_out = st_tar_new_out(checksum_writer);

		// archive
		struct st_archive * archive = st_archive_new(job);
		jp->json = st_io_json_new(archive);
		st_io_json_add_metadata(jp->json, job->job_meta);

		// volume
		jp->current_volume = st_archive_volume_new(job, drive);
		st_io_json_add_volume(jp->json, jp->current_volume);

		jp->block_size = jp->tar_out->ops->get_block_size(jp->tar_out);
		jp->buffer = malloc(jp->block_size);

		char * path;
		asprintf(&path, "%s/%s", job->restore_to->path, jp->first_file);

		status = st_job_copy_archive_file(job, path);

		free(path);

		jp->tar_out->ops->close(jp->tar_out);

		if (!status) {
			struct st_database * driver = st_db_get_default_db();
			jp->db_con = driver->ops->connect(driver, 0);

			// archive
			archive->endtime = jp->current_volume->endtime = time(0);
			st_io_json_update_archive(jp->json, archive);
			// volume
			jp->current_volume->size = tape_writer->ops->position(tape_writer);
			jp->current_volume->endtime = time(0);
			jp->current_volume->digests = st_checksum_get_digest_from_writer(checksum_writer);
			jp->current_volume->nb_checksums = job->nb_checksums;
			st_io_json_update_volume(jp->json, jp->current_volume);

			jp->db_con->ops->new_archive(jp->db_con, archive);
			jp->db_con->ops->update_archive(jp->db_con, archive);

			st_archive_free(archive);

			jp->tar_out->ops->free(jp->tar_out);
			jp->tar_out = 0;

			jp->db_con->ops->close(jp->db_con);
			jp->db_con->ops->free(jp->db_con);

			tape_writer = drive->ops->get_writer(drive);
			st_io_json_write_to(jp->json, tape_writer);
			tape_writer->ops->close(tape_writer);

			tape_writer->ops->free(tape_writer);
			free(tape_writer);
			st_io_json_free(jp->json);
		} else {
			jp->tar_out->ops->free(jp->tar_out);
			jp->tar_out = 0;
		}

		drive = jp->drive;
		tape_writer = jp->tape_writer;
	}

	jp->drive->lock->ops->unlock(jp->drive->lock);

	st_job_copy_rmdir(job->restore_to->path);

	if (jp->stop_request) {
		job->db_ops->add_record(job, st_log_level_error, "Job: copy aborted (job id: %ld), num runs %ld", job->id, job->num_runs);
	} else if (!status) {
		job->done = 1;
		job->db_ops->add_record(job, st_log_level_info, "Job copy (id:%ld) finished with status = OK", job->id);
	} else {
		job->db_ops->add_record(job, st_log_level_error, "Job copy (id:%ld) finished with status = %d", job->id, status);
	}

	return status;
}

void st_job_copy_savepoint_save(struct st_job * job, struct st_changer * changer, struct st_drive * drive, struct st_slot * slot) {
	struct st_job_copy_private * jp = job->data;
	jp->savepoints = realloc(jp->savepoints, (jp->nb_savepoints + 1) * sizeof(struct st_job_copy_savepoint));
	jp->savepoints[jp->nb_savepoints].changer = changer;
	jp->savepoints[jp->nb_savepoints].drive = drive;
	jp->savepoints[jp->nb_savepoints].slot = slot;
	jp->nb_savepoints++;
}

struct st_drive * st_job_copy_select_tape(struct st_job * job, enum st_job_copy_state state) {
	struct st_job_copy_private * jp = job->data;

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

					if (!slot->tape || slot->tape->pool != job->pool || st_job_copy_tape_in_list(jp, slot->tape) || (drive && slot->drive == drive)) {
						slot = 0;
						continue;
					}

					if (slot->drive) {
						if (!slot->drive->lock->ops->trylock(slot->drive->lock)) {
							slot->drive->ops->eod(slot->drive);
							return slot->drive;
						} else {
							st_job_copy_savepoint_save(job, changer, slot->drive, slot);
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

							drive->ops->reset(drive, false, false);
							drive->ops->eod(drive);
							return drive;
						}

						state = look_for_free_drive;
					} else {
						st_job_copy_savepoint_save(job, changer, drive, slot);
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

					drive->ops->reset(drive, false, false);
					drive->ops->eod(drive);
					return drive;
				}
				st_job_copy_savepoint_save(job, changer, drive, slot);
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
							struct st_job_copy_savepoint * sp = jp->savepoints + i;

							changer = sp->changer;
							drive = sp->drive;
							slot = sp->slot;

							if (drive && !drive->lock->ops->trylock(drive->lock)) {
								if (!drive->slot->tape || drive->slot->tape->pool != job->pool) {
									drive->lock->ops->unlock(drive->lock);

									if (i + 1 < jp->nb_savepoints)
										memmove(jp->savepoints + i, jp->savepoints + i + 1, (jp->nb_savepoints - i - 1) * sizeof(struct st_job_copy_savepoint));
									jp->savepoints = realloc(jp->savepoints, (jp->nb_savepoints - 1) * sizeof(struct st_job_copy_savepoint));
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
										memmove(jp->savepoints + i, jp->savepoints + i + 1, (jp->nb_savepoints - i - 1) * sizeof(struct st_job_copy_savepoint));
									jp->savepoints = realloc(jp->savepoints, (jp->nb_savepoints - 1) * sizeof(struct st_job_copy_savepoint));
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

									drive->ops->reset(drive, false, false);
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
				changer = jp->drive->changer;
				drive = jp->drive;
				state = changer_got_tape;
				break;
		}
	}
	return 0;
}

int st_job_copy_stop(struct st_job * job __attribute__((unused))) {
	struct st_job_copy_private * self = job->data;
	if (self && !self->stop_request) {
		self->stop_request = 1;
		job->db_ops->add_record(job, st_log_level_warning, "Job: Stop requested");
		return 0;
	}
	return 1;
}

int st_job_copy_tape_in_list(struct st_job_copy_private * jp, struct st_tape * tape) {
	unsigned int i;
	for (i = 0; i < jp->nb_tapes; i++)
		if (jp->tapes[i] == tape)
			return 1;
	return 0;
}

