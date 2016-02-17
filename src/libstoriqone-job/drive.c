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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// pthread_mutex_init, pthread_mutex_lock, pthread_mutex_unlock
// pthread_mutexattr_destroy, pthread_mutexattr_init, pthread_mutexattr_settype
#include <pthread.h>
// malloc
#include <stdlib.h>
// bzero
#include <strings.h>
// close
#include <unistd.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/json.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/value.h>

#include "drive.h"
#include "io.h"
#include "job.h"

struct soj_drive {
	int fd;
	struct so_value * config;

	int fd_scan_media;

	pthread_mutex_t lock;
};

static bool soj_drive_check_header(struct so_drive * drive);
static bool soj_drive_check_support(struct so_drive * drive, struct so_media_format * format, bool for_writing);
static unsigned int soj_drive_count_archives(struct so_drive * drive);
static int soj_drive_erase_media(struct so_drive * drive, bool quick_mode);
static int soj_drive_finish_import_media(struct so_drive * drive, struct so_pool * pool);
static int soj_drive_format_media(struct so_drive * drive, ssize_t block_size, struct so_pool * pool);
static struct so_stream_reader * soj_drive_get_raw_reader(struct so_drive * drive, int file_position);
static struct so_stream_writer * soj_drive_get_raw_writer(struct so_drive * drive);
static struct so_format_reader * soj_drive_get_reader(struct so_drive * drive, int file_position, struct so_value * checksums);
static struct so_format_writer * soj_drive_get_writer(struct so_drive * drive, struct so_value * checksums);
static struct so_archive * soj_drive_parse_archive(struct so_drive * drive, int archive_position, struct so_value * checksums);
static void soj_drive_release(struct so_drive * drive);
static int soj_drive_scan_media(struct so_drive * drive);
static int soj_drive_sync(struct so_drive * drive);

static struct so_drive_ops soj_drive_ops = {
	.check_header        = soj_drive_check_header,
	.check_support       = soj_drive_check_support,
	.count_archives      = soj_drive_count_archives,
	.erase_media         = soj_drive_erase_media,
	.finish_import_media = soj_drive_finish_import_media,
	.format_media        = soj_drive_format_media,
	.get_raw_reader      = soj_drive_get_raw_reader,
	.get_raw_writer      = soj_drive_get_raw_writer,
	.get_reader          = soj_drive_get_reader,
	.get_writer          = soj_drive_get_writer,
	.parse_archive       = soj_drive_parse_archive,
	.release             = soj_drive_release,
	.scan_media          = soj_drive_scan_media,
	.sync                = soj_drive_sync,
};


static bool soj_drive_check_header(struct so_drive * drive) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{sss{ss}}",
		"command", "check header",
		"params",
			"job key", job->key
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	bool ok = false;
	struct so_value * response = so_json_parse_fd(self->fd, -1);

	pthread_mutex_unlock(&self->lock);

	if (response != NULL) {
		so_value_unpack(response, "{sb}", "returned", &ok);
		so_value_free(response);
	}

	return ok;
}

static bool soj_drive_check_support(struct so_drive * drive, struct so_media_format * format, bool for_writing) {
	struct soj_drive * self = drive->data;

	struct so_value * command = so_value_pack("{sss{sosb}}",
		"command", "check support",
		"params",
			"format", so_media_format_convert(format),
			"for writing", for_writing
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(command, self->fd, true);
	so_value_free(command);

	bool ok = false;
	struct so_value * response = so_json_parse_fd(self->fd, -1);

	pthread_mutex_unlock(&self->lock);

	if (response != NULL) {
		so_value_unpack(response, "{sb}", "returned", &ok);
		so_value_free(response);
	}

	return ok;
}

static unsigned int soj_drive_count_archives(struct so_drive * drive) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{ss}",
		"command", "count archives"
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd_scan_media, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd_scan_media, 5000);
	while (response == NULL && !job->stopped_by_user)
		response = so_json_parse_fd(self->fd_scan_media, 5000);

	pthread_mutex_unlock(&self->lock);

	if (job->stopped_by_user && response != NULL)
		so_value_free(response);
	else if (response != NULL) {
		unsigned int nb_archives = 0;
		so_value_unpack(response, "{su}", "returned", &nb_archives);
		so_value_free(response);

		return nb_archives;
	}

	return 0;
}

static int soj_drive_erase_media(struct so_drive * drive, bool quick_mode) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{sss{sssb}}",
		"command", "erase media",
		"params",
			"job key", job->key,
			"quick mode", quick_mode
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	int failed = -1;
	struct so_value * response = so_json_parse_fd(self->fd, -1);

	pthread_mutex_unlock(&self->lock);

	if (response != NULL) {
		so_value_unpack(response, "{si}", "returned", &failed);
		so_value_free(response);
	}

	return failed;
}

static int soj_drive_finish_import_media(struct so_drive * drive, struct so_pool * pool) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{sss{ssso}}",
		"command", "finish import media",
		"params",
			"job key", job->key,
			"pool", so_pool_convert(pool)
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd_scan_media, true);
	so_value_free(request);

	int failed = -1;
	struct so_value * response = so_json_parse_fd(self->fd_scan_media, -1);

	pthread_mutex_unlock(&self->lock);

	if (response != NULL) {
		so_value_unpack(response, "{si}", "returned", &failed);
		so_value_free(response);
	}

	close(self->fd_scan_media);
	self->fd_scan_media = -1;

	return failed;
}

static int soj_drive_format_media(struct so_drive * drive, ssize_t block_size, struct so_pool * pool) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{sss{ssszso}}",
		"command", "format media",
		"params",
			"job key", job->key,
			"block size", block_size,
			"pool", so_pool_convert(pool)
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	int failed = -1;
	struct so_value * response = so_json_parse_fd(self->fd, -1);

	pthread_mutex_unlock(&self->lock);

	if (response != NULL) {
		so_value_unpack(response, "{si}", "returned", &failed);
		so_value_free(response);
	}

	return failed;
}

static struct so_stream_reader * soj_drive_get_raw_reader(struct so_drive * drive, int file_position) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{sss{sssi}}",
		"command", "get raw reader",
		"params",
			"job key", job->key,
			"file position", file_position
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);

	pthread_mutex_unlock(&self->lock);

	if (response == NULL)
		return NULL;

	bool ok = false;
	so_value_unpack(response, "{sb}", "status", &ok);

	if (!ok) {
		so_value_free(response);
		return NULL;
	}

	struct so_stream_reader * reader = soj_stream_new_reader(drive, response);
	so_value_free(response);
	return reader;
}

static struct so_stream_writer * soj_drive_get_raw_writer(struct so_drive * drive) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{sss{ss}}",
		"command", "get raw writer",
		"params",
			"job key", job->key
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);

	pthread_mutex_unlock(&self->lock);

	if (response == NULL)
		return NULL;

	bool ok = false;
	so_value_unpack(response, "{sb}", "status", &ok);

	if (!ok) {
		so_value_free(response);
		return NULL;
	}

	struct so_stream_writer * writer = soj_stream_new_writer(drive, response);
	so_value_free(response);
	return writer;
}

static struct so_format_reader * soj_drive_get_reader(struct so_drive * drive, int file_position, struct so_value * checksums) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * tmp_checksums = NULL;
	if (checksums != NULL)
		tmp_checksums = so_value_share(checksums);
	else
		tmp_checksums = so_value_new_linked_list();

	struct so_value * request = so_value_pack("{sss{sssiso}}",
		"command", "get reader",
		"params",
			"job key", job->key,
			"file position", file_position,
			"checksums", tmp_checksums
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);

	pthread_mutex_unlock(&self->lock);

	if (response == NULL)
		return NULL;

	bool ok = false;
	so_value_unpack(response, "{sb}", "status", &ok);

	if (!ok) {
		so_value_free(response);
		return NULL;
	}

	struct so_format_reader * reader = soj_format_new_reader(drive, response);
	so_value_free(response);
	return reader;
}

static struct so_format_writer * soj_drive_get_writer(struct so_drive * drive, struct so_value * checksums) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * tmp_checksums = NULL;
	if (checksums != NULL)
		tmp_checksums = so_value_share(checksums);
	else
		tmp_checksums = so_value_new_linked_list();

	struct so_value * request = so_value_pack("{sss{ssso}}",
		"command", "get writer",
		"params",
			"job key", job->key,
			"checksums", tmp_checksums
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);

	pthread_mutex_unlock(&self->lock);

	if (response == NULL)
		return NULL;

	bool ok = false;
	so_value_unpack(response, "{sb}", "status", &ok);

	if (!ok) {
		so_value_free(response);
		return NULL;
	}

	struct so_format_writer * writer = soj_format_new_writer(drive, response);
	so_value_free(response);
	return writer;
}

void soj_drive_init(struct so_drive * drive, struct so_value * config) {
	struct soj_drive * self = drive->data = malloc(sizeof(struct soj_drive));
	bzero(self, sizeof(struct soj_drive));

	self->fd = so_socket(config);
	self->config = so_value_share(config);
	self->fd_scan_media = -1;

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&self->lock, &attr);

	drive->ops = &soj_drive_ops;

	pthread_mutexattr_destroy(&attr);


	struct so_job * job = soj_job_get();
	struct so_value * request = so_value_pack("{sss{ss}}",
		"command", "init peer",
		"params",
			"job key", job->key
	);
	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);
	so_value_free(response);
}

static struct so_archive * soj_drive_parse_archive(struct so_drive * drive, int archive_position, struct so_value * checksums) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{sss{sssisO}}",
		"command", "parse archive",
		"params",
			"job key", job->key,
			"archive position", archive_position,
			"checksums", checksums
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd_scan_media, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd_scan_media, 5000);
	while (response == NULL && !job->stopped_by_user)
		response = so_json_parse_fd(self->fd_scan_media, 5000);

	pthread_mutex_unlock(&self->lock);

	if (job->stopped_by_user && response != NULL)
		so_value_free(response);
	else if (response != NULL) {
		struct so_value * new_archive = NULL;
		so_value_unpack(response, "{so}", "returned", &new_archive);

		struct so_archive * archive = NULL;
		if (new_archive != NULL) {
			archive = malloc(sizeof(struct so_archive));
			bzero(archive, sizeof(struct so_archive));
			so_archive_sync(archive, new_archive);
		}

		so_value_free(response);

		return archive;
	}

	return NULL;
}

static void soj_drive_release(struct so_drive * drive) {
	struct soj_drive * self = drive->data;

	struct so_value * request = so_value_pack("{ss}", "command", "release");

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);

	pthread_mutex_unlock(&self->lock);

	so_value_free(response);

	return;
}

static int soj_drive_scan_media(struct so_drive * drive) {
	struct soj_drive * self = drive->data;
	struct so_job * job = soj_job_get();

	struct so_value * request = so_value_pack("{sss{ss}}",
		"command", "scan media",
		"params",
			"job key", job->key
	);

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);

	pthread_mutex_unlock(&self->lock);

	if (response == NULL)
		return -1;

	bool ok = false;
	so_value_unpack(response, "{sb}", "status", &ok);

	if (!ok) {
		so_value_free(response);
		return -1;
	}

	struct so_value * socket_cmd = NULL;
	so_value_unpack(response, "{s{so}}",
		"socket",
			"command", &socket_cmd
	);

	if (socket_cmd != NULL)
		self->fd_scan_media = so_socket(socket_cmd);

	so_value_free(response);

	return self->fd_scan_media < -1;
}

static int soj_drive_sync(struct so_drive * drive) {
	struct soj_drive * self = drive->data;

	struct so_value * request = so_value_pack("{ss}", "command", "sync");

	pthread_mutex_lock(&self->lock);

	so_json_encode_to_fd(request, self->fd, true);
	so_value_free(request);

	struct so_value * response = so_json_parse_fd(self->fd, -1);

	pthread_mutex_unlock(&self->lock);

	if (response == NULL)
		return 1;

	struct so_value * vdr = NULL;
	so_value_unpack(response, "{so}", "returned", &vdr);

	so_drive_sync(drive, vdr, true);

	so_value_free(response);

	return 0;
}

