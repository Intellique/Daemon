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
*  Last modified: Fri, 25 May 2012 11:13:13 +0200                         *
\*************************************************************************/

// malloc, realloc
#include <stdlib.h>
// memmove
#include <string.h>
// sleep
#include <unistd.h>

#include <stone/database.h>
#include <stone/io.h>
#include <stone/job.h>
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/ressource.h>
#include <stone/library/tape.h>
#include <stone/log.h>
#include <stone/user.h>

struct st_job_backup_private {
	struct st_drive * current_drive;

	struct st_job_save_savepoint {
		struct st_changer * changer;
		struct st_drive * drive;
		struct st_slot * slot;
	} * savepoints;
	unsigned int nb_savepoints;

	struct st_tape ** tapes;
	unsigned int nb_tapes;
};

enum st_job_backup_state {
	changer_got_tape,
	look_for_free_drive,
	look_in_first_changer,
	look_in_next_changer,
	select_new_tape,
};

static void st_job_backupdb_free(struct st_job * job);
static void st_job_backupdb_init(void) __attribute__((constructor));
static void st_job_backupdb_new_job(struct st_database_connection * db, struct st_job * job);
static int st_job_backupdb_run(struct st_job * job);
static void st_job_backupdb_savepoint_save(struct st_job * job, struct st_changer * changer, struct st_drive * drive, struct st_slot * slot);
static struct st_drive * st_job_backupdb_select_tape(struct st_job * job, enum st_job_backup_state state);
static int st_job_backupdb_stop(struct st_job * job);
static int st_job_backupdb_tape_in_list(struct st_job_backup_private * jp, struct st_tape * tape);

struct st_job_ops st_job_backupdb_ops = {
	.free = st_job_backupdb_free,
	.run  = st_job_backupdb_run,
	.stop = st_job_backupdb_stop,
};


struct st_job_driver st_job_backupdb_driver = {
	.name        = "backup-db",
	.new_job     = st_job_backupdb_new_job,
	.cookie      = 0,
	.api_version = STONE_JOB_APIVERSION,
};

void st_job_backupdb_free(struct st_job * job __attribute__((unused))) {}

void st_job_backupdb_init() {
	st_job_register_driver(&st_job_backupdb_driver);
}

void st_job_backupdb_new_job(struct st_database_connection * db __attribute__((unused)), struct st_job * job) {
	job->data = 0;
	job->job_ops = &st_job_backupdb_ops;
}

int st_job_backupdb_run(struct st_job * job) {
	job->db_ops->add_record(job, st_log_level_info, "Start backup job (job id: %ld), num runs %ld", job->id, job->num_runs);

	if (job->user->is_admin) {
		job->sched_status = st_job_status_error;
		job->db_ops->add_record(job, st_log_level_error, "User (%s) is not allowed to backup database", job->user->fullname);
	}

	// check tape and pool
	if (!job->pool)
		job->pool = st_pool_get_by_uuid("d9f976d4-e087-4d0a-ab79-96267f6613f0"); // pool Stone_Db_Backup

	struct st_database * driver = st_db_get_default_db();
	struct st_stream_reader * db_reader = driver->ops->backup_db(driver);

	struct st_drive * drive = st_job_backupdb_select_tape(job, look_in_first_changer);
	drive->ops->eod(drive);

	struct st_stream_writer * tape_writer = drive->ops->get_writer(drive);

	ssize_t block_size = tape_writer->ops->get_block_size(tape_writer);
	char * buffer = malloc(block_size);

	ssize_t nb_read;
	while ((nb_read = db_reader->ops->read(db_reader, buffer, block_size)) > 0) {
		tape_writer->ops->write(tape_writer, buffer, nb_read);
	}

	tape_writer->ops->close(tape_writer);
	tape_writer->ops->free(tape_writer);

	db_reader->ops->close(db_reader);
	db_reader->ops->free(db_reader);

	return 0;
}

void st_job_backupdb_savepoint_save(struct st_job * job, struct st_changer * changer, struct st_drive * drive, struct st_slot * slot) {
	struct st_job_backup_private * jp = job->data;
	jp->savepoints = realloc(jp->savepoints, (jp->nb_savepoints + 1) * sizeof(struct st_job_save_savepoint));
	jp->savepoints[jp->nb_savepoints].changer = changer;
	jp->savepoints[jp->nb_savepoints].drive = drive;
	jp->savepoints[jp->nb_savepoints].slot = slot;
	jp->nb_savepoints++;
}

struct st_drive * st_job_backupdb_select_tape(struct st_job * job, enum st_job_backup_state state) {
	struct st_job_backup_private * jp = job->data;

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

					if (!slot->tape || slot->tape->pool != job->pool || st_job_backupdb_tape_in_list(jp, slot->tape) || (drive && slot->drive == drive)) {
						slot = 0;
						continue;
					}

					if (slot->drive) {
						if (!slot->drive->lock->ops->trylock(slot->drive->lock)) {
							slot->drive->ops->eod(slot->drive);
							return slot->drive;
						} else {
							st_job_backupdb_savepoint_save(job, changer, slot->drive, slot);
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
						st_job_backupdb_savepoint_save(job, changer, drive, slot);
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
				st_job_backupdb_savepoint_save(job, changer, drive, slot);
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

int st_job_backupdb_stop(struct st_job * job __attribute__((unused))) {
	return 0;
}

int st_job_backupdb_tape_in_list(struct st_job_backup_private * jp, struct st_tape * tape) {
	unsigned int i;
	for (i = 0; i < jp->nb_tapes; i++)
		if (jp->tapes[i] == tape)
			return 1;
	return 0;
}

