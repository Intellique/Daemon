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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sun, 15 Jan 2012 11:54:59 +0100                         *
\*************************************************************************/

// errno
#include <errno.h>
// open
#include <fcntl.h>
// free, malloc
#include <stdlib.h>
// strchr
#include <string.h>
// S_*
#include <sys/stat.h>
// S_*
#include <sys/types.h>
// sleep
#include <unistd.h>


#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/ressource.h>
#include <stone/library/tape.h>
#include <stone/job.h>
#include <stone/log.h>
#include <stone/tar.h>
#include <stone/user.h>

struct st_job_restore_private {
	char * buffer;
	ssize_t length;
};

static int st_job_restore_filter(struct st_job * job, struct st_tar_header * header);
static void st_job_restore_free(struct st_job * job);
static void st_job_restore_init(void) __attribute__((constructor));
static int st_job_restore_mkdir(const char * path);
static void st_job_restore_new_job(struct st_database_connection * db, struct st_job * job);
static int st_job_restore_restore(struct st_job * job, struct st_drive * drive);
static int st_job_restore_restore_file(struct st_job * job, struct st_tar_in * tar, struct st_tar_header * header, const char * path);
static int st_job_restore_run(struct st_job * job);
static int st_job_restore_stop(struct st_job * job);
static char * st_job_restore_trunc_filename(char * path, int nb_trunc_path);

struct st_job_ops st_job_restore_ops = {
	.free = st_job_restore_free,
	.run  = st_job_restore_run,
	.stop = st_job_restore_stop,
};

struct st_job_driver st_job_restore_driver = {
	.name        = "restore",
	.new_job     = st_job_restore_new_job,
	.cookie      = 0,
	.api_version = STONE_JOB_APIVERSION,
};


int st_job_restore_filter(struct st_job * job, struct st_tar_header * header) {
	if (job->nb_paths == 0)
		return 1;

	unsigned int i;
	for (i = 0; i < job->nb_paths; i++) {
		ssize_t length = strlen(job->paths[i]);

		if (!strncmp(header->path, job->paths[i], length))
			return 1;
	}

	return 0;
}

void st_job_restore_free(struct st_job * job) {
	free(job->data);
}

void st_job_restore_init() {
	st_job_register_driver(&st_job_restore_driver);
}

int st_job_restore_mkdir(const char * path) {
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

void st_job_restore_new_job(struct st_database_connection * db __attribute__((unused)), struct st_job * job) {
	struct st_job_restore_private * self = malloc(sizeof(struct st_job_restore_private));
	self->buffer = 0;
	self->length = 0;

	job->data = self;
	job->job_ops = &st_job_restore_ops;
}

int st_job_restore_restore(struct st_job * job, struct st_drive * drive) {
	struct st_job_restore_private * jp = job->data;

	struct st_stream_reader * tape_reader = drive->ops->get_reader(drive);
	struct st_tar_in * tar = st_tar_new_in(tape_reader);

	jp->length = tar->ops->get_block_size(tar);
	jp->buffer = malloc(jp->length);

	struct st_tar_header header;
	enum st_tar_header_status status;
	ssize_t restore_path_length = strlen(job->restore_to->path);
	int failed = 0;

	while ((status = tar->ops->get_header(tar, &header)) == ST_TAR_HEADER_OK) {
		if (!st_job_restore_filter(job, &header)) {
			tar->ops->skip_file(tar);
			continue;
		}

		char * path = header.path;
		if (job->restore_to->nb_trunc_path > 0)
			path = st_job_restore_trunc_filename(path, job->restore_to->nb_trunc_path);

		ssize_t length = restore_path_length + strlen(path);
		char * restore_path = malloc(length + 2);

		strcpy(restore_path, job->restore_to->path);

		if (restore_path[restore_path_length - 1] == '/')
			strcpy(restore_path + restore_path_length, path);
		else {
			strcpy(restore_path + restore_path_length, "/");
			strcpy(restore_path + restore_path_length + 1, path);
		}

		failed = st_job_restore_restore_file(job, tar, &header, path);

		free(restore_path);

		if (failed)
			break;
	}

	free(jp->buffer);

	tar->ops->close(tar);
	tar->ops->free(tar);

	return status != ST_TAR_HEADER_NOT_FOUND || failed;
}

int st_job_restore_restore_file(struct st_job * job, struct st_tar_in * tar, struct st_tar_header * header, const char * path) {
	if (header->is_label)
		return 0;

	int status = 0;

	if (header->link[0] != '\0' && !(header->mode & S_IFMT)) {
		status = link(header->link, path);
		if (status)
			job->db_ops->add_record(job, "Warning: failed to create an hardlink from %s to %s because %m", path, header->link);
	} else if (S_ISFIFO(header->mode)) {
		status = mknod(path, S_IFIFO, 0);
		if (status)
			job->db_ops->add_record(job, "Warning: failed to create a fifo named %s because %m", path);
	} else if (S_ISCHR(header->mode)) {
		status = mknod(path, S_IFCHR, header->dev);
		if (status)
			job->db_ops->add_record(job, "Warning: failed to create a character device (major:%d,minor:%d) named %s because %m", (int) header->dev >> 8, (int) header->dev & 0xFF, path);
	} else if (S_ISDIR(header->mode)) {
		status = mkdir(path, header->mode);
		if (status)
			job->db_ops->add_record(job, "Warning: failed to create a directory named %s because %m", path);
	} else if (S_ISBLK(header->mode)) {
		status = mknod(path, S_IFBLK, header->dev);
		if (status)
			job->db_ops->add_record(job, "Warning: failed to create a block device (major:%d,minor:%d) named %s because %m", (int) header->dev >> 8, (int) header->dev & 0xFF, path);
	} else if (S_ISREG(header->mode)) {
		int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, header->mode);
		if (fd < 0) {
			job->db_ops->add_record(job, "Warning: failed to create a file named %s because %m", path);
			return status;
		}

		struct st_job_restore_private * jp = job->data;

		ssize_t nb_read, nb_total_read = 0;
		while ((nb_read = tar->ops->read(tar, jp->buffer, jp->length)) > 0) {
			ssize_t nb_write = write(fd, jp->buffer, nb_read);
			if (nb_write < 0) {
				job->db_ops->add_record(job, "Warning: an error occured while writing to file named %s because %m", path);
			}

			nb_total_read += nb_read;
		}

		if (nb_read < 0)
			job->db_ops->add_record(job, "Warning: an error occured while reading from tape because %m");

		close(fd);

	} else if (S_ISLNK(header->mode)) {
		status = symlink(header->link, header->path);
	}

	return status;
}

int st_job_restore_run(struct st_job * job) {
	job->db_ops->add_record(job, "Start restore job (job id: %ld), num runs %ld", job->id, job->num_runs);

	// check permission
	struct st_user * user = job->user;
	if (!user->can_restore) {
		job->sched_status = st_job_status_error;
		job->db_ops->add_record(job, "Error: user (%s) is not allowed to create archive", user->fullname);
		return 1;
	}

	int status = st_job_restore_mkdir(job->restore_to->path);
	if (status) {
		job->sched_status = st_job_status_error;
		job->db_ops->add_record(job, "Fatal error: failed to create directory (%s)", job->restore_to->path);
		return status;
	}

	enum {
		alert_user,
		drive_is_free,
		look_for_changer,
		look_for_free_drive,
		tape_is_in_changer,
		tape_is_in_drive,
	} state = look_for_changer;

	struct st_changer * changer = 0;
	struct st_drive * drive = 0;
	struct st_slot * slot = 0;
	short has_alert_user = 0;

	unsigned i, j;
	for (i = 0; i < job->nb_tapes; i++) {
		struct st_job_tape * tape = job->tapes + i;

		short stop = 0;
		while (!stop) {
			switch (state) {
				case alert_user:
					job->sched_status = st_job_status_pause;
					job->db_ops->update_status(job);

					if (!has_alert_user) {
						job->db_ops->add_record(job, "Tape not found (named: %s)", tape->tape->name);
						st_log_write_all(st_log_level_error, st_log_type_user_message, "Job: format tape (id:%ld) request you to put a tape (named: %s) in your changer or standalone drive", job->id, tape->tape->name);
					}
					has_alert_user = 1;
					sleep(15);

					job->sched_status = st_job_status_running;
					job->db_ops->update_status(job);

					state = look_for_changer;
					break;

				case drive_is_free:
					if (!drive->lock->ops->trylock(drive->lock)) {
						drive->ops->set_file_position(drive, tape->tape_position);
						st_job_restore_restore(job, drive);
						state = tape_is_in_changer;
						stop = 1;
					} else {
						state = tape_is_in_changer;
						sleep(15);
					}
					break;

				case look_for_changer:
					changer = st_changer_get_by_tape(tape->tape);

					if (changer) {
						slot = changer->ops->get_tape(changer, tape->tape);
						drive = slot->drive;
						state = drive ? drive_is_free : look_for_free_drive;
					} else {
						state = alert_user;
					}
					break;

				case look_for_free_drive:
					slot->lock->ops->lock(slot->lock);

					if (slot->tape != tape->tape) {
						// it seem that someone has load tape before
						slot->lock->ops->unlock(slot->lock);

						changer = 0;
						drive = 0;
						slot = 0;

						state = look_for_changer;
						break;
					}

					drive = 0;
					for (j = 0; j < changer->nb_drives && !drive; j++) {
						drive = changer->drives + j;

						if (drive->lock->ops->trylock(drive->lock))
							drive = 0;
					}

					if (drive) {
						changer->lock->ops->lock(changer->lock);
						changer->ops->load(changer, slot, drive);
						changer->lock->ops->unlock(changer->lock);
						slot->lock->ops->unlock(slot->lock);
						drive->ops->reset(drive);

						drive->ops->set_file_position(drive, tape->tape_position);
						st_job_restore_restore(job, drive);
						state = tape_is_in_changer;
						stop = 1;
					} else {
						slot->lock->ops->unlock(slot->lock);
						sleep(15);
						state = look_for_changer;
					}
					break;

				case tape_is_in_changer:
					slot = changer->ops->get_tape(changer, tape->tape);

					if (slot && slot->drive) {
						if (!slot->drive->lock->ops->trylock(slot->drive->lock)) {
							drive->lock->ops->unlock(drive->lock);
							drive = slot->drive;

							drive->ops->set_file_position(drive, tape->tape_position);
							st_job_restore_restore(job, drive);
							stop = 1;
						} else {
							drive->lock->ops->unlock(drive->lock);
							drive = slot->drive;
							state = drive_is_free;
						}
					} else if (slot) {
						struct st_slot * sl = 0;
						for (j = changer->nb_drives; !sl && j < changer->nb_slots; i++) {
							struct st_slot * ptrsl = changer->slots + j;
							if (ptrsl->lock->ops->trylock(ptrsl->lock))
								continue;
							if (ptrsl->address == drive->slot->src_address)
								sl = ptrsl;
							else
								ptrsl->lock->ops->unlock(ptrsl->lock);
						}
						for (j = changer->nb_drives; !sl && j < changer->nb_slots; i++) {
							struct st_slot * ptrsl = changer->slots + j;
							if (ptrsl->lock->ops->trylock(ptrsl->lock))
								continue;
							if (!ptrsl->full)
								sl = ptrsl;
							else
								ptrsl->lock->ops->unlock(ptrsl->lock);
						}

						if (!sl) {
							// panic, no slot for unloading tape
						}

						drive->ops->eject(drive);
					} else {
						drive->lock->ops->unlock(drive->lock);
						state = look_for_changer;
					}
					break;

				case tape_is_in_drive:
					if (drive->slot->tape == tape->tape)
						state = drive_is_free;
					break;
			}
		}
	}

	return 0;
}

int st_job_restore_stop(struct st_job * job __attribute__((unused))) {
	return 0;
}

char * st_job_restore_trunc_filename(char * path, int nb_trunc_path) {
	while (nb_trunc_path > 0 && path) {
		path = strchr(path, '/');
		nb_trunc_path--;
	}

	return path;
}

