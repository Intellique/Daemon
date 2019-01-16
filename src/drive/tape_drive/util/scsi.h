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
*  Copyright (C) 2013-2019, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __SO_TAPEDRIVE_UTIL_SCSI_H__
#define __SO_TAPEDRIVE_UTIL_SCSI_H__

// bool
#include <stdbool.h>
// off_t
#include <sys/types.h>

struct so_drive;
struct so_media;
struct so_media_format;
struct sodr_tape_drive_ltfs_volume_coherency;

enum sodr_tape_drive_scsi_mam_attributes {
	sodr_tape_drive_scsi_mam_remaining_capacity      = 0x0000,
	sodr_tape_drive_scsi_mam_maximum_capacity        = 0x0001,
	sodr_tape_drive_scsi_mam_load_count              = 0x0003,
	sodr_tape_drive_scsi_mam_mam_space_remaining     = 0x0004,
	sodr_tape_drive_scsi_mam_assigning_organisation  = 0x0005,
	sodr_tape_drive_scsi_mam_formatted_density_code  = 0x0006,
	sodr_tape_drive_scsi_mam_initialization_count    = 0x0007,
	sodr_tape_drive_scsi_mam_volume_identifier       = 0x0008, // Not supported ?
	sodr_tape_drive_scsi_mam_volume_change_reference = 0x0009,

	// from 0x000A to 0x0209 : Reserved

	sodr_tape_drive_scsi_mam_device_at_last_load   = 0x020A,
	sodr_tape_drive_scsi_mam_device_at_last_load_2 = 0x020B,
	sodr_tape_drive_scsi_mam_device_at_last_load_3 = 0x020C,
	sodr_tape_drive_scsi_mam_device_at_last_load_4 = 0x020D,

	// from 0x020E to 0x021F : Reserved

	sodr_tape_drive_scsi_mam_total_written_in_medium_life  = 0x0220,
	sodr_tape_drive_scsi_mam_total_read_in_medium_life     = 0x0221,
	sodr_tape_drive_scsi_mam_total_written_in_current_load = 0x0222,
	sodr_tape_drive_scsi_mam_total_read_current_load       = 0x0223,

	// from 0x0226 to 0x033F : Reserved

	sodr_tape_drive_scsi_mam_medium_manufacturer        = 0x0400,
	sodr_tape_drive_scsi_mam_medium_serial_number       = 0x0401,
	sodr_tape_drive_scsi_mam_medium_length              = 0x0402,
	sodr_tape_drive_scsi_mam_medium_width               = 0x0403,
	sodr_tape_drive_scsi_mam_medium_assign_organization = 0x0404,
	sodr_tape_drive_scsi_mam_medium_density_code        = 0x0405,
	sodr_tape_drive_scsi_mam_medium_manufacturer_date   = 0x0406,
	sodr_tape_drive_scsi_mam_mam_capacity               = 0x0407,
	sodr_tape_drive_scsi_mam_medium_type                = 0x0408,
	sodr_tape_drive_scsi_mam_medium_type_information    = 0x0409,

	sodr_tape_drive_scsi_mam_application_vendor           = 0x0800,
	sodr_tape_drive_scsi_mam_application_name             = 0x0801,
	sodr_tape_drive_scsi_mam_application_version          = 0x0802,
	sodr_tape_drive_scsi_mam_user_medium_text_label       = 0x0803,
	sodr_tape_drive_scsi_mam_date_and_time_last_written   = 0x0804,
	sodr_tape_drive_scsi_mam_text_localization_identifier = 0x0805,
	sodr_tape_drive_scsi_mam_barcode                      = 0x0806,
	sodr_tape_drive_scsi_mam_owning_host_textual_name     = 0x0807,
	sodr_tape_drive_scsi_mam_media_pool                   = 0x0808,
	sodr_tape_drive_scsi_mam_application_format_version   = 0x080B,
	sodr_tape_drive_scsi_mam_volume_coherency_infomation  = 0x080C,

	sodr_tape_drive_scsi_mam_unique_cardtrige_identity = 0x1000,
};

struct sodr_tape_drive_scsi_mam_attribute {
	enum sodr_tape_drive_scsi_mam_attributes identifier:16;
	enum sodr_tape_drive_scsi_mam_attribute_format {
		sodr_tape_drive_scsi_mam_attribute_format_binary   = 0x0,
		sodr_tape_drive_scsi_mam_attribute_format_ascii    = 0x1,
		sodr_tape_drive_scsi_mam_attribute_format_text     = 0x2,
		sodr_tape_drive_scsi_mam_attribute_format_reserved = 0x3,
	} format:2;
	unsigned char reserved:5;
	bool read_only:1;
	unsigned short length;
	union {
		unsigned char int8;
		unsigned short be16;
		unsigned int be32;
		unsigned long long be64;
		char text[160];
	} value;
} __attribute__((packed));

struct sodr_tape_drive_scsi_position {
	unsigned int partition;
	off_t block_position;
	bool end_of_partition;
};

struct sodr_tape_drive_scsi_volume_coherency_information {
	unsigned char volume_change_reference_value_length;
	unsigned long long volume_change_reference_value;
	unsigned long long volume_coherency_count;
	unsigned long long volume_coherency_set_identifier;
	unsigned short application_client_specific_information_length;
} __attribute__((packed));

/**
 * \brief application client specific information
 */
struct sodr_tape_drive_scsi_ltfs_acsi {
	char header[5]; // must be "LTFS\0"
	char volume_uuid[37]; // uuid of media
	unsigned char version; // must be 0x01
} __attribute__((packed));

bool sodr_tape_drive_scsi_check_drive(struct so_drive * drive, const char * path);
bool sodr_tape_drive_scsi_check_support(struct so_media_format * format, bool for_writing, const char * path);
int sodr_tape_drive_scsi_erase_media(int fd, bool quick_mode);
/**
 * \brief SCSI command to format medium
 * \pre The current logical position of tape should be bottom of partition in partition 0.
 * \param path : path of generic scsi device
 * \param parition_size : size of second partition. Zero means format into one partition.
 * \param two_times : if partition_size is greater than zero, the drive will format into one partition then
 * into two partition
 * \return 0 if succeed
 */
int sodr_tape_drive_scsi_format_medium(const char * path, size_t partition_size, bool two_times);
/**
 * \brief Set position on tape
 * \remark Require LTO-4 drive at least
 */
bool sodr_tape_drive_scsi_has_attribute(int fd, enum sodr_tape_drive_scsi_mam_attributes, unsigned char part);
int sodr_tape_drive_scsi_locate(int fd, struct sodr_tape_drive_scsi_position * position, struct so_media_format * format);
int sodr_tape_drive_scsi_read_density(struct so_drive * drive, const char * path);
int sodr_tape_drive_scsi_read_position(int fd, struct sodr_tape_drive_scsi_position * position);
int sodr_tape_drive_scsi_read_medium_serial_number(int fd, char * medium_serial_number, size_t length);
int sodr_tape_drive_scsi_read_mam(int fd, struct so_media * media);
int sodr_tape_drive_scsi_read_volume_change_reference(int fd, unsigned int * volume_change_reference);
int sodr_tape_drive_scsi_read_volume_coherency(int fd, struct sodr_tape_drive_ltfs_volume_coherency * volume_coherency, unsigned char part);
int sodr_tape_drive_scsi_rewind(int fd);
int sodr_tape_drive_scsi_setup(const char * path);
int sodr_tape_drive_scsi_size_available(int fd, struct so_media * media);
int sodr_tape_drive_scsi_test_unit_ready(int fd, bool wait);
int sodr_tape_drive_scsi_write_attribute(int fd, const struct sodr_tape_drive_scsi_mam_attribute * attribute, unsigned char part);

#endif
