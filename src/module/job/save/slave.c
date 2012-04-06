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
*  Last modified: Thu, 05 Apr 2012 19:11:50 +0200                         *
\*************************************************************************/

// open
#include <fcntl.h>
// free
#include <stdlib.h>
// open, stat
#include <sys/stat.h>
// open, stat
#include <sys/types.h>
// read
#include <unistd.h>

#include <stone/database.h>
#include <stone/io/checksum.h>
#include <stone/library/archive.h>
#include <stone/log.h>
#include <stone/job.h>
#include <stone/threadpool.h>

#include "common.h"

static void st_job_save_slave_work(void * arg);


void st_job_save_add_file(struct st_job_save_private * jp, struct st_archive_file * file) {
    struct st_archive_files * f = malloc(sizeof(struct st_archive_files));
    f->file = file;
    f->next = 0;

    pthread_mutex_lock(&jp->lock);
    if (!jp->files_first)
        jp->files_first = jp->files_last = f;
    else if (jp->files_first == jp->files_last)
        jp->files_first->next = jp->files_last = f;
    else
        jp->files_last = jp->files_last->next = f;
    pthread_mutex_unlock(&jp->lock);
}

void st_job_save_start_slave(struct st_job * job) {
    st_threadpool_run(st_job_save_slave_work, job);
}

void st_job_save_slave_work(void * arg) {
    struct st_job * job = arg;
    struct st_job_save_private * jp = job->data;

    while (jp->run) {
        pthread_mutex_lock(&jp->lock);
        if (!jp->files_first)
            pthread_cond_wait(&jp->wait, &jp->lock);
        if (!jp->run) {
            pthread_mutex_unlock(&jp->lock);
            break;
        }
        struct st_archive_files * tmp = jp->files_first;
        if (jp->files_first == jp->files_last)
            jp->files_first = jp->files_last = 0;
        else
            jp->files_first = tmp->next;

        struct st_archive_file * file = tmp->file;
        free(tmp);
        pthread_mutex_unlock(&jp->lock);

        jp->db_con->ops->new_file(jp->db_con, file);
        jp->db_con->ops->file_link_to_volume(jp->db_con, file, jp->current_volume);

        if (file->type == st_archive_file_type_regular_file) {
            int fd = open(file->name, O_RDONLY);
            struct st_stream_writer * file_checksum = st_checksum_get_steam_writer((const char **) job->checksums, job->nb_checksums, 0);

            char * buffer = malloc(1 << 22);
            for (;;) {
                ssize_t nb_read = read(fd, jp->buffer, 1 << 22);
                if (nb_read == 0)
                    break;

                if (nb_read < 0) {
                    job->db_ops->add_record(job, st_log_level_error, "Unexpected error while reading from file '%s' because %m", file->name);
                    break;
                }

				file_checksum->ops->write(file_checksum, buffer, nb_read);

                job->done = (float) jp->total_size_done / jp->total_size;
                job->db_ops->update_status(job);
            }
            free(buffer);

            file_checksum->ops->close(file_checksum);

            file->digests = st_checksum_get_digest_from_writer(file_checksum);
            file->nb_checksums = job->nb_checksums;
            jp->db_con->ops->file_add_checksum(jp->db_con, file);

            close(fd);
            file_checksum->ops->free(file_checksum);
        }

        st_archive_file_free(file);
    }
}

