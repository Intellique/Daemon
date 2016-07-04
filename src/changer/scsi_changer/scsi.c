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

#define _GNU_SOURCE
// open, size_t
#include <sys/types.h>

// be*toh, htobe*
#include <endian.h>
// open
#include <fcntl.h>
// glob, globfree
#include <glob.h>
// sg_io_hdr_t
#include <scsi/scsi.h>
// sg_io_hdr_t
#include <scsi/sg.h>
// bool
#include <stdbool.h>
// printf, sscanf, snprintf
#include <stdio.h>
// free
#include <stdlib.h>
// memcpy, memset, strdup
#include <string.h>
// ioctl
#include <sys/ioctl.h>
// close
#include <unistd.h>

#include <libstoriqone/changer.h>
#include <libstoriqone/file.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>
#include <libstoriqone-changer/drive.h>

#include "scsi.h"

enum scsi_loader_element_type {
	scsi_loader_element_type_all_elements             = 0x0,
	scsi_loader_element_type_medium_transport_element = 0x1,
	scsi_loader_element_type_storage_element          = 0x2,
	scsi_loader_element_type_import_export_element    = 0x3,
	scsi_loader_element_type_data_transfer            = 0x4,
};

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

struct scsi_loader_data_transfer_element {
	unsigned short element_address;
	bool full:1;
	bool reserved0:1;
	bool execpt:1;
	bool access:1;
	unsigned char reserved1:4;
	unsigned char reserved2;
	unsigned char additional_sense_code;
	unsigned char additional_sense_code_qualifier;
	unsigned char logical_unit_number:3;
	bool reserved3:1;
	bool logical_unit_valid:1;
	bool id_valid:1;
	unsigned char reserved4:2;
	unsigned char scsi_bus_address;
	unsigned char reserved5;
	unsigned char reserved6:6;
	bool invert:1;
	bool source_valid:1;
	unsigned short source_storage_element_address;
	char primary_volume_tag_information[36];
	unsigned char code_set_1:4;
	unsigned char reserved7:4;
	unsigned char identifier_type_1:4;
	unsigned char reserved8:4;
	unsigned char reserved9;
	unsigned char identifier_length_1;
	char device_identifier_1[34];
	unsigned char code_set_2:4;
	unsigned char reserved10:4;
	unsigned char identifier_type_2:4;
	unsigned char reserved11:4;
	unsigned char reserved12;
	unsigned char identifier_length_2;
	unsigned char device_identifier_2[8];
	unsigned char tape_drive_serial_number[18];
} __attribute__((packed));

struct scsi_loader_element_status {
	enum scsi_loader_element_type type:8;
	unsigned char reserved0:6;
	bool alternate_volume_tag:1;
	bool primary_volume_tag:1;
	unsigned short element_descriptor_length;
	unsigned char reserved1;
	unsigned int byte_count_of_descriptor_data_available:24;
} __attribute__((packed));

struct scsi_loader_import_export_element {
	unsigned short element_address;
	bool full:1;
	bool import_export:1;
	bool execpt:1;
	bool access:1;
	bool export_enable:1;
	bool import_enable:1;
	unsigned char reserved0:2;
	unsigned char reserved1;
	unsigned char additional_sense_code;
	unsigned char additional_sense_code_qualifier;
	unsigned char reserved2[3];
	unsigned char reserved3:6;
	bool invert:1;
	bool source_valid:1;
	unsigned short source_storage_element_address;
	char primary_volume_tag_information[36];
	unsigned char reserved4[4];
} __attribute__((packed));

struct scsi_loader_medium_transport_element {
	unsigned short element_address;
	bool full:1;
	bool reserved0:1;
	bool execpt:1;
	unsigned char element_descriptor_length:5;
	unsigned char reserved2;
	unsigned char additional_sense_code;
	unsigned char additional_sense_code_qualifier;
	unsigned char reserved3[3];
	unsigned char reserved4:6;
	bool invert:1;
	bool source_valid:1;
	unsigned short source_storage_element_address;
	unsigned char primary_volume_tag_information[36];
	unsigned char reserved5[4];
} __attribute__((packed));

struct scsi_loader_storage_element {
	unsigned short element_address;
	bool full:1;
	bool reserved0:1;
	bool execpt:1;
	bool access:1;
	unsigned char reserved1:4;
	unsigned char reserved2;
	unsigned char additional_sense_code;
	unsigned char additional_sense_code_qualifier;
	unsigned char reserved3[3];
	unsigned char reserved4:6;
	bool invert:1;
	bool source_valid:1;
	unsigned short source_storage_element_address;
	char primary_volume_tag_information[36];
	unsigned char reserved5[4];
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


static void sochgr_scsi_changer_scsi_setup_drive(struct so_drive * drive, struct so_value * config);
static void sochgr_scsi_changer_scsi_update_status2(int fd, struct so_changer * changer, const char * device, struct so_slot * slots, int start_element, unsigned int nb_elements, enum scsi_loader_element_type type, struct so_value * available_drives);


void sochgr_scsi_changer_scsi_loader_check_slot(struct so_changer * changer, const char * device, struct so_slot * slot) {
	int fd = open(device, O_RDWR);
	if (fd < 0)
		return;

	enum scsi_loader_element_type type = scsi_loader_element_type_storage_element;
	if (slot->drive != NULL)
		type = scsi_loader_element_type_data_transfer;
	else if (slot->is_ie_port)
		type = scsi_loader_element_type_import_export_element;

	struct sochgr_scsi_changer_slot * sp = slot->data;
	sochgr_scsi_changer_scsi_update_status2(fd, changer, device, slot, sp->address, 1, type, NULL);

	close(fd);
}

bool sochgr_scsi_changer_scsi_check_changer(struct so_changer * changer, const char * path) {
	int fd = open(path, O_RDWR);
	if (fd < 0)
		return false;

	struct {
		unsigned char peripheral_device_type:5;
		unsigned char peripheral_device_qualifier:3;
		unsigned char reserved0:7;
		unsigned char removable_medium_bit:1;
		unsigned char version;
		unsigned char response_data_format:4;
		unsigned char hi_sup:1;
		unsigned char norm_aca:1;
		unsigned char reserved1:1;
		unsigned char asynchronous_event_reporting_capability:1;
		unsigned char additional_length;
		unsigned char reserved2:7;
		unsigned char scc_supported:1;
		unsigned char addr16:1;
		unsigned char reserved3:2;
		unsigned char medium_changer:1;
		unsigned char multi_port:1;
		unsigned char reserved4:1;
		unsigned char enclosure_service:1;
		unsigned char basic_queuing:1;
		unsigned char reserved5:1;
		unsigned char command_queuing:1;
		unsigned char reserved6:1;
		unsigned char linked_command:1;
		unsigned char synchonous_transfer:1;
		unsigned char wide_bus_16:1;
		unsigned char reserved7:1;
		unsigned char relative_addressing:1;
		char vendor_identification[8];
		char product_identification[16];
		char product_revision_level[4];
		unsigned char full_firmware_revision_level[19];
		unsigned char bar_code:1;
		unsigned char reserved8:7;
		unsigned char information_units_supported:1;
		unsigned char quick_arbitration_supported:1;
		unsigned char clocking:2;
		unsigned char reserved9:4;
		unsigned char reserved10;
		unsigned char version_description[16];
		unsigned char reserved11[22];
		unsigned char unit_serial_number[12];
	} __attribute__((packed)) result_inquiry;

	struct scsi_inquiry command_inquiry = {
		.operation_code = 0x12,
		.enable_vital_product_data = 0,
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
	header.timeout = 60000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);

	if (status) {
		close(fd);
		return false;
	}

	char * model = strndup(result_inquiry.product_identification, 16);
	so_string_rtrim(model, ' ');

	char * vendor = strndup(result_inquiry.vendor_identification, 8);
	so_string_rtrim(vendor, ' ');

	char * revision = strndup(result_inquiry.product_revision_level, 4);
	so_string_rtrim(revision, ' ');

	bool barcode = result_inquiry.bar_code;


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
	header.timeout = 60000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	status = ioctl(fd, SG_IO, &header);
	close(fd);

	if (status != 0) {
		close(fd);
		free(model);
		free(vendor);
		free(revision);
		return false;
	}

	so_string_rtrim(result_serial_number.unit_serial_number, ' ');
	char * serial_number = strdup(result_serial_number.unit_serial_number);

	bool found = !strcmp(vendor, changer->vendor) && !strcmp(model, changer->model) && !strcmp(serial_number, changer->serial_number);

	if (found) {
		free(changer->revision);
		changer->revision = revision;
		changer->barcode = barcode;
	} else {
		free(revision);
	}

	free(model);
	free(vendor);
	free(serial_number);

	return found;
}

bool sochgr_scsi_changer_scsi_check_drive(struct so_drive * drive, const char * path) {
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
		.operation_code = 0x12,
		.enable_vital_product_data = 0,
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
	header.timeout = 60000;
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
	char * revision = strndup(result_inquiry.product_revision_level, 4);
	so_string_rtrim(revision, ' ');

	bool ok = !strcmp(drive->vendor, vendor);
	if (ok)
		ok = !strcmp(drive->model, model);

	free(vendor);
	free(model);
	free(revision);

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
	header.timeout = 60000;
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

	return ok;
}

int sochgr_scsi_changer_scsi_loader_ready(const char * device) {
	int fd = open(device, O_RDWR);
	if (fd < 0)
		return 1;

	struct scsi_request_sense sense;
	struct {
		unsigned char operation_code;
		unsigned char reserved[5];
	} __attribute__((packed)) command = {
		.operation_code = 0x00,
		.reserved = { 0, 0, 0, 0, 0 },
	};

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
	header.timeout = 1200000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	ioctl(fd, SG_IO, &header);
	close(fd);

	return sense.sense_key;
}

void sochgr_scsi_changer_scsi_new_status(struct so_changer * changer, const char * device, struct so_value * available_drives, int * transport_address) {
	int fd = open(device, O_RDWR);
	if (fd < 0)
		return;

	so_file_close_fd_on_exec(fd, true);

	struct scsi_request_sense sense;
	struct {
		unsigned char mode_data_length;
		unsigned char reserved0[3];

		unsigned char page_code:6;
		bool reserved1:1;
		bool page_saved:1;
		unsigned char parameter_length;
		unsigned short medium_transport_element_address;
		unsigned short number_of_medium_transport_elements;
		unsigned short first_storage_element_address;
		unsigned short number_of_storage_elements;
		unsigned short first_import_export_element_address;
		unsigned short number_of_import_export_elements;
		unsigned short first_data_transfer_element_address;
		unsigned short number_of_data_transfer_elements;
		unsigned char reserved2[2];
	} __attribute__((packed)) result;
	struct {
		unsigned char operation_code;
		unsigned char reserved0:3;
		bool disable_block_descriptors:1;
		unsigned char reserved1:4;
		enum {
			page_code_element_address_assignement_page = 0x1D,
			page_code_transport_geometry_descriptor_page = 0x1E,
			page_code_device_capabilities_page = 0x1F,
			page_code_unique_properties_page = 0x21,
			page_code_lcd_mode_page = 0x22,
			page_code_cleaning_configuration_page = 0x25,
			page_code_operating_mode_page = 0x26,
			page_code_all_pages = 0x3F,
		} code_page:6;
		enum {
			page_control_current_value = 0x00,
			page_control_changeable_value = 0x01,
			page_control_default_value = 0x02,
			page_control_saved_value = 0x03,
		} page_control:2;
		unsigned char reserved2;
		unsigned char allocation_length;
		unsigned char reserved3;
	} __attribute__((packed)) command = {
		.operation_code = 0x1A,
		.reserved0 = 0,
		.disable_block_descriptors = false,
		.reserved1 = 0,
		.code_page = page_code_element_address_assignement_page,
		.page_control = page_control_current_value,
		.reserved2 = 0,
		.allocation_length = sizeof(result),
		.reserved3 = 0,
	};

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
	header.timeout = 1200000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int failed = ioctl(fd, SG_IO, &header);
	if (failed)
		return;

	result.medium_transport_element_address = be16toh(result.medium_transport_element_address);
	result.number_of_medium_transport_elements = be16toh(result.number_of_medium_transport_elements);
	result.first_storage_element_address = be16toh(result.first_storage_element_address);
	result.number_of_storage_elements = be16toh(result.number_of_storage_elements);
	result.first_import_export_element_address = be16toh(result.first_import_export_element_address);
	result.number_of_import_export_elements = be16toh(result.number_of_import_export_elements);
	result.first_data_transfer_element_address = be16toh(result.first_data_transfer_element_address);
	result.number_of_data_transfer_elements = be16toh(result.number_of_data_transfer_elements);

	if (transport_address != NULL)
		*transport_address = result.medium_transport_element_address;

	changer->nb_drives = result.number_of_data_transfer_elements;
	changer->drives = calloc(sizeof(struct so_drive), changer->nb_drives);

	changer->nb_slots = result.number_of_storage_elements;
	unsigned short nb_io_slots = result.number_of_import_export_elements;
	if (nb_io_slots > 0)
		changer->nb_slots += nb_io_slots;
	changer->nb_slots += changer->nb_drives;

	if (changer->nb_slots > 0)
		changer->slots = calloc(sizeof(struct so_slot), changer->nb_slots);

	unsigned int i;
	for (i = 0; i < changer->nb_slots; i++) {
		struct so_slot * slot = changer->slots + i;

		slot->changer = changer;
		slot->drive = NULL;
		slot->media = NULL;

		slot->index = i;
		slot->volume_name = NULL;
		slot->full = false;

		struct sochgr_scsi_changer_slot * sp = slot->data = malloc(sizeof(struct sochgr_scsi_changer_slot));
		sp->address = sp->src_address = 0;
		sp->src_slot = NULL;
	}

	sochgr_scsi_changer_scsi_update_status2(fd, changer, device, changer->slots, result.first_data_transfer_element_address, result.number_of_data_transfer_elements, scsi_loader_element_type_data_transfer, available_drives);
	sochgr_scsi_changer_scsi_update_status2(fd, changer, device, changer->slots + result.number_of_data_transfer_elements, result.first_storage_element_address, result.number_of_storage_elements, scsi_loader_element_type_storage_element, NULL);
	if (result.number_of_import_export_elements > 0)
		sochgr_scsi_changer_scsi_update_status2(fd, changer, device, changer->slots + (result.number_of_data_transfer_elements + result.number_of_storage_elements), result.first_import_export_element_address, result.number_of_import_export_elements, scsi_loader_element_type_import_export_element, NULL);

	for (i = 0; i < changer->nb_drives; i++) {
		struct so_drive * drive = changer->drives + i;
		if (!drive->enable)
			continue;

		drive->ops->update_status(drive);

		struct so_slot * sl = changer->slots + i;
		struct sochgr_scsi_changer_slot * sp = sl->data;

		if (sp->src_address == 0)
			continue;

		unsigned int j;
		for (j = changer->nb_drives; j < changer->nb_slots; j++) {
			struct so_slot * sl2 = changer->slots + j;
			struct sochgr_scsi_changer_slot * sp2 = sl2->data;

			if (sp->src_address == sp2->address) {
				sp->src_slot = sl2;
				break;
			}
		}
	}

	close(fd);
}

int sochgr_scsi_changer_scsi_medium_removal(const char * device, bool allow) {
	int fd = open(device, O_RDWR);
	if (fd < 0)
		return -1;

	struct scsi_request_sense sense;
	struct {
		unsigned char operation_code;
		unsigned char reserved0[3];
		enum {
			medium_removal_allow = 0x00,
			medium_removal_prevent = 0x01,
		} prevent:2;
		unsigned char reserved1:6;
		unsigned char reserved2;
	} __attribute__((packed)) command = {
		.operation_code = 0x1E,
		.reserved0 = { 0, 0, 0 },
		.prevent = allow ? medium_removal_allow : medium_removal_prevent,
		.reserved1 = 0,
		.reserved2 = 0,
	};

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
	header.timeout = 1200000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int failed = ioctl(fd, SG_IO, &header);
	close(fd);

	return failed;
}

int sochgr_scsi_changer_scsi_move(const char * device, int transport_address, struct so_slot * from, struct so_slot * to) {
	int fd = open(device, O_RDWR);
	if (fd < 0)
		return -1;

	struct sochgr_scsi_changer_slot * sf = from->data;
	struct sochgr_scsi_changer_slot * st = to->data;

	struct scsi_request_sense sense;
	struct {
		unsigned char operation_code;
		unsigned char reserved0;
		unsigned short transport_element_address;
		unsigned short source_address;
		unsigned short destination_address;
		unsigned char reserved1[2];
		bool invert:1;
		unsigned char reserved2:7;
		unsigned char reserved3;
	} __attribute__((packed)) command = {
		.operation_code = 0xA5,
		.reserved0 = 0,
		.transport_element_address = htobe16(transport_address),
		.source_address = htobe16(sf->address),
		.destination_address = htobe16(st->address),
		.reserved1 = { 0, 0 },
		.invert = false,
		.reserved2 = 0,
		.reserved3 = 0,
	};

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
	header.timeout = 1200000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	ioctl(fd, SG_IO, &header);

	close(fd);

	return sense.sense_key;
}

static void sochgr_scsi_changer_scsi_setup_drive(struct so_drive * drive, struct so_value * config) {
	glob_t gl;
	glob("/sys/class/scsi_device/*/device/scsi_tape", 0, NULL, &gl);

	unsigned int i;
	for (i = 0; i < gl.gl_pathc; i++) {
		char * ptr = strrchr(gl.gl_pathv[i], '/');
		*ptr = '\0';

		char link[256];
		ssize_t length = readlink(gl.gl_pathv[i], link, 256);
		link[length] = '\0';

		ptr = strrchr(link, '/') + 1;

		char * path;
		int size = asprintf(&path, "%s/generic", gl.gl_pathv[i]);
		if (size < 0)
			continue;

		length = readlink(path, link, 256);
		if (length < 0)
			continue;

		link[length] = '\0';
		free(path);

		char * scsi_device;
		ptr = strrchr(link, '/');
		size = asprintf(&scsi_device, "/dev%s", ptr);

		if (size < 0) {
			free(path);
			continue;
		}

		int fd = open(scsi_device, O_RDWR);
		free(scsi_device);

		if (fd < 0)
			continue;

		struct {
			unsigned char peripheral_device_type:5;
			unsigned char peripheral_device_qualifier:3;
			unsigned char reserved0:7;
			bool removable_medium_bit:1;
			unsigned char version:3;
			unsigned char ecma_version:3;
			unsigned char iso_version:2;
			unsigned char response_data_format:4;
			bool hi_sup:1;
			bool norm_aca:1;
			bool obsolete0:1;
			bool asynchronous_event_reporting_capability:1;
			unsigned char additional_length;
			unsigned char reserved1:7;
			bool scc_supported:1;
			bool addr16:1;
			bool addr32:1;
			bool obsolete1:1;
			bool medium_changer:1;
			bool multi_port:1;
			bool vs:1;
			bool enclosure_service:1;
			bool basic_queuing:1;
			bool reserved2:1;
			bool command_queuing:1;
			bool trans_dis:1;
			bool linked_command:1;
			bool synchonous_transfer:1;
			bool wide_bus_16:1;
			bool obsolete2:1;
			bool relative_addressing:1;
			char vendor_identification[8];
			char product_identification[16];
			char product_revision_level[4];
			bool aut_dis:1;
			unsigned char performance_limit;
			unsigned char reserved3[3];
			unsigned char oem_specific;
		} __attribute__((packed)) result_inquiry;

		struct scsi_inquiry command_inquiry = {
			.operation_code = 0x12,
			.enable_vital_product_data = 0,
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
		header.timeout = 60000;
		header.dxfer_direction = SG_DXFER_FROM_DEV;

		int status = ioctl(fd, SG_IO, &header);

		if (status) {
			close(fd);
			continue;
		}

		char * vendor = strndup(result_inquiry.vendor_identification, 7);
		so_string_rtrim(vendor, ' ');
		bool ok = !strcmp(vendor, drive->vendor);

		char * model = strndup(result_inquiry.product_identification, 15);
		so_string_rtrim(model, ' ');
		if (ok)
			ok = !strcmp(model, drive->model);

		free(vendor);
		free(model);

		if (!ok) {
			close(fd);
			continue;
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
		header.timeout = 60000;
		header.dxfer_direction = SG_DXFER_FROM_DEV;

		status = ioctl(fd, SG_IO, &header);

		result_serial_number.unit_serial_number[11] = '\0';
		so_string_rtrim(result_serial_number.unit_serial_number, ' ');
		char * serial_number = strdup(result_serial_number.unit_serial_number);

		ok = !strcmp(serial_number, drive->serial_number);

		free(serial_number);
		close(fd);

		if (ok) {
			sochgr_drive_register(drive, config, "tape_drive");
			break;
		}
	}

	globfree(&gl);
}

void sochgr_scsi_changer_scsi_update_status(struct so_changer * changer, const char * device) {
	int fd = open(device, O_RDWR);
	if (fd < 0)
		return;

	struct scsi_request_sense sense;
	struct {
		unsigned char mode_data_length;
		unsigned char reserved0[3];

		unsigned char page_code:6;
		bool reserved1:1;
		bool page_saved:1;
		unsigned char parameter_length;
		unsigned short medium_transport_element_address;
		unsigned short number_of_medium_transport_elements;
		unsigned short first_storage_element_address;
		unsigned short number_of_storage_elements;
		unsigned short first_import_export_element_address;
		unsigned short number_of_import_export_elements;
		unsigned short first_data_transfer_element_address;
		unsigned short number_of_data_transfer_elements;
		unsigned char reserved2[2];
	} __attribute__((packed)) result;
	struct {
		unsigned char operation_code;
		unsigned char reserved0:3;
		bool disable_block_descriptors:1;
		unsigned char reserved1:4;
		enum {
			page_code_element_address_assignement_page = 0x1D,
			page_code_transport_geometry_descriptor_page = 0x1E,
			page_code_device_capabilities_page = 0x1F,
			page_code_unique_properties_page = 0x21,
			page_code_lcd_mode_page = 0x22,
			page_code_cleaning_configuration_page = 0x25,
			page_code_operating_mode_page = 0x26,
			page_code_all_pages = 0x3F,
		} code_page:6;
		enum {
			page_control_current_value = 0x00,
			page_control_changeable_value = 0x01,
			page_control_default_value = 0x02,
			page_control_saved_value = 0x03,
		} page_control:2;
		unsigned char reserved2;
		unsigned char allocation_length;
		unsigned char reserved3;
	} __attribute__((packed)) command = {
		.operation_code = 0x1A,
		.reserved0 = 0,
		.disable_block_descriptors = false,
		.reserved1 = 0,
		.code_page = page_code_element_address_assignement_page,
		.page_control = page_control_current_value,
		.reserved2 = 0,
		.allocation_length = sizeof(result),
		.reserved3 = 0,
	};

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
	header.timeout = 1200000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int failed = ioctl(fd, SG_IO, &header);
	if (failed)
		return;

	result.medium_transport_element_address = be16toh(result.medium_transport_element_address);
	result.number_of_medium_transport_elements = be16toh(result.number_of_medium_transport_elements);
	result.first_storage_element_address = be16toh(result.first_storage_element_address);
	result.number_of_storage_elements = be16toh(result.number_of_storage_elements);
	result.first_import_export_element_address = be16toh(result.first_import_export_element_address);
	result.number_of_import_export_elements = be16toh(result.number_of_import_export_elements);
	result.first_data_transfer_element_address = be16toh(result.first_data_transfer_element_address);
	result.number_of_data_transfer_elements = be16toh(result.number_of_data_transfer_elements);

	sochgr_scsi_changer_scsi_update_status2(fd, changer, device, changer->slots, result.first_data_transfer_element_address, result.number_of_data_transfer_elements, scsi_loader_element_type_data_transfer, NULL);
	sochgr_scsi_changer_scsi_update_status2(fd, changer, device, changer->slots + result.number_of_data_transfer_elements, result.first_storage_element_address, result.number_of_storage_elements, scsi_loader_element_type_storage_element, NULL);
	if (result.number_of_import_export_elements > 0)
		sochgr_scsi_changer_scsi_update_status2(fd, changer, device, changer->slots + (result.number_of_data_transfer_elements + result.number_of_storage_elements), result.first_import_export_element_address, result.number_of_import_export_elements, scsi_loader_element_type_import_export_element, NULL);

	close(fd);
}

static void sochgr_scsi_changer_scsi_update_status2(int fd, struct so_changer * changer, const char * device, struct so_slot * slots, int start_element, unsigned int nb_elements, enum scsi_loader_element_type type, struct so_value * available_drives) {
	size_t size_needed = 16;
	switch (type) {
		case scsi_loader_element_type_all_elements:
		case scsi_loader_element_type_data_transfer:
			size_needed += nb_elements * sizeof(struct scsi_loader_data_transfer_element);
			break;

		case scsi_loader_element_type_import_export_element:
		case scsi_loader_element_type_medium_transport_element:
		case scsi_loader_element_type_storage_element:
			size_needed += nb_elements * sizeof(struct scsi_loader_import_export_element);
	}

	struct scsi_request_sense sense;
	struct {
		unsigned short first_element_address;
		unsigned short number_of_elements;
		unsigned char reserved;
		unsigned int byte_count_of_report:24;
	} __attribute__((packed)) * result = malloc(size_needed);
	struct {
		unsigned char operation_code;
		enum scsi_loader_element_type type:4;
		bool volume_tag:1;
		unsigned char reserved0:3;
		unsigned short starting_element_address;
		unsigned short number_of_elements;
		bool device_id:1;
		bool current_data:1;
		unsigned char reserved1:6;
		unsigned char allocation_length[3];
		unsigned char reserved2;
		unsigned char reserved3:7;
		bool serial_number_request:1;
	} __attribute__((packed)) command = {
		.operation_code = 0xB8,
		.type = type,
		.volume_tag = changer->barcode,
		.reserved0 = 0,
		.starting_element_address = htobe16(start_element),
		.number_of_elements = htobe16(nb_elements),
		.device_id = true,
		.current_data = false,
		.reserved1 = 0,
		.allocation_length = { (size_needed >> 16) & 0xFF, (size_needed >> 8) & 0xFF, size_needed & 0xFF, },
		.reserved2 = 0,
		.reserved3 = 0,
		.serial_number_request = false,
	};

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(result, 0, size_needed);

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = size_needed & 0xFFFFFFFF;
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = (unsigned char *) result;
	header.timeout = 1200000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	unsigned char * ptr;
	struct scsi_loader_element_status * element_header;
	do {
		int failed = ioctl(fd, SG_IO, &header);
		if (failed) {
			free(result);
			return;
		}

		result->number_of_elements = be16toh(result->number_of_elements);

		ptr = (unsigned char *) (result + 1);

		element_header = (struct scsi_loader_element_status *) (ptr);
		element_header->element_descriptor_length = be16toh(element_header->element_descriptor_length);
		element_header->byte_count_of_descriptor_data_available = be32toh(element_header->byte_count_of_descriptor_data_available) & 0xFFFFFF;
		ptr += sizeof(struct scsi_loader_element_status);

		if (element_header->type != type) {
			sleep(1);
			sochgr_scsi_changer_scsi_loader_ready(device);
		}
	} while (element_header->type != type);

	unsigned short i;
	for (i = 0; i < result->number_of_elements; i++) {
		struct so_slot * slot;
		switch (element_header->type) {
			case scsi_loader_element_type_data_transfer: {
					struct scsi_loader_data_transfer_element * data_transfer_element = (struct scsi_loader_data_transfer_element *) ptr;

					slot = slots + i;
					slot->full = data_transfer_element->full;

					if (data_transfer_element->full) {
						char volume_name[37];

						strncpy(volume_name, data_transfer_element->primary_volume_tag_information, 36);
						volume_name[36] = '\0';

						so_string_rtrim(volume_name, ' ');

						slot->volume_name = strdup(volume_name);
					}

					slot->is_ie_port = false;

					struct sochgr_scsi_changer_slot * sl = slot->data;
					sl->address = be16toh(data_transfer_element->element_address);
					sl->src_address = be16toh(data_transfer_element->source_storage_element_address);

					char dev_name[35];
					strncpy(dev_name, data_transfer_element->device_identifier_1, 34);
					dev_name[34] = '\0';

					if (available_drives != NULL && so_value_hashtable_has_key2(available_drives, dev_name)) {
						struct so_value * drive = so_value_hashtable_get2(available_drives, dev_name, false, false);
						struct so_drive * dr = changer->drives + i;

						dr->status = so_drive_status_unknown;
						dr->changer = changer;
						dr->slot = slot;
						dr->index = i;
						slot->drive = dr;

						struct so_value * vslot = NULL;
						so_value_unpack(drive, "{sssssssssbso}",
							"model", &dr->model,
							"vendor", &dr->vendor,
							"firmware revision", &dr->revision,
							"serial number", &dr->serial_number,
							"enable", &dr->enable,
							"slot", &vslot
						);

						if (vslot != NULL) {
							if (slot->volume_name != NULL)
								so_value_hashtable_put2(vslot, "volume name", so_value_new_string(slot->volume_name), true);
							else
								so_value_hashtable_put2(vslot, "volume name", so_value_new_null(), true);
							so_value_hashtable_put2(vslot, "ie port", so_value_new_boolean(slot->is_ie_port), true);
						}

						sochgr_scsi_changer_scsi_setup_drive(dr, drive);
					}

					break;
				}

			case scsi_loader_element_type_import_export_element: {
					struct scsi_loader_import_export_element * import_export_element = (struct scsi_loader_import_export_element *) ptr;

					slot = slots + i;
					slot->full = import_export_element->full;

					if (import_export_element->full) {
						char volume_name[37];

						strncpy(volume_name, import_export_element->primary_volume_tag_information, 36);
						volume_name[36] = '\0';

						so_string_rtrim(volume_name, ' ');

						slot->volume_name = strdup(volume_name);
					}

					slot->is_ie_port = true;

					struct sochgr_scsi_changer_slot * sl = slot->data;
					sl->address = be16toh(import_export_element->element_address);

					break;
				 }

			case scsi_loader_element_type_storage_element: {
					struct scsi_loader_storage_element * storage_element = (struct scsi_loader_storage_element *) ptr;

					slot = slots + i;
					slot->full = storage_element->full;

					if (storage_element->full) {
						char volume_name[37];

						strncpy(volume_name, storage_element->primary_volume_tag_information, 36);
						volume_name[36] = '\0';

						so_string_rtrim(volume_name, ' ');

						slot->volume_name = strdup(volume_name);
					}

					slot->is_ie_port = false;

					struct sochgr_scsi_changer_slot * sl = slot->data;
					sl->address = be16toh(storage_element->element_address);

					break;
				}

			default:
				break;
		}

		ptr += element_header->element_descriptor_length;
	}

	free(result);
}

