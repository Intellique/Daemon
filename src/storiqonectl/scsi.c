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

#define _GNU_SOURCE
// open, size_t
#include <sys/types.h>

// be*toh, htobe*
#include <endian.h>
// open
#include <fcntl.h>
// sg_io_hdr_t
#include <scsi/scsi.h>
// sg_io_hdr_t
#include <scsi/sg.h>
// malloc
#include <stdlib.h>
// memcpy, memmem, memset, strdup
#include <string.h>
// ioctl
#include <sys/ioctl.h>
// close
#include <unistd.h>

#include <libstoriqone/changer.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

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
	unsigned char enable_vital_product_data:1;
	unsigned char reserved0:4;
	unsigned char obsolete:3;
	unsigned char page_code;
	unsigned char reserved1;
	unsigned char allocation_length;
	unsigned char reserved2;
} __attribute__((packed));

struct scsi_loader_data_transfer_element {
	unsigned short element_address;
	unsigned char full:1;
	unsigned char reserved0:1;
	unsigned char execpt:1;
	unsigned char access:1;
	unsigned char reserved1:4;
	unsigned char reserved2;
	unsigned char additional_sense_code;
	unsigned char additional_sense_code_qualifier;
	unsigned char logical_unit_number:3;
	unsigned char reserved3:1;
	unsigned char logical_unit_valid:1;
	unsigned char id_valid:1;
	unsigned char reserved4:2;
	unsigned char scsi_bus_address;
	unsigned char reserved5;
	unsigned char reserved6:6;
	unsigned char invert:1;
	unsigned char source_valid:1;
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
	unsigned alternate_volume_tag:1;
	unsigned primary_volume_tag:1;
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
	unsigned char valid:1;							/* Byte 0 Bit 7 */
	unsigned char segment_number;					/* Byte 1 */
	unsigned char sense_key:4;						/* Byte 2 Bits 0-3 */
	unsigned char :1;								/* Byte 2 Bit 4 */
	unsigned char ili:1;							/* Byte 2 Bit 5 */
	unsigned char eom:1;							/* Byte 2 Bit 6 */
	unsigned char filemark:1;						/* Byte 2 Bit 7 */
	unsigned char information[4];					/* Bytes 3-6 */
	unsigned char additional_sense_length;			/* Byte 7 */
	unsigned char command_specific_information[4];	/* Bytes 8-11 */
	unsigned char additional_sense_code;			/* Byte 12 */
	unsigned char additional_sense_code_qualifier;	/* Byte 13 */
	unsigned char :8;								/* Byte 14 */
	unsigned char bit_pointer:3;					/* Byte 15 */
	unsigned char bpv:1;
	unsigned char :2;
	unsigned char command_data :1;
	unsigned char sksv:1;
	unsigned char field_data[2];					/* Byte 16,17 */
};


static int soctl_scsi_loader_ready(int fd);
static void soctl_scsi_loader_status_slot(int fd, struct so_changer * changer, struct so_value * available_drives, struct so_slot * slots, int start_element, int nb_elements, enum scsi_loader_element_type type);

int soctl_scsi_loaderinfo(const char * filename, struct so_changer * changer, struct so_value * available_drives) {
	int fd = open(filename, O_RDWR);
	if (fd < 0)
		return 1;

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
		return 2;
	}

	changer->model = strndup(result_inquiry.product_identification, 16);
	so_string_rtrim(changer->model, ' ');

	changer->vendor = strndup(result_inquiry.vendor_identification, 8);
	so_string_rtrim(changer->vendor, ' ');

	changer->revision = strndup(result_inquiry.product_revision_level, 4);
	so_string_rtrim(changer->revision, ' ');

	changer->barcode = result_inquiry.bar_code;


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

	if (status) {
		close(fd);
		return 3;
	}

	so_string_rtrim(result_serial_number.unit_serial_number, ' ');
	changer->serial_number = strdup(result_serial_number.unit_serial_number);

	unsigned char buffer[60];
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
		.allocation_length = sizeof(buffer),
		.reserved3 = 0,
	};

	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(&buffer, 0, sizeof(buffer));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(buffer);
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = buffer;
	header.timeout = 1200000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int failed = ioctl(fd, SG_IO, &header);
	if (failed) {
		close(fd);
		return 4;
	}


	struct {
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
	} __attribute__((packed)) * result = memmem(buffer, sizeof(buffer), "\x9d\x12", 2);

	if (result == NULL)
		return 5;

	result->medium_transport_element_address = be16toh(result->medium_transport_element_address);
	result->number_of_medium_transport_elements = be16toh(result->number_of_medium_transport_elements);
	result->first_storage_element_address = be16toh(result->first_storage_element_address);
	result->number_of_storage_elements = be16toh(result->number_of_storage_elements);
	result->first_import_export_element_address = be16toh(result->first_import_export_element_address);
	result->number_of_import_export_elements = be16toh(result->number_of_import_export_elements);
	result->first_data_transfer_element_address = be16toh(result->first_data_transfer_element_address);
	result->number_of_data_transfer_elements = be16toh(result->number_of_data_transfer_elements);

	changer->nb_drives = result->number_of_data_transfer_elements;
	changer->drives = calloc(changer->nb_drives, sizeof(struct so_drive));

	changer->nb_slots = result->number_of_data_transfer_elements + result->number_of_storage_elements + result->number_of_import_export_elements;
	changer->slots = calloc(changer->nb_slots, sizeof(struct so_slot));

	unsigned int i;
	for (i = 0; i < changer->nb_slots; i++) {
		struct so_slot * sl = changer->slots + i;
		sl->changer = changer;
		sl->index = i;

		if (i < changer->nb_drives)
			sl->drive = changer->drives + i;
	}

	soctl_scsi_loader_status_slot(fd, changer, available_drives, changer->slots, result->first_data_transfer_element_address, result->number_of_data_transfer_elements, scsi_loader_element_type_data_transfer);
	soctl_scsi_loader_status_slot(fd, changer, available_drives, changer->slots + changer->nb_drives, result->first_storage_element_address, result->number_of_storage_elements, scsi_loader_element_type_storage_element);
	if (result->number_of_import_export_elements > 0)
		soctl_scsi_loader_status_slot(fd, changer, available_drives, changer->slots + (result->number_of_data_transfer_elements + result->number_of_storage_elements), result->first_import_export_element_address, result->number_of_import_export_elements, scsi_loader_element_type_import_export_element);

	return 0;
}

static int soctl_scsi_loader_ready(int fd) {
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

	return sense.sense_key;
}

static void soctl_scsi_loader_status_slot(int fd, struct so_changer * changer, struct so_value * available_drives, struct so_slot * slots, int start_element, int nb_elements, enum scsi_loader_element_type type) {
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
		.allocation_length = { size_needed >> 16, size_needed >> 8, size_needed & 0xFF, },
		.reserved2 = 0,
		.reserved3 = 0,
		.serial_number_request = 0,
	};

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(result, 0, size_needed);

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = size_needed;
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
		element_header->byte_count_of_descriptor_data_available = be32toh(element_header->byte_count_of_descriptor_data_available);
		ptr += sizeof(struct scsi_loader_element_status);

		if (element_header->type != type) {
			sleep(1);
			soctl_scsi_loader_ready(fd);
		}
	} while (element_header->type != type);

	unsigned short i;
	for (i = 0; i < result->number_of_elements; i++) {
		struct so_slot * slot;
		switch (element_header->type) {
			case scsi_loader_element_type_all_elements:
				break;

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

					char drive_id[35];
					strncpy(drive_id, data_transfer_element->device_identifier_1, 34);
					drive_id[34] = '\0';

					struct so_value * val_drive = so_value_hashtable_get2(available_drives, drive_id, false, true);
					struct so_drive * l_drive = so_value_custom_get(val_drive);
					struct so_drive * c_drive = changer->drives + i;

					if (l_drive != NULL) {
						memcpy(c_drive, l_drive, sizeof(struct so_drive));
						bzero(l_drive, sizeof(struct so_drive));
						c_drive->enable = true;
					} else
						c_drive->enable = false;

					c_drive->changer = changer;
					c_drive->slot = slot;

					so_value_free(val_drive);
				}
				break;

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
				 }
				 break;

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
				}
				break;

			default:
				break;
		}

		ptr += element_header->element_descriptor_length;
	}

	free(result);
}

int soctl_scsi_tapeinfo(const char * filename, struct so_drive * drive) {
	int fd = open(filename, O_RDWR);
	if (fd < 0)
		return 1;

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
		return 2;
	}

	drive->vendor = strndup(result_inquiry.vendor_identification, 7);
	so_string_rtrim(drive->vendor, ' ');

	drive->model = strndup(result_inquiry.product_identification, 15);
	so_string_rtrim(drive->model, ' ');

	drive->revision = strndup(result_inquiry.product_revision_level, 4);
	so_string_rtrim(drive->revision, ' ');

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

	if (status)
		return 3;

	result_serial_number.unit_serial_number[11] = '\0';
	so_string_rtrim(result_serial_number.unit_serial_number, ' ');
	drive->serial_number = strdup(result_serial_number.unit_serial_number);

	return 0;
}
