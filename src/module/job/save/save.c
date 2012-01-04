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
*  Last modified: Wed, 04 Jan 2012 16:53:12 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// open
#include <fcntl.h>
// scandir
#include <dirent.h>
// free, malloc
#include <stdlib.h>
// asprintf
#include <stdio.h>
// strcmp
#include <string.h>
// open, stat
#include <sys/stat.h>
// open, stat
#include <sys/types.h>
// access, close, sleep, stat
#include <unistd.h>

#include <stone/io.h>
#include <stone/job.h>
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
	ssize_t total_size;
};

static void st_job_save_archive_file(struct st_job * job, const char * path);
static int st_job_save_compute_filter(const struct dirent * file);
static ssize_t st_job_save_compute_total_size(struct st_job * job, const char * path);
static void st_job_save_free(struct st_job * job);
static void st_job_save_init(void) __attribute__((constructor));
static void st_job_save_new_job(struct st_database_connection * db, struct st_job * job);
static int st_job_save_run(struct st_job * job);
static int st_job_save_stop(struct st_job * job);

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


void st_job_save_archive_file(struct st_job * job, const char * path) {
	struct st_job_save_private * jp = job->data;

	struct stat st;
	if (stat(path, &st)) {
		job->db_ops->add_record(job, "Error while getting information about: %s", path);
		return;
	}

	if (S_ISSOCK(st.st_mode))
		return;

	struct st_tar_header header;
	jp->tar->ops->add_file(jp->tar, path, &header);

	if (S_ISREG(st.st_mode)) {
		int fd = open(path, O_RDONLY);

		ssize_t nb_read;
		while ((nb_read = read(fd, jp->buffer, jp->block_size)) > 0) {
			jp->tar->ops->write(jp->tar, jp->buffer, nb_read);
		}

		close(fd);
	} else if (S_ISDIR(st.st_mode)) {
		if (access(path, R_OK | X_OK)) {
			job->db_ops->add_record(job, "Error, Can't read directory: %s", path);
			return;
		}

		struct dirent ** dl = 0;
		int nb_files = scandir(path, &dl, st_job_save_compute_filter, 0);

		int i;
		for (i = 0; i < nb_files; i++) {
			char * subpath = 0;
			asprintf(&subpath, "%s/%s", path, dl[i]->d_name);

			st_job_save_archive_file(job, subpath);

			free(subpath);
			free(dl[i]);
		}

		free(dl);
	}
}

int st_job_save_compute_filter(const struct dirent * file) {
	if (!strcmp(file->d_name, "."))
		return 0;
	return strcmp(file->d_name, "..");
}

ssize_t st_job_save_compute_total_size(struct st_job * job, const char * path) {
	struct stat st;

	if (stat(path, &st)) {
		job->db_ops->add_record(job, "Error while getting information about: %s", path);
		return 0;
	}

	if (S_ISREG(st.st_mode)) {
		if (access(path, R_OK))
			job->db_ops->add_record(job, "Error, Can't read file: %s", path);
		return st.st_size;
	}

	ssize_t total = 0;
	if (S_ISDIR(st.st_mode)) {
		if (access(path, R_OK | X_OK)) {
			job->db_ops->add_record(job, "Error, Can't read directory: %s", path);
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

void st_job_save_new_job(struct st_database_connection * db, struct st_job * job) {
	struct st_job_save_private * self = malloc(sizeof(struct st_job_save_private));
	self->tar = 0;
	self->total_size = 0;

	job->data = self;
	job->job_ops = &st_job_save_ops;
}

int st_job_save_run(struct st_job * job) {
	struct st_job_save_private * jp = job->data;

	job->db_ops->add_record(job, "Start archive job (job id: %ld), num runs %ld", job->id, job->num_runs);

	// check permission
	struct st_user * user = job->user;
	if (!user->can_archive) {
		job->sched_status = st_job_status_error;
		job->db_ops->add_record(job, "Error: user (%s) is not allowed to create archive", user->fullname);
		return 1;
	}

	// get changer
	struct st_changer * changer = st_changer_get_first_changer();
	if (!changer) {
		job->sched_status = st_job_status_error;
		job->db_ops->add_record(job, "Error: There is no changer");
		return 2;
	}

	unsigned short nb_tries = 1;
	do {
		if (changer->lock->ops->trylock(changer->lock)) {
			changer = st_changer_get_next_changer(changer);
			if (!changer) {
				job->sched_status = st_job_status_pause;
				if (nb_tries == 1)
					job->db_ops->add_record(job, "Waiting for free changer");
				sleep(60);
				changer = st_changer_get_first_changer();
			}
			job->sched_status = st_job_status_running;
			nb_tries++;
			if (nb_tries < 1)
				nb_tries = 2;
		} else {
			break;
		}
	} while (nb_tries);
	job->db_ops->add_record(job, "Got changer: %s %s", changer->vendor, changer->model);

	// get free drive
	struct st_drive * drive = changer->ops->get_free_drive(changer);
	if (drive->lock->ops->trylock(drive->lock)) {
		changer->lock->ops->unlock(changer->lock);

		job->sched_status = st_job_status_pause;
		job->db_ops->add_record(job, "Waiting for drive: %s %s", drive->vendor, drive->model);

		drive->lock->ops->lock(drive->lock);
		job->sched_status = st_job_status_running;
	}
	job->db_ops->add_record(job, "Got drive: %s %s", drive->vendor, drive->model);

	// check tape and pool
	if (job->pool) {
	}

	// compute total files size
	unsigned int i;
	for (i = 0; i < job->nb_paths; i++)
		jp->total_size += st_job_save_compute_total_size(job, job->paths[i]);

	if (jp->total_size == 0) {
		job->db_ops->add_record(job, "Error, there is no file to archive or total size of all files is null");
		return 3;
	}

	char bufsize[32];
	st_util_convert_size_to_string(jp->total_size, bufsize, 32);
	job->db_ops->add_record(job, "Will archive %s", bufsize);

	// select one tape
	struct st_tape * tape = drive->slot->tape;
	if (!tape) {
		changer->lock->ops->lock(changer->lock);

		struct st_slot * sl = changer->ops->get_tape(changer, job->pool);

		if (changer->ops->can_load()) {
			if (!sl && job->pool) {
				st_log_write_all2(st_log_level_warning, st_log_type_user_message, job->user, "Your library doesn't contains candidate tape, please insert a new one or one which is member of pool (%s)", job->pool->name);

				job->sched_status = st_job_status_pause;
				job->db_ops->add_record(job, "Warning: Waiting for new tape");
			} else if (!sl) {
				st_log_write_all2(st_log_level_warning, st_log_type_user_message, job->user, "Your library doesn't contains candidate tape, please insert a new one");

				job->sched_status = st_job_status_pause;
				job->db_ops->add_record(job, "Warning: Waiting for new tape");
			}

			while (!sl) {
				changer->lock->ops->unlock(changer->lock);
				sleep(60);
				changer->lock->ops->lock(changer->lock);
				sl = changer->ops->get_tape(changer, job->pool);
			}

			job->sched_status = st_job_status_running;
		} else {
		}

		if (!sl->drive) {
			job->db_ops->add_record(job, "Loading tape (%s)", sl->tape->label);
			changer->ops->load(changer, sl, drive);
		}
	}

	struct st_stream_writer * tape_writer = drive->ops->get_writer(drive);
	struct st_stream_writer * checksum_writer = st_checksum_get_steam_writer((const char **) job->checksums, job->nb_checksums, tape_writer);
	jp->tar = st_tar_new_out(checksum_writer);

	jp->block_size = jp->tar->ops->get_block_size(jp->tar);
	jp->buffer = malloc(jp->block_size);

	for (i = 0; i < job->nb_paths; i++)
		st_job_save_archive_file(job, job->paths[i]);

	jp->tar->ops->close(jp->tar);

	return 0;
}

int st_job_save_stop(struct st_job * job) {
	return 0;
}

