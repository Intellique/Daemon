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

// open, size_t
#include <sys/types.h>

// be*toh, htobe*
#include <endian.h>
// open
#include <fcntl.h>
// dgettext
#include <libintl.h>
// sg_io_hdr_t
#include <scsi/scsi.h>
// sg_io_hdr_t
#include <scsi/sg.h>
// bool
#include <stdbool.h>
// free
#include <stdlib.h>
// memcpy, memset, strdup
#include <string.h>
// ioctl
#include <sys/ioctl.h>
// close
#include <unistd.h>

#include <libstoriqone/drive.h>
#include <libstoriqone/log.h>
#include <libstoriqone/string.h>

#include "scsi.h"
#include "../media.h"

struct scsi_command {
	const char * name;
	unsigned char op_code;
	unsigned char cdb_length;
	unsigned int timeout;
	bool available;
};

struct scsi_density {
	unsigned short available_density_descriptor_length;
	unsigned char reserved[2];
	struct {
		unsigned char primary_density_code;
		unsigned char secondary_density_code;
		unsigned char reserved0:5;
		bool deflt:1;
		bool duplicate:1;
		bool write_ok:1;
		unsigned char reserved1[2];
		unsigned int bits_per_mm:24;
		unsigned short media_width;
		unsigned short tracks;
		unsigned int capacity;
		char assiging_organization[8];
		char density_name[8];
		char description[20];
	} values[8];
} __attribute__((packed));

struct scsi_inquiry {
	unsigned char operation_code;
	bool enable_vital_product_data:1;
	unsigned char reserved0:4;
	unsigned char obsolete:3;
	unsigned char page_code;
	unsigned char reserved1;
	unsigned char allocation_length;
	unsigned char reserved2;
} __attribute__((packed));

struct scsi_request_sense {
	unsigned char error_code:7;						/* Byte 0 Bits 0-6 */
	bool valid:1;									/* Byte 0 Bit 7 */
	unsigned char segment_number;					/* Byte 1 */
	unsigned char sense_key:4;						/* Byte 2 Bits 0-3 */
	bool :1;										/* Byte 2 Bit 4 */
	bool ili:1;										/* Byte 2 Bit 5 */
	bool eom:1;										/* Byte 2 Bit 6 */
	bool filemark:1;								/* Byte 2 Bit 7 */
	unsigned char information[4];					/* Bytes 3-6 */
	unsigned char additional_sense_length;			/* Byte 7 */
	unsigned char command_specific_information[4];	/* Bytes 8-11 */
	unsigned char additional_sense_code;			/* Byte 12 */
	unsigned char additional_sense_code_qualifier;	/* Byte 13 */
	unsigned char :8;								/* Byte 14 */
	unsigned char bit_pointer:3;					/* Byte 15 */
	unsigned char bpv:1;
	unsigned char :2;
	bool command_data:1;
	bool sksv:1;
	unsigned char field_data[2];					/* Byte 16,17 */
};

static int sodr_tape_drive_scsi_locate10(int fd, struct sodr_tape_drive_scsi_position * position, struct so_media_format * format);
/**
 * \brief Set position on tape
 * \remark Require LTO-4 drive at least
 */
static int sodr_tape_drive_scsi_locate16(int fd, struct sodr_tape_drive_scsi_position * position, struct so_media_format * format);
static int sodr_tape_drive_scsi_setup2(int fd, struct scsi_command * scsi_command);
static int sodr_tape_drive_scsi_test_unit_ready2(int fd);

static struct scsi_command scsi_command_erase = {
	.name = "ERASE",
	.op_code = 0x19,
	.cdb_length = 6,
	.timeout = 10800,
	.available = true
};

static struct scsi_command scsi_command_inquiry = {
	.name = "INQUIRY",
	.op_code = 0x12,
	.cdb_length = 6,
	.timeout = 60,
	.available = true
};

static struct scsi_command scsi_command_locate10 = {
	.name = "LOCATE (10)",
	.op_code = 0x2B,
	.cdb_length = 10,
	.timeout = 1260,
	.available = true
};

static struct scsi_command scsi_command_locate16 = {
	.name = "LOCATE (16)",
	.op_code = 0x92,
	.cdb_length = 16,
	.timeout = 1260,
	.available = true
};

static struct scsi_command scsi_command_log_sense = {
	.name = "LOG SENSE",
	.op_code = 0x4D,
	.cdb_length = 10,
	.timeout = 60,
	.available = true
};

static struct scsi_command scsi_command_read_attribute = {
	.name = "READ ATTRIBUTE",
	.op_code = 0x8C,
	.cdb_length = 16,
	.timeout = 60,
	.available = true
};

static struct scsi_command scsi_command_read_position = {
	.name = "READ POSITION",
	.op_code = 0x34,
	.cdb_length = 16,
	.timeout = 60,
	.available = true
};

static struct scsi_command scsi_command_report_density_support = {
	.name = "REPORT DENSITY SUPPORT",
	.op_code = 0x44,
	.cdb_length = 10,
	.timeout = 60,
	.available = true
};

static struct scsi_command scsi_command_rewind = {
	.name = "REWIND",
	.op_code = 0x01,
	.cdb_length = 6,
	.timeout = 540,
	.available = true
};

static struct scsi_command scsi_command_write_attribute = {
	.name = "WRITE ATTRIBUTE",
	.op_code = 0x8d,
	.cdb_length = 16,
	.timeout = 60,
	.available = true
};


bool sodr_tape_drive_scsi_check_drive(struct so_drive * drive, const char * path) {
	if (!scsi_command_inquiry.available)
		return false;

	int fd = open(path, O_RDWR);
	if (fd < 0)
		return false;

	struct {
		unsigned char peripheral_device_type:5;
		unsigned char peripheral_device_qualifier:3;
		unsigned char reserved0:7;
		unsigned char removable_medium_bit:1;
		unsigned char version:3;
		unsigned char ecma_version:3;
		unsigned char iso_version:2;
		unsigned char response_data_format:4;
		unsigned char hi_sup:1;
		unsigned char norm_aca:1;
		unsigned char obsolete0:1;
		unsigned char asynchronous_event_reporting_capability:1;
		unsigned char additional_length;
		unsigned char reserved1:7;
		unsigned char scc_supported:1;
		unsigned char addr16:1;
		unsigned char addr32:1;
		unsigned char obsolete1:1;
		unsigned char medium_changer:1;
		unsigned char multi_port:1;
		unsigned char vs:1;
		unsigned char enclosure_service:1;
		unsigned char basic_queuing:1;
		unsigned char reserved2:1;
		unsigned char command_queuing:1;
		unsigned char trans_dis:1;
		unsigned char linked_command:1;
		unsigned char synchonous_transfer:1;
		unsigned char wide_bus_16:1;
		unsigned char obsolete2:1;
		unsigned char relative_addressing:1;
		char vendor_identification[8];
		char product_identification[16];
		char product_revision_level[4];
		unsigned char aut_dis:1;
		unsigned char performance_limit;
		unsigned char reserved3[3];
		unsigned char oem_specific;
	} __attribute__((packed)) result_inquiry;

	struct scsi_inquiry command_inquiry = {
		.operation_code = 0x12, // Inquiry (6)
		.enable_vital_product_data = false,
		.page_code = 0,
		.allocation_length = sizeof(result_inquiry),
	};

	struct scsi_request_sense sense;
	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(&result_inquiry, 0, sizeof(result_inquiry));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command_inquiry);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(result_inquiry);
	header.cmdp = (unsigned char *) &command_inquiry;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = (unsigned char *) &result_inquiry;
	header.timeout = 1000 * scsi_command_inquiry.timeout;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);

	if (status) {
		close(fd);
		return false;
	}

	char * vendor = strndup(result_inquiry.vendor_identification, 7);
	so_string_rtrim(vendor, ' ');
	char * model = strndup(result_inquiry.product_identification, 15);
	so_string_rtrim(model, ' ');

	bool ok = !strcmp(drive->vendor, vendor);
	if (ok)
		ok = !strcmp(drive->model, model);

	free(vendor);
	free(model);

	if (!ok) {
		close(fd);
		return false;
	}

	struct {
		unsigned char peripheral_device_type:5;
		unsigned char peripheral_device_qualifier:3;
		unsigned char page_code;
		unsigned char reserved;
		unsigned char page_length;
		char unit_serial_number[12];
	} __attribute__((packed)) result_serial_number;

	struct scsi_inquiry command_serial_number = {
		.operation_code = 0x12,
		.enable_vital_product_data = 1,
		.page_code = 0x80,
		.allocation_length = sizeof(result_serial_number),
	};

	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(&result_serial_number, 0, sizeof(result_serial_number));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command_serial_number);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(result_serial_number);
	header.cmdp = (unsigned char *) &command_serial_number;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = (unsigned char *) &result_serial_number;
	header.timeout = 1000 * scsi_command_inquiry.timeout;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	status = ioctl(fd, SG_IO, &header);

	close(fd);

	if (status != 0)
		return false;

	result_serial_number.unit_serial_number[11] = '\0';
	so_string_rtrim(result_serial_number.unit_serial_number, ' ');
	char * serial_number = strdup(result_serial_number.unit_serial_number);

	ok = !strcmp(drive->serial_number, serial_number);

	free(serial_number);

	if (ok) {
		drive->revision = malloc(5);
		strncpy(drive->revision, result_inquiry.product_revision_level, 4);
		drive->revision[4] = '\0';
		so_string_rtrim(drive->revision, ' ');
	}

	return ok;
}

bool sodr_tape_drive_scsi_check_support(struct so_media_format * format, bool for_writing, const char * path) {
	if (!scsi_command_report_density_support.available)
		return false;

	int fd = open(path, O_RDWR);
	if (fd < 0)
		return false;

	struct scsi_density result;

	struct {
		unsigned char operation_code;
		bool media:1;
		unsigned char reserved0:4;
		unsigned char obsolete:3;
		unsigned char reserved1[5];
		unsigned short allocation_length;
		unsigned char control;
	} __attribute__((packed)) command = {
		.operation_code = 0x44, // Report Density Support (10)
		.media = false,
		.allocation_length = htobe16(sizeof(result)),
		.control = 0,
	};

	struct scsi_request_sense sense;
	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(&result, 0, sizeof(result));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(result);
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = (unsigned char *) &result;
	header.timeout = 1000 * scsi_command_report_density_support.timeout;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int failed = ioctl(fd, SG_IO, &header);
	close(fd);

	if (failed != 0)
		return false;

	unsigned short int nb_densities = be16toh(result.available_density_descriptor_length);
	unsigned int i;
	for (i = 0; i < nb_densities && result.values[i].primary_density_code > 0 && i < 8; i++)
		if (format->density_code == result.values[i].primary_density_code)
			return !for_writing || result.values[i].write_ok;

	return false;
}

int sodr_tape_drive_scsi_erase_media(int fd, bool quick_mode) {
	if (!scsi_command_erase.available)
		return -1;

	struct {
		unsigned char op_code;
		bool long_mode:1;
		bool immed:1;
		unsigned char reserved0:3;
		unsigned char obsolete:3;
		unsigned char reserved1[3];
		unsigned char control;
	} __attribute__((packed)) command = {
		.op_code = 0x19, // ERASE (6)
		.long_mode = !quick_mode,
		.immed = false,
	};

	struct scsi_request_sense sense;
	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = 0;
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = NULL;
	header.timeout = 1000 * scsi_command_erase.timeout;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	return ioctl(fd, SG_IO, &header);
}

int sodr_tape_drive_scsi_locate(int fd, struct sodr_tape_drive_scsi_position * position, struct so_media_format * format) {
	if (!format->support_partition && position->partition > 0)
		return -1;

	if (scsi_command_locate16.available)
		return sodr_tape_drive_scsi_locate16(fd, position, format);
	else
		return sodr_tape_drive_scsi_locate10(fd, position, format);
}

static int sodr_tape_drive_scsi_locate10(int fd, struct sodr_tape_drive_scsi_position * position, struct so_media_format * format) {
	struct {
		unsigned char op_code;
		bool immed:1;
		bool change_partition:1;
		bool block_type:1;
		unsigned char reserved0:2;
		unsigned char obsolete:3;
		unsigned char reserved1;
		unsigned int block_address;
		unsigned char reserved2;
		unsigned char partition;
		unsigned char control;
	} __attribute__((packed)) command = {
		.op_code = 0x2B, // LOCATE (10)
		.immed = false,
		.change_partition = format->support_partition,
		.partition = position->partition,
		.block_address = htobe32(position->block_position),
	};

	struct scsi_request_sense sense;
	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = 0;
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = NULL;
	header.timeout = 10380000; // 173 minutes
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);

	return status != 0 || sense.sense_key;
}

static int sodr_tape_drive_scsi_locate16(int fd, struct sodr_tape_drive_scsi_position * position, struct so_media_format * format) {
	struct {
		unsigned char op_code;
		bool immed:1;
		bool change_partition:1;
		bool block_type:1;
		enum {
			block_address = 0x0,
			logical_file_identified = 0x1,
			end_of_data = 0x3
		} dest_type:2;
		unsigned char reserved0:3;
		bool bam:1;
		unsigned char reserved1:7;
		unsigned char partition;
		unsigned long long int block_address;
		unsigned char reserved2[2];
		unsigned char control;
	} __attribute__((packed)) command = {
		.op_code = 0x92, // LOCATE (16)
		.immed = false,
		.dest_type = position->end_of_partition ? end_of_data : block_address,
		.bam = false,
		.partition = position->partition,
		.block_address = htobe64(position->block_position),
		.change_partition = format->support_partition,
	};

	struct scsi_request_sense sense;
	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = 0;
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = NULL;
	header.timeout = 10380000; // 173 minutes
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);

	return status != 0 || sense.sense_key;
}

int sodr_tape_drive_scsi_read_density(struct so_drive * drive, const char * path) {
	if (!scsi_command_report_density_support.available)
		return -1;

	int fd = open(path, O_RDWR);
	if (fd < 0)
		return 1;

	struct scsi_density result;

	struct {
		unsigned char operation_code;
		bool media:1;
		unsigned char reserved0:4;
		unsigned char obsolete:3;
		unsigned char reserved1[5];
		unsigned short allocation_length;
		unsigned char control;
	} __attribute__((packed)) command = {
		.operation_code = 0x44, // Report Density Support (10)
		.media = false,
		.allocation_length = htobe16(sizeof(result)),
		.control = 0,
	};

	struct scsi_request_sense sense;
	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(&result, 0, sizeof(result));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(result);
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = (unsigned char *) &result;
	header.timeout = 1000 * scsi_command_report_density_support.timeout;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int failed = ioctl(fd, SG_IO, &header);
	close(fd);

	if (failed != 0)
		return -1;

	unsigned short int nb_densities = be16toh(result.available_density_descriptor_length);
	unsigned int i;
	for (i = 0; i < nb_densities && result.values[i].primary_density_code > 0 && i < 8; i++)
		if (drive->density_code < result.values[i].primary_density_code)
			drive->density_code = result.values[i].primary_density_code;

	return 0;
}

int sodr_tape_drive_scsi_read_position(int fd, struct sodr_tape_drive_scsi_position * position) {
	if (!scsi_command_read_attribute.available)
		return -1;

	struct {
		bool reserved0:1;
		bool perr:1;
		bool block_position_unknown:1;
		bool reserved1:1;
		bool byte_count_unknown:1;     // BYCU
		bool block_count_unknown:1;    // BCU
		bool end_of_partition:1;       // EOP
		bool beginning_of_partition:1; // BOP
		unsigned char partition_number;
		unsigned char reserved2[2];
		unsigned int first_block_location;
		unsigned int last_block_location;
		unsigned char reserved3;
		unsigned int number_of_blocks_in_buffer[3];
		unsigned int number_of_blocks_in_buffer2[3];
	} __attribute__((packed)) result;

	struct {
		unsigned char operation_code;
		enum {
			scsi_read_attribute_service_action_attributes_values = 0x00,
			scsi_read_attribute_service_action_attribute_list = 0x01,
			scsi_read_attribute_service_action_volume_list = 0x02,
			scsi_read_attribute_service_action_parition_list = 0x03,
		} service_action:5;
		unsigned char obsolete:3;
		unsigned char reserved[5];
		unsigned short parameter_length;
		unsigned char control;
	} __attribute__((packed)) command = {
		.operation_code = 0x34, // READ POSITION (10)
		.service_action = scsi_read_attribute_service_action_attributes_values,
		.parameter_length = 0,
		.control = 0,
	};

	struct scsi_request_sense sense;
	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(&result, 0, sizeof(result));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(result);
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = (unsigned char *) &result;
	header.timeout = 1000 * scsi_command_read_attribute.timeout;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);
	if (status)
		return status;

	bzero(position, sizeof(struct sodr_tape_drive_scsi_position));

	position->partition = result.partition_number;
	position->block_position = be32toh(result.first_block_location);
	position->end_of_partition = result.end_of_partition;

	return 0;
}

int sodr_tape_drive_scsi_read_medium_serial_number(int fd, char * medium_serial_number, size_t length) {
	if (!scsi_command_read_attribute.available)
		return -1;

	struct {
		unsigned char operation_code;
		enum {
			scsi_read_attribute_service_action_attributes_values = 0x00,
			scsi_read_attribute_service_action_attribute_list = 0x01,
			scsi_read_attribute_service_action_volume_list = 0x02,
			scsi_read_attribute_service_action_parition_list = 0x03,
		} service_action:5;
		unsigned char obsolete:3;
		unsigned char reserved0[3];
		unsigned char volume_number;
		unsigned char reserved1;
		unsigned char partition_number;
		unsigned short first_attribute_id;
		unsigned short allocation_length;
		unsigned char reserved2;
		unsigned char control;
	} __attribute__((packed)) command = {
		.operation_code = 0x8C, // READ ATTRIBUTE (16)
		.service_action = scsi_read_attribute_service_action_attributes_values,
		.volume_number = 0,
		.partition_number = 0,
		.first_attribute_id = 0,
		.allocation_length = htobe16(1024),
		.control = 0,
	};

	struct scsi_request_sense sense;
	unsigned char buffer[1024];

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(buffer, 0, 1024);

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(buffer);
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = buffer;
	header.timeout = 1000 * scsi_command_read_attribute.timeout;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);
	if (status)
		return status;

	unsigned int data_available = be32toh(*(unsigned int *) buffer);
	unsigned char * ptr = buffer + 4;

	for (ptr = buffer + 4; ptr < buffer + data_available;) {
		struct sodr_tape_drive_scsi_mam_attribute * attr = (struct sodr_tape_drive_scsi_mam_attribute *) ptr;
		attr->identifier = be16toh(attr->identifier);
		attr->length = be16toh(attr->length);

		ptr += attr->length + 5;

		if (attr->length == 0)
			continue;

		char * space;
		switch (attr->identifier) {
			case sodr_tape_drive_scsi_mam_medium_serial_number:
				strncpy(medium_serial_number, attr->value.text, length);
				space = strchr(medium_serial_number, ' ');
				if (space)
					*space = '\0';
				break;

			default:
				break;
		}
	}

	return 0;
}

int sodr_tape_drive_scsi_read_mam(int fd, struct so_media * media) {
	if (!scsi_command_read_attribute.available)
		return -1;

	struct {
		unsigned char operation_code;
		enum {
			scsi_read_attribute_service_action_attributes_values = 0x00,
			scsi_read_attribute_service_action_attribute_list = 0x01,
			scsi_read_attribute_service_action_volume_list = 0x02,
			scsi_read_attribute_service_action_parition_list = 0x03,
		} service_action:5;
		unsigned char obsolete:3;
		unsigned char reserved0[3];
		unsigned char volume_number;
		unsigned char reserved1;
		unsigned char partition_number;
		unsigned short first_attribute_id;
		unsigned short allocation_length;
		unsigned char reserved2;
		unsigned char control;
	} __attribute__((packed)) command = {
		.operation_code = 0x8C, // READ ATTRIBUTE (16)
		.service_action = scsi_read_attribute_service_action_attributes_values,
		.volume_number = 0,
		.partition_number = 0,
		.first_attribute_id = 0,
		.allocation_length = htobe16(1024),
		.control = 0,
	};

	struct scsi_request_sense sense;
	unsigned char buffer[1024];

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(buffer, 0, 1024);

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(buffer);
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = buffer;
	header.timeout = 60000; // 1 minute
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);
	if (status)
		return status;

	static unsigned long last_hash = 0;
	const unsigned long hash = so_string_compute_hash2(media->name);

	unsigned int data_available = be32toh(*(unsigned int *) buffer);
	unsigned char * ptr = buffer + 4;

	for (ptr = buffer + 4; ptr < buffer + data_available;) {
		struct sodr_tape_drive_scsi_mam_attribute * attr = (struct sodr_tape_drive_scsi_mam_attribute *) ptr;
		attr->identifier = be16toh(attr->identifier);
		attr->length = be16toh(attr->length);

		ptr += attr->length + 5;

		if (attr->length == 0)
			continue;

		char * space;
		char buf[33];
		switch (attr->identifier) {
			case sodr_tape_drive_scsi_mam_load_count:
				media->load_count = be64toh(attr->value.be64);
				break;

			case sodr_tape_drive_scsi_mam_total_written_in_medium_life:
				media->nb_total_write = be64toh(attr->value.be64);
				media->nb_total_write <<= 10;
				break;

			case sodr_tape_drive_scsi_mam_total_read_in_medium_life:
				media->nb_total_read = be64toh(attr->value.be64);
				media->nb_total_read <<= 10;
				break;

			case sodr_tape_drive_scsi_mam_medium_manufacturer:
				if (last_hash != hash) {
					strncpy(buf, attr->value.text, 8);
					buf[8] = '\0';
					so_string_rtrim(buf, ' ');
					so_log_write(so_log_level_debug,
						dgettext("storiqone-drive-tape", "Media information of %s, Medium manufacturer: %s"),
						media->name, buf);
				}
				break;

			case sodr_tape_drive_scsi_mam_medium_serial_number:
				media->medium_serial_number = strdup(attr->value.text);
				space = strchr(media->medium_serial_number, ' ');
				if (space)
					*space = '\0';
				break;

			case sodr_tape_drive_scsi_mam_medium_manufacturer_date:
				if (last_hash != hash) {
					strncpy(buf, attr->value.text, 8);
					buf[8] = '\0';
					so_string_rtrim(buf, ' ');
					so_log_write(so_log_level_debug,
						dgettext("storiqone-drive-tape", "Media information of %s, Medium date: %s"),
						media->name, buf);
				}
				break;

			case sodr_tape_drive_scsi_mam_medium_type:
				switch (attr->value.int8) {
					case 0x01:
						media->type = so_media_type_cleaning;
						break;

					case 0x80:
						media->type = so_media_type_worm;
						break;

					case 0x00:
					default:
						media->type = so_media_type_rewritable;
						break;
				}

			default:
				break;
		}
	}

	last_hash = hash;

	return 0;
}

int sodr_tape_drive_scsi_read_volume_change_reference(int fd, unsigned int * volume_change_reference) {
	if (!scsi_command_read_attribute.available || volume_change_reference == NULL)
		return -1;

	struct {
		unsigned char operation_code;
		enum {
			scsi_read_attribute_service_action_attributes_values = 0x00,
			scsi_read_attribute_service_action_attribute_list = 0x01,
			scsi_read_attribute_service_action_volume_list = 0x02,
			scsi_read_attribute_service_action_parition_list = 0x03,
		} service_action:5;
		unsigned char obsolete:3;
		unsigned char reserved0[3];
		unsigned char volume_number;
		unsigned char reserved1;
		unsigned char partition_number;
		unsigned short first_attribute_id;
		unsigned short allocation_length;
		unsigned char reserved2;
		unsigned char control;
	} __attribute__((packed)) command = {
		.operation_code = 0x8C, // READ ATTRIBUTE (16)
		.service_action = scsi_read_attribute_service_action_attributes_values,
		.volume_number = 0,
		.partition_number = 0,
		.first_attribute_id = 0,
		.allocation_length = htobe16(1024),
		.control = 0,
	};

	struct scsi_request_sense sense;
	unsigned char buffer[1024];

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(buffer, 0, 1024);

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(buffer);
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = buffer;
	header.timeout = 1000 * scsi_command_read_attribute.timeout;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);
	if (status)
		return status;

	unsigned int data_available = be32toh(*(unsigned int *) buffer);
	unsigned char * ptr = buffer + 4;

	for (ptr = buffer + 4; ptr < buffer + data_available;) {
		struct sodr_tape_drive_scsi_mam_attribute * attr = (struct sodr_tape_drive_scsi_mam_attribute *) ptr;
		attr->identifier = be16toh(attr->identifier);
		attr->length = be16toh(attr->length);

		ptr += attr->length + 5;

		if (attr->length == 0)
			continue;

		switch (attr->identifier) {
			case sodr_tape_drive_scsi_mam_volume_change_reference:
				*volume_change_reference = be32toh(attr->value.be32);
				break;

			default:
				break;
		}
	}

	return 0;
}

int sodr_tape_drive_scsi_read_volume_coherency(int fd, struct sodr_tape_drive_ltfs_volume_coherency * volume_coherency, unsigned char part) {
	if (!scsi_command_read_attribute.available || volume_coherency == NULL)
		return -1;

	struct {
		unsigned char operation_code;
		enum {
			scsi_read_attribute_service_action_attributes_values = 0x00,
			scsi_read_attribute_service_action_attribute_list = 0x01,
			scsi_read_attribute_service_action_volume_list = 0x02,
			scsi_read_attribute_service_action_parition_list = 0x03,
		} service_action:5;
		unsigned char obsolete:3;
		unsigned char reserved0[3];
		unsigned char volume_number;
		unsigned char reserved1;
		unsigned char partition_number;
		unsigned short first_attribute_id;
		unsigned short allocation_length;
		unsigned char reserved2;
		unsigned char control;
	} __attribute__((packed)) command = {
		.operation_code = 0x8C, // READ ATTRIBUTE (16)
		.service_action = scsi_read_attribute_service_action_attributes_values,
		.volume_number = 0,
		.partition_number = part,
		.first_attribute_id = 0,
		.allocation_length = htobe16(1024),
		.control = 0,
	};

	struct scsi_request_sense sense;
	unsigned char buffer[1024];

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(buffer, 0, 1024);

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(buffer);
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = buffer;
	header.timeout = 1000 * scsi_command_read_attribute.timeout;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);
	if (status)
		return status;

	unsigned int data_available = be32toh(*(unsigned int *) buffer);
	unsigned char * ptr = buffer + 4;

	for (ptr = buffer + 4; ptr < buffer + data_available;) {
		struct sodr_tape_drive_scsi_mam_attribute * attr = (struct sodr_tape_drive_scsi_mam_attribute *) ptr;
		attr->identifier = be16toh(attr->identifier);
		attr->length = be16toh(attr->length);

		ptr += attr->length + 5;

		if (attr->length == 0)
			continue;

		switch (attr->identifier) {
			case sodr_tape_drive_scsi_mam_volume_coherency_infomation: {
					struct sodr_tape_drive_scsi_volume_coherency_information * vci = (struct sodr_tape_drive_scsi_volume_coherency_information *) &attr->value.text;
					if (vci->volume_change_reference_value_length != 8)
						return 1;

					vci->volume_change_reference_value = be64toh(vci->volume_change_reference_value);
					vci->volume_coherency_count = be64toh(vci->volume_coherency_count);
					vci->volume_coherency_set_identifier = be64toh(vci->volume_coherency_set_identifier);
					vci->application_client_specific_information_length = be16toh(vci->application_client_specific_information_length);

					volume_coherency->volume_change_reference = vci->volume_change_reference_value;
					volume_coherency->generation_number = vci->volume_coherency_count;
					volume_coherency->block_position_of_last_index = vci->volume_coherency_set_identifier;

					break;
				}

			default:
				break;
		}
	}

	return 0;
}

int sodr_tape_drive_scsi_rewind(int fd) {
	if (!scsi_command_rewind.available)
		return -1;

	struct {
		unsigned char op_code;
		bool immed:1;
		unsigned char reserved0:4;
		unsigned char obsolete:3;
		unsigned int reserved1:24;
		unsigned char control;
	} __attribute__((packed)) command = {
		.op_code = 0x01, // REWIND (6)
		.immed = false,
	};

	struct scsi_request_sense sense;
	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = 0;
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = NULL;
	header.timeout = 1000 * scsi_command_rewind.timeout;
	header.dxfer_direction = SG_DXFER_NONE;

	int status = ioctl(fd, SG_IO, &header);

	return status != 0 || sense.sense_key;
}

int sodr_tape_drive_scsi_setup(const char * path) {
	int fd = open(path, O_RDWR);
	if (fd < 0)
		return -1;

	sodr_tape_drive_scsi_setup2(fd, &scsi_command_erase);
	sodr_tape_drive_scsi_setup2(fd, &scsi_command_inquiry);
	sodr_tape_drive_scsi_setup2(fd, &scsi_command_locate10);
	sodr_tape_drive_scsi_setup2(fd, &scsi_command_locate16);
	sodr_tape_drive_scsi_setup2(fd, &scsi_command_log_sense);
	sodr_tape_drive_scsi_setup2(fd, &scsi_command_read_attribute);
	sodr_tape_drive_scsi_setup2(fd, &scsi_command_read_position);
	sodr_tape_drive_scsi_setup2(fd, &scsi_command_report_density_support);
	sodr_tape_drive_scsi_setup2(fd, &scsi_command_rewind);
	sodr_tape_drive_scsi_setup2(fd, &scsi_command_write_attribute);

	close(fd);

	return 0;
}

static int sodr_tape_drive_scsi_setup2(int fd, struct scsi_command * scsi_command) {
	unsigned char buffer[48];
	memset(&buffer, 0, 48);

	struct report_result {
		unsigned char reserved0;
		enum {
			unsupported_command = 0x1,
			supported_command_scsi_standard = 0x3,
			supported_command_vendor_specific = 0x5
		} support:3;
		unsigned char reserved1:4;
		bool command_timeout_descriptor:1;
		unsigned short cdb_length;
		// unsigned char cdb_usage_data[scsi_command->cdb_length];
	} __attribute__((packed));

	struct timeout_descriptor {
		unsigned short descriptor_length;
		unsigned char reserved2;
		unsigned char command_specific;
		unsigned int nominal_command_processing_timeout;
		unsigned int recommended_command_timeout;
	} __attribute__((packed));

	struct {
		unsigned char op_code;
		unsigned char service_action:5;
		unsigned char reserved0:3;
		enum {
			reporting_options_list_op_codes = 0x0,
			reporting_options_single_op_code = 0x1,
			reporting_options_single_op_code_and_service_action = 0x2
		} reporting_options:3;
		unsigned char reserved1:4;
		bool return_command_timeouts_descriptor:1;
		unsigned char requested_operation_code;
		unsigned short requested_service_action;
		unsigned int allocation_length;
		unsigned char reserved2;
		unsigned char control;
	} __attribute__((packed)) command = {
		.op_code = 0xA3, // REPORT SUPPORTED OPCODES (A3 (0C))
		.service_action = 0x0C,
		.reporting_options = reporting_options_single_op_code,
		.return_command_timeouts_descriptor = true,
		.requested_operation_code = scsi_command->op_code,
		.requested_service_action = 0x0,
		.allocation_length = htobe32(48),
	};

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));

	struct scsi_request_sense sense;
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = 48;
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = buffer;
	header.timeout = 60000; // 1 minutes
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);

	if (status == 0) {
		struct report_result * result = (struct report_result *) &buffer;
		result->cdb_length = be16toh(result->cdb_length);

		struct timeout_descriptor * timeout = (struct timeout_descriptor *) (buffer + result->cdb_length + 4);

		scsi_command->available = result->support != unsupported_command;
		scsi_command->timeout = be32toh(timeout->recommended_command_timeout);

		so_log_write(so_log_level_debug,
				dgettext("storiqone-drive-tape", "SCSI supported command: '%s', available: %s, timeout: %u seconds"),
				scsi_command->name, scsi_command->available ? dgettext("storiqone-drive-tape", "yes") : dgettext("storiqone-drive-tape", "no"), scsi_command->timeout);
	}

	return status;
}

int sodr_tape_drive_scsi_size_available(int fd, struct so_media * media) {
	struct {
		unsigned char page_code:6;
		unsigned char reserved0:2;
		unsigned char reserved1;
		unsigned short page_length;
		struct {
			unsigned short parameter_code;
			bool lp:1;
			bool lbin:1;
			unsigned char tmc:2;
			bool etc:1;
			bool tsd:1;
			bool ds:1;
			bool du:1;
			unsigned char parameter_length;
			unsigned int value;
		} values[4];
	} __attribute__((packed)) result;

	struct {
		unsigned char operation_code;
		bool saved_paged:1;
		bool parameter_pointer_control:1;
		unsigned char reserved0:3;
		unsigned char obsolete:3;
		unsigned char page_code:6;
		enum {
			page_control_max_value = 0x0,
			page_control_current_value = 0x1,
			page_control_max_value2 = 0x2, // same as page_control_max_value ?
			page_control_power_on_value = 0x3,
		} page_control:2;
		unsigned char reserved1[2];
		unsigned short parameter_pointer; // must be a bigger endian integer
		unsigned short allocation_length; // must be a bigger endian integer
		unsigned char control;
	} __attribute__((packed)) command = {
		.operation_code = 0x4D, // Log Sense (10)
		.saved_paged = false,
		.parameter_pointer_control = false,
		.page_code = 0x31,
		.page_control = page_control_current_value,
		.parameter_pointer = 0,
		.allocation_length = htobe16(sizeof(result)),
		.control = 0,
	};

	struct scsi_request_sense sense;
	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(&result, 0, sizeof(result));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(result);
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = (unsigned char *) &result;
	header.timeout = 1000 * scsi_command_log_sense.timeout;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);
	if (status || result.page_code != 0x31)
		return 1;

	result.page_length = be16toh(result.page_length);
	unsigned short i;
	for (i = 0; i < 4; i++) {
		result.values[i].parameter_code = be16toh(result.values[i].parameter_code);
		result.values[i].value = be32toh(result.values[i].value);
	}

	unsigned long long int free_block = result.values[0].value;
	if (result.values[3].value > 0)
		free_block = result.values[1].value;
	free_block <<= 20;
	media->free_block = free_block / media->block_size;

	unsigned long long int total_block = result.values[2].value + result.values[3].value;
	total_block <<= 20;
	media->total_block = total_block / media->block_size;

	return 0;
}

int sodr_tape_drive_scsi_test_unit_ready(int fd) {
	struct {
		unsigned char operation_code;
		unsigned char reserved[5];
	} __attribute__((packed)) command = {
		.operation_code = 0x00,
		.reserved = { 0, 0, 0, 0, 0 },
	};

	sg_io_hdr_t header;
	struct scsi_request_sense sense;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = 0;
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = NULL;
	header.timeout = 60000; // 1 min
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	ioctl(fd, SG_IO, &header);

	return sense.sense_key;
}

int sodr_tape_drive_scsi_test_unit_ready2(int fd) {
	int try, status = 1;
	for (try = 0; try < 5 && status != 0; try++) {
		status = sodr_tape_drive_scsi_test_unit_ready(fd);
		if (status != 0)
			sleep(1);
	}
	return status;
}

int sodr_tape_drive_scsi_write_attribute(int fd, struct sodr_tape_drive_scsi_mam_attribute * attribute, unsigned char part) {
	if (!scsi_command_write_attribute.available)
		return -1;

	unsigned int attribute_length = 5 + attribute->length;

	attribute->identifier = htobe16(attribute->identifier);
	attribute->length = htobe16(attribute->length);

	if (attribute->format == sodr_tape_drive_scsi_mam_attribute_format_binary) {
		if (attribute->length == 2)
			attribute->value.be16 = htobe16(attribute->value.be16);
		else if (attribute->length == 4)
			attribute->value.be32 = htobe32(attribute->value.be32);
		else if (attribute->length == 8)
			attribute->value.be64 = htobe64(attribute->value.be64);
	}

	struct {
		unsigned int data_available;
		struct sodr_tape_drive_scsi_mam_attribute attr;
	} __attribute__((packed)) data = {
		.data_available = htobe32(attribute_length),
		.attr = *attribute
	};

	struct {
		unsigned char operation_code;
		bool write_through_cache:1;
		unsigned char reserved0:7;
		unsigned char reserved1[3];
		unsigned char volume_number;
		unsigned char reserved2;
		unsigned char partition_number;
		unsigned short reserved3;
		unsigned int parameter_list_length;
		unsigned char reserved4;
		unsigned char control;
	} __attribute__((packed)) command = {
		.operation_code = 0x8D, // WRITE ATTRIBUTE (16)
		.write_through_cache = false,
		.volume_number = 0,
		.partition_number = part,
		.parameter_list_length = htobe32(4 + attribute_length),
		.control = 0,
	};

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));

	struct scsi_request_sense sense;
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = 4 + attribute_length;
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = (unsigned char *) &data;
	header.timeout = 1000 * scsi_command_read_attribute.timeout;
	header.dxfer_direction = SG_DXFER_TO_DEV;

	if (sodr_tape_drive_scsi_test_unit_ready2(fd) != 0)
		return 1;

	bool ok = false;
	int status, try;
	for (try = 0; try < 5 && !ok; try++) {
		status = ioctl(fd, SG_IO, &header);
		if (sense.error_code != 0) {
			status = sense.error_code;
			if (sodr_tape_drive_scsi_test_unit_ready2(fd) != 0)
				return 1;
		} else
			ok = true;
	}

	return status;
}

