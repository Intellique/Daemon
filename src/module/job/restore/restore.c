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
*  Last modified: Fri, 07 Sep 2012 10:04:09 +0200                         *
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


#include <stone/io.h>
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/ressource.h>
#include <stone/library/tape.h>
#include <stone/job.h>
#include <stone/log.h>
#include <stone/tar.h>
#include <stone/user.h>
#include <stone/util.h>

struct st_job_restore_private {
	char * buffer;
	ssize_t length;

	char * previous_file;
	ssize_t position;
	ssize_t nb_total_read_partial; // used for restoring partial archive
	ssize_t nb_total_read_total; // used for restoring complete archive
	ssize_t total_size;
	void (*compute_progress)(struct st_job * job, struct st_job_restore_private * jp);

	struct st_changer * changer;
	struct st_drive * drive;
	struct st_slot * slot;

	int stop_request;
};

static void st_job_restore_compute_complete_progress(struct st_job * job, struct st_job_restore_private * jp);
static void st_job_restore_compute_partial_progress(struct st_job * job, struct st_job_restore_private * jp);
static int st_job_restore_filter(struct st_job_block_number * block_number, struct st_tar_header * header);
static void st_job_restore_free(struct st_job * job);
static void st_job_restore_init(void) __attribute__((constructor));
static int st_job_restore_load_tape(struct st_job * job, struct st_tape * tape);
static int st_job_restore_mkdir(const char * path);
static void st_job_restore_new_job(struct st_database_connection * db, struct st_job * job);
static int st_job_restore_restore_archive(struct st_job * job);
static int st_job_restore_restore_file(struct st_job * job, struct st_tar_in * tar, struct st_tar_header * header, const char * path);
static int st_job_restore_restore_files(struct st_job * job);
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


void st_job_restore_compute_complete_progress(struct st_job * job, struct st_job_restore_private * jp) {
	job->done = (float) jp->nb_total_read_total / jp->total_size;
	job->db_ops->update_status(job);
}

void st_job_restore_compute_partial_progress(struct st_job * job, struct st_job_restore_private * jp) {
	job->done = (float) jp->nb_total_read_partial / jp->total_size;
	job->db_ops->update_status(job);
}

int st_job_restore_filter(struct st_job_block_number * block_number, struct st_tar_header * header) {
	if (block_number->path == NULL)
		return 1;

	char * path = block_number->path;
	if (*header->path != '/')
		path++;

	char * tar_header = strdup(header->path);
	st_util_string_delete_double_char(tar_header, '/');

	int diff;
	if (header->offset > 0)
		diff = strncmp(tar_header, path, strlen(header->path));
	else
		diff = strcmp(tar_header, path);

	free(tar_header);

	return diff == 0 ? 1 : 0;
}

void st_job_restore_free(struct st_job * job) {
	free(job->data);
}

void st_job_restore_init() {
	st_job_register_driver(&st_job_restore_driver);
}

int st_job_restore_load_tape(struct st_job * job, struct st_tape * tape) {
	struct st_job_restore_private * jp = job->data;
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
					st_log_write_all(st_log_level_error, st_log_type_user_message, "Job: restore (id:%ld) request you to put a tape (named: %s) in your changer or standalone drive", job->id, tape->name);
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

					if (jp->drive->lock->ops->trylock(jp->drive->lock)) {
						jp->drive = 0;
						continue;
					}

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
						// find original slot of loaded tape in selected drive
						struct st_slot * sl = 0;
						for (i = jp->changer->nb_drives; i < jp->changer->nb_slots; i++) {
							sl = jp->changer->slots + i;

							if (!sl->tape && sl->address == jp->drive->slot->src_address && !sl->lock->ops->trylock(sl->lock))
								break;

							sl = 0;
						}

						// if original slot is not found
						for (i = jp->changer->nb_drives; !sl && i < jp->changer->nb_slots; i++) {
							sl = jp->changer->slots + i;

							if (!sl->tape && !sl->lock->ops->trylock(sl->lock))
								break;

							sl = 0;
						}

						if (!sl) {
							// no free slot for unloading tape
							job->sched_status = st_job_status_error;
							job->db_ops->add_record(job, st_log_level_error, "No slot free for unloading tape");
							return 1;
						}

						jp->drive->ops->eject(jp->drive);
						jp->changer->lock->ops->lock(jp->changer->lock);
						jp->changer->ops->unload(jp->changer, jp->drive, sl);
						jp->changer->lock->ops->unlock(jp->changer->lock);
						sl->lock->ops->unlock(sl->lock);
					}

					jp->changer->lock->ops->lock(jp->changer->lock);
					jp->changer->ops->load(jp->changer, jp->slot, jp->drive);
					jp->changer->lock->ops->unlock(jp->changer->lock);
					jp->slot->lock->ops->unlock(jp->slot->lock);
					jp->drive->ops->reset(jp->drive);

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

int st_job_restore_mkdir(const char * path) {
	if (!access(path, F_OK))
		return 0;

	char * path2 = strdup(path);
	st_util_string_delete_double_char(path2, '/');

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

	self->previous_file = NULL;
	self->position = 0;
	self->nb_total_read_partial = 0;
	self->nb_total_read_total = 0;
	self->total_size = 0;
	self->compute_progress = st_job_restore_compute_complete_progress;

	self->changer = 0;
	self->drive = 0;
	self->slot = 0;

	self->stop_request = 0;

	job->data = self;
	job->job_ops = &st_job_restore_ops;
}

int st_job_restore_restore_archive(struct st_job * job) {
	struct st_job_restore_private * jp = job->data;
	jp->compute_progress = st_job_restore_compute_complete_progress;

	unsigned int i;
	int status = 0;
	for (i = 0; i < job->nb_block_numbers && !status && !jp->stop_request; i++) {
		struct st_job_block_number * bn = job->block_numbers + i;

		status = st_job_restore_load_tape(job, bn->tape);
		if (status) {
			job->db_ops->add_record(job, st_log_level_error, "Failed to load tape: %s", bn->tape->name);
			return status;
		}

		jp->drive->ops->set_file_position(jp->drive, bn->tape_position);

		struct st_stream_reader * tape_reader = jp->drive->ops->get_reader(jp->drive);
		struct st_tar_in * tar = st_tar_new_in(tape_reader);

		jp->length = tar->ops->get_block_size(tar);
		jp->buffer = malloc(jp->length);

		struct st_tar_header header;
		enum st_tar_header_status hdr_status;
		ssize_t restore_path_length = strlen(job->restore_to->path);
		int failed = 0;

		while ((hdr_status = tar->ops->get_header(tar, &header)) == ST_TAR_HEADER_OK && !jp->stop_request) {
			char * path = header.path;
			if (job->restore_to->nb_trunc_path > 0)
				path = st_job_restore_trunc_filename(path, job->restore_to->nb_trunc_path);

			if (!path)
				continue;

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
				st_job_restore_mkdir(restore_path);
				*pos = '/';
			}

			failed = st_job_restore_restore_file(job, tar, &header, restore_path);

			free(restore_path);
			st_tar_free_header(&header);

			if (failed)
				break;
		}

		free(jp->buffer);

		tar->ops->close(tar);
		tar->ops->free(tar);

		status = (hdr_status != ST_TAR_HEADER_NOT_FOUND || failed);
	}

	return status;
}

int st_job_restore_restore_file(struct st_job * job, struct st_tar_in * tar, struct st_tar_header * header, const char * path) {
	if (header->is_label)
		return 0;

	if (header->path)
		st_util_string_delete_double_char(header->path, '/');
	if (header->link)
		st_util_string_delete_double_char(header->link, '/');

	struct st_job_restore_private * jp = job->data;
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

		status = st_job_restore_mkdir(path);
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

		const char * filename = path;
		if (header->offset > 0) {
			filename = jp->previous_file;
		} else {
			if (jp->previous_file != NULL)
				free(jp->previous_file);
			jp->previous_file = strdup(path);
		}

		int fd = open(filename, O_CREAT | O_WRONLY, header->mode);
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

		struct st_job_restore_private * jp = job->data;

		ssize_t nb_read, nb_total_read = 0;
		while ((nb_read = tar->ops->read(tar, jp->buffer, jp->length)) > 0) {
			ssize_t nb_write = write(fd, jp->buffer, nb_read);
			if (nb_write < 0) {
				job->db_ops->add_record(job, st_log_level_warning, "An error occured while writing to file named %s because %m", path);
			}

			nb_total_read += nb_read;

			jp->nb_total_read_partial += nb_read;
			jp->nb_total_read_total = jp->position + tar->ops->position(tar);

			jp->compute_progress(job, jp);
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

	jp->nb_total_read_total = jp->position + tar->ops->position(tar);
	jp->compute_progress(job, jp);

	return status;
}

int st_job_restore_restore_files(struct st_job * job) {
	struct st_job_restore_private * jp = job->data;
	jp->compute_progress = st_job_restore_compute_partial_progress;

	unsigned int i;
	int status = 0;
	ssize_t restore_path_length = strlen(job->restore_to->path);

	for (i = 0; i < job->nb_block_numbers && !status && !jp->stop_request;) {
		struct st_job_block_number * bn = job->block_numbers + i;

		status = st_job_restore_load_tape(job, bn->tape);
		if (status) {
			job->db_ops->add_record(job, st_log_level_error, "Failed to load tape: %s", bn->tape->name);
			return status;
		}

		jp->drive->ops->set_file_position(jp->drive, bn->tape_position);

		long current_sequence = bn->sequence;
		struct st_stream_reader * tape_reader = jp->drive->ops->get_reader(jp->drive);

		jp->length = tape_reader->ops->get_block_size(tape_reader);
		jp->buffer = malloc(jp->length);

		struct st_tar_in * tar = st_tar_new_in(tape_reader);

		while (i < job->nb_block_numbers && bn->sequence == current_sequence && !jp->stop_request) {
			if (bn->block_number > 0)
				tape_reader->ops->set_position(tape_reader, bn->block_number);

			enum st_tar_header_status hdr_status = ST_TAR_HEADER_BAD_CHECKSUM;
			struct st_tar_header header;

			while ((hdr_status = tar->ops->get_header(tar, &header)) != ST_TAR_HEADER_OK);

			while (hdr_status == ST_TAR_HEADER_OK && !st_job_restore_filter(bn, &header) && !jp->stop_request) {
				st_tar_free_header(&header);
				tar->ops->skip_file(tar);
				hdr_status = tar->ops->get_header(tar, &header);
			}

			if (hdr_status == ST_TAR_HEADER_OK && st_job_restore_filter(bn, &header) && !jp->stop_request) {
				char * path = header.path;
				if (job->restore_to->nb_trunc_path > 0)
					path = st_job_restore_trunc_filename(path, job->restore_to->nb_trunc_path);

				if (!path) {
					tar->ops->skip_file(tar);
					hdr_status = tar->ops->get_header(tar, &header);
					continue;
				}

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
					st_job_restore_mkdir(restore_path);
					*pos = '/';
				}

				int failed = st_job_restore_restore_file(job, tar, &header, restore_path);

				free(restore_path);
				st_tar_free_header(&header);

				if (failed)
					break;
			}

			i++;
			bn = job->block_numbers + i;
		}

		tar->ops->close(tar);
		tar->ops->free(tar);
		free(jp->buffer);
		jp->buffer = 0;
	}

	return status;
}

int st_job_restore_run(struct st_job * job) {
	struct st_job_restore_private * jp = job->data;

	job->db_ops->add_record(job, st_log_level_info, "Start restore job (job id: %ld), num runs %ld", job->id, job->num_runs);

	// check permission
	struct st_user * user = job->user;
	if (!user->can_restore) {
		job->sched_status = st_job_status_error;
		job->db_ops->add_record(job, st_log_level_error, "User (%s) is not allowed to create archive", user->fullname);
		return 1;
	}

	int status = st_job_restore_mkdir(job->restore_to->path);
	if (status) {
		job->sched_status = st_job_status_error;
		job->db_ops->add_record(job, st_log_level_error, "Fatal error: failed to create directory (%s)", job->restore_to->path);
		return status;
	}

	unsigned int i;
	char * last_file = NULL;
	for (i = 0; i < job->nb_block_numbers; i++) {
		int diff = last_file != NULL && job->block_numbers[i].path != NULL ? strcmp(last_file, job->block_numbers[i].path) : 1;
		if (diff != 0)
			jp->total_size += job->block_numbers[i].size;

		if (strlen(job->block_numbers[i].path) > 0)
			last_file = job->block_numbers[i].path;
	}

	if (job->nb_paths > 0)
		status = st_job_restore_restore_files(job);
	else
		status = st_job_restore_restore_archive(job);

	jp->drive->lock->ops->unlock(jp->drive->lock);

	if (jp->previous_file != NULL)
		free(jp->previous_file);
	jp->previous_file = NULL;

	if (jp->stop_request) {
		job->db_ops->add_record(job, st_log_level_error, "Job: restore aborted (job id: %ld), num runs %ld", job->id, job->num_runs);
	} else if (!status) {
		job->done = 1;
		job->db_ops->add_record(job, st_log_level_info, "Job restore (id:%ld) finished with status = OK", job->id);
	} else
		job->db_ops->add_record(job, st_log_level_error, "Job restore (id:%ld) finished with status = %d", job->id, status);

	return status;
}

int st_job_restore_stop(struct st_job * job) {
	struct st_job_restore_private * self = job->data;
	if (self && !self->stop_request) {
		self->stop_request = 1;
		job->db_ops->add_record(job, st_log_level_warning, "Job: Stop requested");
		return 0;
	}
	return 1;
}

char * st_job_restore_trunc_filename(char * path, int nb_trunc_path) {
	while (nb_trunc_path > 0 && path) {
		path = strchr(path, '/');
		nb_trunc_path--;

		if (path && *path == '/')
			path++;
	}

	return path;
}

