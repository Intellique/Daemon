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

#define _GNU_SOURCE
#define _XOPEN_SOURCE
// gettext
#include <libintl.h>
// magic_buffer, magic_close, magic_open
#include <magic.h>
// bool
#include <stdbool.h>
// calloc, free
#include <stdlib.h>
// asprintf, sscanf
#include <stdio.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>
// S_ISREG
#include <sys/stat.h>
// localtime_r, mktime, strptime, time
#include <time.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/config.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/file.h>
#include <libstoriqone/format.h>
#include <libstoriqone/io.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-drive/drive.h>
#include <libstoriqone-drive/log.h>

#include "ltfs.h"
#include "../../media.h"
#include "../../util/scsi.h"

#include "storiqone.version"

struct sodr_tape_drive_format_ltfs_default_value {
	char * user;
	uid_t user_id;
	char * group;
	gid_t group_id;

	mode_t file_mask;
	mode_t directory_mask;
};

static unsigned int sodr_tape_drive_format_ltfs_count_extent(struct so_value * extents);
static unsigned int sodr_tape_drive_format_ltfs_count_files_inner(struct so_value * index);
static struct so_value * sodr_tape_drive_format_ltfs_find(struct so_value * index, const char * node);
static void sodr_tape_drive_format_ltfs_parse_index_inner(struct sodr_tape_drive_format_ltfs * self, struct so_value * index, const char * path, struct sodr_tape_drive_format_ltfs_default_value * default_value);
static int sodr_tape_drive_format_ltfs_remove_mam2(int scsi_fd, enum sodr_tape_drive_scsi_mam_attributes attr);


unsigned int sodr_tape_drive_format_ltfs_count_archives(struct so_media * media) {
	struct sodr_tape_drive_media * mp = media->private_data;
	return mp->data.ltfs.nb_files > 0 ? 1 : 0;
}

static unsigned int sodr_tape_drive_format_ltfs_count_extent(struct so_value * extents) {
	struct so_value * children = NULL;
	so_value_unpack(extents, "{so}", "children", &children);

	unsigned int nb_extents = 0;
	struct so_value_iterator * iter = so_value_list_get_iterator(children);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * elt = so_value_iterator_get_value(iter, false);

		const char * elt_name = NULL;
		so_value_unpack(elt, "{sS}", "name", &elt_name);

		if (elt_name == NULL)
			continue;

		if (strcmp(elt_name, "extent") == 0)
			nb_extents++;
	}
	so_value_iterator_free(iter);

	return nb_extents;
}

unsigned int sodr_tape_drive_format_ltfs_count_files(struct so_value * index) {
	// find root directory
	struct so_value * root = sodr_tape_drive_format_ltfs_find(index, "directory");

	if (root != NULL)
		return sodr_tape_drive_format_ltfs_count_files_inner(root);
	return 0;
}

static unsigned int sodr_tape_drive_format_ltfs_count_files_inner(struct so_value * index) {
	struct so_value * files = sodr_tape_drive_format_ltfs_find(index, "contents");
	if (files == NULL)
		return 0;

	struct so_value * children = NULL;
	if (so_value_unpack(files, "{so}", "children", &children) < 1)
		return 0;

	unsigned int nb_files = 0;
	struct so_value_iterator * iter = so_value_list_get_iterator(children);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * elt = so_value_iterator_get_value(iter, false);

		const char * elt_name = NULL;
		so_value_unpack(elt, "{sS}", "name", &elt_name);

		if (elt_name == NULL)
			continue;

		nb_files++;

		if (strcmp(elt_name, "directory") == 0)
			nb_files += sodr_tape_drive_format_ltfs_count_files_inner(elt);
	}
	so_value_iterator_free(iter);

	return nb_files;
}

struct so_value * sodr_tape_drive_format_ltfs_create_label(time_t time, const char * uuid, size_t block_size, const char * partition) {
	struct tm gmt;
	gmtime_r(&time, &gmt);

	char buf_time[64];
	strftime(buf_time, 64, "%FT%T.000000000Z", &gmt);

	return so_value_pack("{sss{ss}s[{ssss}{ssss}{ssss}{s[{ssss}]ss}{s[{ssss}{ssss}]ss}{sssz}{ssss}]}",
		"name", "ltfslabel",
		"attributes",
			"version", "2.2.0",
		"children",
			"name", "creator",
			"value", "Intellique StoriqOne " STORIQONE_VERSION " - Linux - tape_drive",

			"name", "formattime",
			"value", buf_time,

			"name", "volumeuuid",
			"value", uuid,

			"children",
				"name", "partition",
				"value", partition,
			"name", "location",

			"children",
				"name", "index",
				"value", "a",

				"name", "data",
				"value", "b",
			"name", "partitions",

			"name", "blocksize",
			"value", block_size,

			"name", "compression",
			"value", "false"
	);
}

struct so_value * sodr_tape_drive_format_ltfs_create_empty_fs(time_t time, const char * uuid, const char * partition, unsigned long long position, const char * previous_partiton, unsigned long long previous_position) {
	struct tm gmt;
	gmtime_r(&time, &gmt);

	char buf_time[64];
	strftime(buf_time, 64, "%FT%T.000000000Z", &gmt);

	if (previous_partiton != NULL)
		return so_value_pack("{sss{ss}s[{ssss}{ssss}{ssss}{ssss}{s[{ssss}{sssI}]ss}{s[{ssss}{sssI}]ss}{sssb}{sssi}{s[{ssss}{ssss}{ssss}{ssss}{ssss}{ssss}{ssss}{sssb}{ss}]ss}]}",
			"name", "ltfsindex",
			"attributes",
				"version", "2.2.0",
			"children",
				"name", "creator",
				"value", "Intellique StoriqOne " STORIQONE_VERSION " - Linux - tape_drive",

				"name", "volumeuuid",
				"value", uuid,

				"name", "generationnumber",
				"value", "1",

				"name", "updatetime",
				"value", buf_time,

				"children",
					"name", "partition",
					"value", partition,

					"name", "startblock",
					"value", position,
				"name", "location",

				"children",
					"name", "partition",
					"value", previous_partiton,

					"name", "startblock",
					"value", previous_position,
				"name", "previousgenerationlocation",

				"name", "allowpolicyupdate",
				"value", false,

				"name", "highestfileuid",
				"value", 1,

				"children",
					"name", "fileuid",
					"value", "1",

					"name", "name",
					"value", "root",

					"name", "creationtime",
					"value", buf_time,

					"name", "changetime",
					"value", buf_time,

					"name", "modifytime",
					"value", buf_time,

					"name", "accesstime",
					"value", buf_time,

					"name", "backuptime",
					"value", buf_time,

					"name", "readonly",
					"value", false,

					"name", "contents",
				"name", "directory"
		);
	else
		return so_value_pack("{sss{ss}s[{ssss}{ssss}{ssss}{ssss}{s[{ssss}{sssI}]ss}{sssb}{sssi}{s[{ssss}{ssss}{ssss}{ssss}{ssss}{ssss}{ssss}{sssb}{ss}]ss}]}",
			"name", "ltfsindex",
			"attributes",
				"version", "2.2.0",
			"children",
				"name", "creator",
				"value", "Intellique StoriqOne " STORIQONE_VERSION " - Linux - tape_drive",

				"name", "volumeuuid",
				"value", uuid,

				"name", "generationnumber",
				"value", "1",

				"name", "updatetime",
				"value", buf_time,

				"children",
					"name", "partition",
					"value", partition,

					"name", "startblock",
					"value", position,
				"name", "location",

				"name", "allowpolicyupdate",
				"value", false,

				"name", "highestfileuid",
				"value", 1,

				"children",
					"name", "fileuid",
					"value", "1",

					"name", "name",
					"value", "root",

					"name", "creationtime",
					"value", buf_time,

					"name", "changetime",
					"value", buf_time,

					"name", "modifytime",
					"value", buf_time,

					"name", "accesstime",
					"value", buf_time,

					"name", "backuptime",
					"value", buf_time,

					"name", "readonly",
					"value", false,

					"name", "contents",
				"name", "directory"
		);
}

static struct so_value * sodr_tape_drive_format_ltfs_find(struct so_value * index, const char * node) {
	struct so_value * children = NULL;
	so_value_unpack(index, "{so}", "children", &children);

	struct so_value_iterator * iter = so_value_list_get_iterator(children);
	struct so_value * elt = NULL;
	while (elt == NULL && so_value_iterator_has_next(iter)) {
		elt = so_value_iterator_get_value(iter, false);

		const char * elt_name = NULL;
		so_value_unpack(elt, "{sS}", "name", &elt_name);

		if (strcmp(elt_name, node) != 0)
			elt = NULL;
	}
	so_value_iterator_free(iter);

	return elt;
}

struct so_archive * sodr_tape_drive_format_ltfs_parse_archive(struct so_drive * drive, const bool * const disconnected, struct so_value * checksums, struct so_database_connection * db) {
	struct so_media * media = drive->slot->media;
	struct sodr_tape_drive_media * mp = media->private_data;

	struct so_format_reader * reader = drive->ops->get_reader(0, NULL, db);
	if (reader == NULL)
		return NULL;

	if (*disconnected) {
		reader->ops->close(reader);
		reader->ops->free(reader);
		return NULL;
	}

	struct so_archive * archive = so_archive_new();
	strncpy(archive->uuid, media->uuid, 36);
	archive->uuid[36] = '\0';

	if (mp->data.ltfs.owner_identifier[0] != '\0')
		archive->name = strdup(mp->data.ltfs.owner_identifier);
	else {
		struct tm tm_now;
		time_t now = time(NULL);
		localtime_r(&now, &tm_now);

		archive->name = malloc(41);
		strftime(archive->name, 40, "Imported LTFS %F %T%z", &tm_now);
	}

	archive->can_append = false;
	struct so_archive_volume * vol = so_archive_add_volume(archive);
	vol->media = media;
	vol->media_position = 0;

	struct so_format_file file;
	enum so_format_reader_header_status status;

	struct sodr_files {
		struct so_archive_file * file;
		ssize_t position;
		time_t archived_time;
		char * mime_type;
		struct so_value * digests;

		struct sodr_files * next;
	} * first = NULL, * last = NULL;

	bool has_checksum = so_value_list_get_length(checksums) > 0;

	magic_t magicFile = magic_open(MAGIC_MIME_TYPE);
	magic_load(magicFile, NULL);

	so_format_file_init(&file);
	while (status = reader->ops->get_header(reader, &file), !*disconnected && status == so_format_reader_header_ok) {
		struct sodr_files * new_file = malloc(sizeof(struct sodr_files));
		bzero(new_file, sizeof(struct sodr_files));

		new_file->file = so_archive_file_import(&file);
		new_file->file->selected_path = reader->ops->get_root(reader);
		new_file->position = reader->ops->position(reader);
		vol->nb_files++;

		if (has_checksum && S_ISREG(file.mode)) {
			struct so_stream_writer * writer = so_io_checksum_writer_new(NULL, checksums, true);

			char buffer[32768];
			ssize_t nb_read;
			while (nb_read = reader->ops->read(reader, buffer, 32768), nb_read > 0) {
				writer->ops->write(writer, buffer, nb_read);

				if (new_file->mime_type == NULL) {
					const char * mt = magic_buffer(magicFile, buffer, nb_read);
					if (mt != NULL)
						new_file->mime_type = strdup(mt);
					else
						new_file->mime_type = strdup("");
				}
			}

			if (new_file->mime_type == NULL) {
				const char * mt = magic_buffer(magicFile, NULL, 0);
				if (mt != NULL)
					new_file->mime_type = strdup(mt);
				else
					new_file->mime_type = strdup("");
			}

			writer->ops->close(writer);
			new_file->digests = so_io_checksum_writer_get_checksums(writer);
			writer->ops->free(writer);
		} else if (S_ISDIR(file.mode)) {
			new_file->mime_type = strdup("inode/directory");
			new_file->digests = NULL;
		} else if (S_ISLNK(file.mode)) {
			new_file->mime_type = strdup("inode/symlink");
			new_file->digests = NULL;
		} else {
			new_file->mime_type = strdup("application/octet-stream");
			new_file->digests = NULL;
		}

		vol->size += file.size;

		new_file->archived_time = time(NULL);
		new_file->next = NULL;

		if (first == NULL)
			first = last = new_file;
		else
			last = last->next = new_file;

		so_format_file_free(&file);
		so_format_file_init(&file);
	}

	reader->ops->close(reader);
	reader->ops->free(reader);

	vol->end_time = time(NULL);
	archive->size = vol->size;

	so_format_file_free(&file);

	magic_close(magicFile);

	vol->files = calloc(vol->nb_files, sizeof(struct so_archive_files));

	unsigned int i;
	struct sodr_files * a_file = first;
	for (i = 0; i < vol->nb_files; i++) {
		struct so_archive_files * ptr_file = vol->files + i;
		ptr_file->position = a_file->position;
		ptr_file->archived_time = a_file->archived_time;

		struct so_archive_file * new_file = ptr_file->file = a_file->file;

		new_file->mime_type = a_file->mime_type;
		new_file->digests = a_file->digests;

		struct sodr_files * tmp_file = a_file;
		a_file = tmp_file->next;

		free(tmp_file);
	}

	return archive;
}

void sodr_tape_drive_format_ltfs_parse_index(struct sodr_tape_drive_media * mp, struct so_value * index) {
	// find root directory
	struct so_value * root = sodr_tape_drive_format_ltfs_find(index, "directory");
	if (root == NULL)
		return;

	struct sodr_tape_drive_format_ltfs * self = &mp->data.ltfs;
	self->first_file = self->last_file = NULL;
	self->nb_files = 0;

	struct so_value * contents = sodr_tape_drive_format_ltfs_find(root, "contents");
	if (contents == NULL)
		return;

	struct so_value * files = NULL;
	so_value_unpack(contents, "{so}", "children", &files);

	struct so_value * config = so_config_get();
	unsigned int uid = 0, gid = 0, file_mask = 0644, directory_mask = 0755;
	const char * base_directory;

	so_value_unpack(config, "{s{s{sususususS}}}",
		"format",
			"ltfs",
				"user id", &uid,
				"group id", &gid,
				"file mask", &file_mask,
				"directory mask", &directory_mask,
				"base directory", &base_directory
	);

	struct sodr_tape_drive_format_ltfs_default_value default_value = {
		.user = so_file_uid2name(uid),
		.user_id = uid,
		.group = so_file_gid2name(gid),
		.group_id = gid,

		.file_mask = file_mask,
		.directory_mask = directory_mask,
	};

	struct so_value_iterator * iter = so_value_list_get_iterator(files);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * file = so_value_iterator_get_value(iter, false);
		sodr_tape_drive_format_ltfs_parse_index_inner(self, file, base_directory, &default_value);
	}
	so_value_iterator_free(iter);

	free(default_value.user);
	free(default_value.group);
}

static void sodr_tape_drive_format_ltfs_parse_index_inner(struct sodr_tape_drive_format_ltfs * self, struct so_value * index, const char * path, struct sodr_tape_drive_format_ltfs_default_value * default_value) {
	struct so_value * children = NULL;
	if (so_value_unpack(index, "{so}", "children", &children) < 1)
		return;

	struct sodr_tape_drive_format_ltfs_file * file = malloc(sizeof(struct sodr_tape_drive_format_ltfs_file));
	bzero(file, sizeof(struct sodr_tape_drive_format_ltfs_file));

	if (self->first_file == NULL)
		self->first_file = self->last_file = file;
	else
		self->last_file = self->last_file->next = file;
	self->nb_files++;

	file->file.mode = S_IFREG | default_value->file_mask;

	struct so_value_iterator * iter = so_value_list_get_iterator(children);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * elt = so_value_iterator_get_value(iter, false);

		const char * elt_name = NULL;
		so_value_unpack(elt, "{sS}", "name", &elt_name);

		if (elt_name == NULL)
			continue;

		if (strcmp(elt_name, "name") == 0) {
			const char * file_name = NULL;
			so_value_unpack(elt, "{sS}", "value", &file_name);

			int size = asprintf(&file->file.filename, "%s/%s", path, file_name);
			if (size < 0)
				continue;
		} else if (strcmp(elt_name, "creationtime") == 0) {
			const char * ctime = NULL;
			so_value_unpack(elt, "{sS}", "value", &ctime);

			if (ctime != NULL)
				file->file.ctime = sodr_tape_drive_format_ltfs_parse_time(ctime);
		} else if (strcmp(elt_name, "modifytime") == 0) {
			const char * mtime = NULL;
			so_value_unpack(elt, "{sS}", "value", &mtime);

			if (mtime != NULL)
				file->file.mtime = sodr_tape_drive_format_ltfs_parse_time(mtime);
		} else if (strcmp(elt_name, "length") == 0) {
			const char * length = NULL;
			so_value_unpack(elt, "{sS}", "value", &length);

			if (length != NULL)
				sscanf(length, "%zd", &file->file.size);
		} else if (strcmp(elt_name, "extentinfo") == 0) {
			file->nb_extents = sodr_tape_drive_format_ltfs_count_extent(elt);
			file->extents = calloc(file->nb_extents, sizeof(struct sodr_tape_drive_format_ltfs_extent));

			struct so_value * children = NULL;
			so_value_unpack(elt, "{so}", "children", &children);

			struct so_value_iterator * iter = so_value_list_get_iterator(children);
			unsigned int i;
			for (i = 0; so_value_iterator_has_next(iter); i++) {
				struct so_value * vextent = so_value_iterator_get_value(iter, false);

				const char * child_name = NULL;
				so_value_unpack(vextent, "{sS}", "name", &child_name);

				if (child_name == NULL)
					continue;

				if (strcmp(child_name, "extent") != 0)
					continue;

				struct sodr_tape_drive_format_ltfs_extent * ext = file->extents + i;

				struct so_value * vextent_infos = NULL;
				so_value_unpack(vextent, "{so}", "children", &vextent_infos);

				struct so_value_iterator * iter_extent_info = so_value_list_get_iterator(vextent_infos);
				while (so_value_iterator_has_next(iter_extent_info)) {
					struct so_value * info = so_value_iterator_get_value(iter_extent_info, false);

					const char * info_name = NULL, * info_value = NULL;
					so_value_unpack(info, "{sSsS}", "name", &info_name, "value", &info_value);

					if (info_name == NULL)
						continue;

					if (info_value == NULL)
						continue;

					if (strcmp(info_name, "fileoffset") == 0) {
						sscanf(info_value, "%zd", &file->file.position);
						ext->file_offset = file->file.position;
					} else if (strcmp(info_name, "partition") == 0)
						ext->partition = info_value[0] - 'a';
					else if (strcmp(info_name, "startblock") == 0)
						sscanf(info_value, "%zd", &ext->start_block);
					else if (strcmp(info_name, "byteoffset") == 0)
						sscanf(info_value, "%zd", &ext->byte_offset);
					else if (strcmp(info_name, "bytecount") == 0)
						sscanf(info_value, "%zd", &ext->byte_count);
				}
				so_value_iterator_free(iter_extent_info);
			}
			so_value_iterator_free(iter);
		} else if (strcmp(elt_name, "symlink") == 0) {
			so_value_unpack(elt, "{ss}", "value", &file->file.link);
			file->file.mode = S_IFLNK | default_value->file_mask;
		}
	}
	so_value_iterator_free(iter);

	file->file.user = strdup(default_value->user);
	file->file.uid = default_value->user_id;
	file->file.group = strdup(default_value->group);
	file->file.gid = default_value->group_id;

	const char * elt_name = NULL;
	so_value_unpack(index, "{sS}", "name", &elt_name);

	if (strcmp(elt_name, "directory") == 0) {
		struct so_value * contents = sodr_tape_drive_format_ltfs_find(index, "contents");

		struct so_value * files = NULL;
		so_value_unpack(contents, "{so}", "children", &files);

		struct so_value_iterator * iter = so_value_list_get_iterator(files);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * child = so_value_iterator_get_value(iter, false);
			sodr_tape_drive_format_ltfs_parse_index_inner(self, child, file->file.filename, default_value);
		}
		so_value_iterator_free(iter);

		file->file.mode = S_IFDIR | default_value->directory_mask;
	}
}

time_t sodr_tape_drive_format_ltfs_parse_time(const char * date) {
	struct tm tm;
	bzero(&tm, sizeof(tm));

	strptime(date, "%FT%T", &tm);

	return mktime(&tm) - timezone;
}

int sodr_tape_drive_format_ltfs_update_mam(int scsi_fd, struct so_drive * drive, struct so_media * media, struct so_database_connection * db) {
	const char * media_name = so_media_get_name(media);

	static struct sodr_tape_drive_scsi_mam_attribute application_vendor = {
		.identifier = sodr_tape_drive_scsi_mam_application_vendor,
		.format     = sodr_tape_drive_scsi_mam_attribute_format_ascii,
		.read_only  = false,
		.length     = 8,
		.value.text = "INTELLIQ"
	};

	sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, try to update attribute 'application vendor' with 'INTELLIQ' on media '%s'"),
		drive->vendor, drive->model, drive->index, media_name);

	int failed = sodr_tape_drive_scsi_write_attribute(scsi_fd, &application_vendor);
	if (failed != 0)
		sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, failed to update attribute 'application vendor' with 'INTELLIQ' on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);
	else
		sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, attribute 'application vendor' updated with 'INTELLIQ' on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);

	if (failed != 0)
		return failed;


	static struct sodr_tape_drive_scsi_mam_attribute application_name = {
		.identifier = sodr_tape_drive_scsi_mam_application_name,
		.format     = sodr_tape_drive_scsi_mam_attribute_format_ascii,
		.read_only  = false,
		.length     = 32,
		.value.text = "STORIQ ONE                      "
	};

	sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, try to update attribute 'application name' with 'STORIQ ONE' on media '%s'"),
		drive->vendor, drive->model, drive->index, media_name);

	failed = sodr_tape_drive_scsi_write_attribute(scsi_fd, &application_name);
	if (failed != 0)
		sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, failed to update attribute 'application name' with 'STORIQ ONE' on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);
	else
		sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, attribute 'application name' updated with 'STORIQ ONE' on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);

	if (failed != 0)
		return failed;


	struct sodr_tape_drive_scsi_mam_attribute application_version = {
		.identifier = sodr_tape_drive_scsi_mam_application_version,
		.format     = sodr_tape_drive_scsi_mam_attribute_format_ascii,
		.read_only  = false,
		.length     = 8,
		.value.text = "        "
	};

	static const char * version = STORIQONE_VERSION + 1;
	int version_length = strcspn(version, "-~");

	if (version_length > 8)
		version_length = 8;

	strncpy(application_version.value.text, version, version_length);

	sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, try to update attribute 'application version' with '%*s' on media '%s'"),
		drive->vendor, drive->model, drive->index, version_length, version, media_name);

	failed = sodr_tape_drive_scsi_write_attribute(scsi_fd, &application_version);
	if (failed != 0)
		sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, failed to update attribute 'application version' with '%*s' on media '%s'"),
			drive->vendor, drive->model, drive->index, version_length, version, media_name);
	else
		sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, attribute 'application version' updated with '%*s' on media '%s'"),
			drive->vendor, drive->model, drive->index, version_length, version, media_name);

	if (failed != 0)
		return failed;


	static struct sodr_tape_drive_scsi_mam_attribute user_medium_text_label = {
		.identifier = sodr_tape_drive_scsi_mam_user_medium_text_label,
		.format     = sodr_tape_drive_scsi_mam_attribute_format_ascii,
		.read_only  = false,
		.length     = 160,
		.value.text = "                                                                                                                                                                "
	};

	int media_name_length = strlen(media_name);
	if (media_name_length > 160)
		media_name_length = 160;

	strncpy(user_medium_text_label.value.text, media_name, media_name_length);

	sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, try to update attribute 'user medium text label' with '%*s' on media '%s'"),
		drive->vendor, drive->model, drive->index, media_name_length, media_name, media_name);

	failed = sodr_tape_drive_scsi_write_attribute(scsi_fd, &user_medium_text_label);
	if (failed != 0)
		sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, failed to update attribute 'user medium text label' with '%*s' on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name_length, media_name, media_name);
	else
		sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, attribute 'user medium text label' updated with '%*s' on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name_length, media_name, media_name);

	if (failed != 0)
		return failed;


	if (media->label != NULL) {
		static struct sodr_tape_drive_scsi_mam_attribute bar_code = {
			.identifier = sodr_tape_drive_scsi_mam_barcode,
			.format     = sodr_tape_drive_scsi_mam_attribute_format_ascii,
			.read_only  = false,
			.length     = 32,
			.value.text = "                                "
		};

		int media_label_length = strlen(media->label);
		if (media_label_length > 32)
			media_label_length = 32;

		strncpy(bar_code.value.text, media->label, media_label_length);

		sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, try to update 'bar code' with '%*s' on media '%s'"),
			drive->vendor, drive->model, drive->index, media_label_length, media->label, media_name);

		failed = sodr_tape_drive_scsi_write_attribute(scsi_fd, &bar_code);
		if (failed != 0)
			sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, failed to update 'bar code' with '%*s' on media '%s'"),
				drive->vendor, drive->model, drive->index, media_label_length, media->label, media_name);
		else
			sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, update 'bar code' with '%*s' on media '%s'"),
				drive->vendor, drive->model, drive->index, media_label_length, media->label, media_name);

		if (failed != 0)
			return failed;
	} else
		sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, skip update of attribute 'bar code' on media '%s' because there is no bar code available"),
			drive->vendor, drive->model, drive->index, media_name);


	static struct sodr_tape_drive_scsi_mam_attribute application_format_version = {
		.identifier = sodr_tape_drive_scsi_mam_application_format_version,
		.format     = sodr_tape_drive_scsi_mam_attribute_format_ascii,
		.read_only  = false,
		.length     = 16,
		.value.text = "2.2.0           "
	};

	sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, try to update attribute 'application format version' with '2.2.0' on media '%s'"),
		drive->vendor, drive->model, drive->index, media_name);

	failed = sodr_tape_drive_scsi_write_attribute(scsi_fd, &application_format_version);
	if (failed != 0)
		sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, failed to update attribute 'application format version' with '2.2.0' on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);
	else
		sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, attribute 'application format version' updated with '2.2.0' on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);

	return failed;
}

int sodr_tape_drive_format_ltfs_remove_mam(int scsi_fd, struct so_drive * drive, struct so_media * media, struct so_database_connection * db) {
	const char * media_name = so_media_get_name(media);

	sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, try to remove attribute 'application vendor' on media '%s'"),
		drive->vendor, drive->model, drive->index, media_name);

	int failed = sodr_tape_drive_format_ltfs_remove_mam2(scsi_fd, sodr_tape_drive_scsi_mam_application_vendor);
	if (failed != 0)
		sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, failed to remove attribute 'application vendor' on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);
	else
		sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, attribute 'application vendor' removed on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);

	if (failed != 0)
		return failed;



	sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, try to remove attribute 'application name' on media '%s'"),
		drive->vendor, drive->model, drive->index, media_name);

	failed = sodr_tape_drive_format_ltfs_remove_mam2(scsi_fd, sodr_tape_drive_scsi_mam_application_name);
	if (failed != 0)
		sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, failed to remove attribute 'application name' on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);
	else
		sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, attribute 'application name' removed on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);

	if (failed != 0)
		return failed;


	sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, try to remove attribute 'application version' on media '%s'"),
		drive->vendor, drive->model, drive->index, media_name);

	failed = sodr_tape_drive_format_ltfs_remove_mam2(scsi_fd, sodr_tape_drive_scsi_mam_application_version);
	if (failed != 0)
		sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, failed to remove attribute 'application version' on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);
	else
		sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, attribute 'application version' removed on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);

	if (failed != 0)
		return failed;


	sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, try to remove attribute 'user medium text label' on media '%s'"),
		drive->vendor, drive->model, drive->index, media_name);

	failed = sodr_tape_drive_format_ltfs_remove_mam2(scsi_fd, sodr_tape_drive_scsi_mam_user_medium_text_label);
	if (failed != 0)
		sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, failed to remove attribute 'user medium text label' on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);
	else
		sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, attribute 'user medium text label' removed on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);

	if (failed != 0)
		return failed;


	sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, try to remove attribute 'application name' on media '%s'"),
		drive->vendor, drive->model, drive->index, media_name);

	failed = sodr_tape_drive_format_ltfs_remove_mam2(scsi_fd, sodr_tape_drive_scsi_mam_application_name);
	if (failed != 0)
		sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, failed to remove attribute 'application name' on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);
	else
		sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Update medium auxiliary memory, attribute 'application name' removed on media '%s'"),
			drive->vendor, drive->model, drive->index, media_name);

	return failed;
}

static int sodr_tape_drive_format_ltfs_remove_mam2(int scsi_fd, enum sodr_tape_drive_scsi_mam_attributes attr) {
	struct sodr_tape_drive_scsi_mam_attribute attribute = {
		.identifier = attr,
		.format     = sodr_tape_drive_scsi_mam_attribute_format_ascii,
		.read_only  = false,
		.length     = 0,
	};
	return sodr_tape_drive_scsi_write_attribute(scsi_fd, &attribute);
}

