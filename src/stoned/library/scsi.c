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
*  Last modified: Tue, 12 Jun 2012 17:27:54 +0200                         *
\*************************************************************************/

// htobe16
#include <endian.h>
// ssize_t
#include <sys/types.h>

// sg_io_hdr_t
#include <scsi/scsi.h>
// sg_io_hdr_t
#include <scsi/sg.h>
// calloc, malloc
#include <stdlib.h>
// memset, strdup, strncpy
#include <string.h>
// ioctl
#include <sys/ioctl.h>

#include "scsi.h"


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

enum scsi_loader_element_type {
    scsi_loader_element_type_all_elements             = 0x0,
    scsi_loader_element_type_medium_transport_element = 0x1,
    scsi_loader_element_type_storage_element          = 0x2,
    scsi_loader_element_type_import_export_element    = 0x3,
    scsi_loader_element_type_data_transfer            = 0x4,
};

struct scsi_loader_element_status {
    enum scsi_loader_element_type type:8;
    unsigned char reserved0:6;
    unsigned alternate_volume_tag:1;
    unsigned primary_volume_tag:1;
    unsigned short element_descriptor_length;
    unsigned char reserved1;
    unsigned int byte_count_of_descriptor_data_available:24;
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
    unsigned char device_identifier_1[34];
    unsigned char code_set_2:4;
    unsigned char reserved10:4;
    unsigned char identifier_type_2:4;
    unsigned char reserved11:4;
} __attribute__((packed));

struct scsi_loader_import_export_element {
    unsigned short element_address;
    unsigned char full:1;
    unsigned char import_export:1;
    unsigned char execpt:1;
    unsigned char access:1;
    unsigned char export_enable:1;
    unsigned char import_enable:1;
    unsigned char reserved0:2;
    unsigned char reserved1;
    unsigned char additional_sense_code;
    unsigned char additional_sense_code_qualifier;
    unsigned char reserved2[3];
    unsigned char reserved3:6;
    unsigned char invert:1;
    unsigned char source_valid:1;
    unsigned short source_storage_element_address;
    char primary_volume_tag_information[36];
    unsigned char reserved4[4];
} __attribute__((packed));

struct scsi_loader_medium_transport_element {
    unsigned short element_address;
    unsigned char full:1;
    unsigned char reserved0:1;
    unsigned char execpt:1;
    unsigned char reserved1:5;
    unsigned char reserved2;
    unsigned char additional_sense_code;
    unsigned char additional_sense_code_qualifier;
    unsigned char reserved3[3];
    unsigned char reserved4:6;
    unsigned char invert:1;
    unsigned char source_valid:1;
    unsigned short source_storage_element_address;
    unsigned char primary_volume_tag_information[36];
    unsigned char reserved5[4];
} __attribute__((packed));

struct scsi_loader_storage_element {
    unsigned short element_address;
    unsigned char full:1;
    unsigned char reserved0:1;
    unsigned char execpt:1;
    unsigned char access:1;
    unsigned char reserved1:4;
    unsigned char reserved2;
    unsigned char additional_sense_code;
    unsigned char additional_sense_code_qualifier;
    unsigned char reserved3[3];
    unsigned char reserved4:6;
    unsigned char invert:1;
    unsigned char source_valid:1;
    unsigned short source_storage_element_address;
    char primary_volume_tag_information[36];
    unsigned char reserved5[4];
} __attribute__((packed));

enum scsi_mam_attribute {
	scsi_mam_remaining_capacity  = 0x0000,
	scsi_mam_maximum_capacity    = 0x0001,
	scsi_mam_load_count          = 0x0003,
	scsi_mam_mam_space_remaining = 0x0004,

	scsi_mam_device_at_last_load           = 0x020A,
	scsi_mam_device_at_last_load_2         = 0x020B,
	scsi_mam_device_at_last_load_3         = 0x020C,
	scsi_mam_device_at_last_load_4         = 0x020D,
	scsi_mam_total_written_in_medium_life  = 0x0220,
	scsi_mam_total_read_in_medium_life     = 0x0221,
	scsi_mam_total_written_in_current_load = 0x0222,
	scsi_mam_total_read_current_load       = 0x0223,

	scsi_mam_medium_manufacturer      = 0x0400,
	scsi_mam_medium_serial_number     = 0x0401,
	scsi_mam_medium_manufacturer_date = 0x0406,
	scsi_mam_mam_capacity             = 0x0407,
	scsi_mam_medium_type              = 0x0408,
	scsi_mam_medium_type_information  = 0x0409,

	scsi_mam_application_vendor           = 0x0800,
	scsi_mam_application_name             = 0x0801,
	scsi_mam_application_version          = 0x0802,
	scsi_mam_user_medium_text_label       = 0x0803,
	scsi_mam_date_and_time_last_written   = 0x0804,
	scsi_mam_text_localization_identifier = 0x0805,
	scsi_mam_barcode                      = 0x0806,
	scsi_mam_owning_host_textual_name     = 0x0807,
	scsi_mam_media_pool                   = 0x0808,

	scsi_mam_unique_cardtrige_identity = 0x1000,
};

struct scsi_request_sense {
	unsigned char error_code:7;						/* Byte 0 Bits 0-6 */
	char valid:1;									/* Byte 0 Bit 7 */
	unsigned char segment_number;					/* Byte 1 */
	unsigned char sense_key:4;						/* Byte 2 Bits 0-3 */
	unsigned char :1;								/* Byte 2 Bit 4 */
	char ili:1;										/* Byte 2 Bit 5 */
	char eom:1;										/* Byte 2 Bit 6 */
	char filemark:1;								/* Byte 2 Bit 7 */
	unsigned char information[4];					/* Bytes 3-6 */
	unsigned char additional_sense_length;			/* Byte 7 */
	unsigned char command_specific_information[4];	/* Bytes 8-11 */
	unsigned char additional_sense_code;			/* Byte 12 */
	unsigned char additional_sense_code_qualifier;	/* Byte 13 */
	unsigned char :8;								/* Byte 14 */
	unsigned char bit_pointer:3;					/* Byte 15 */
	char bpv:1;
	unsigned char :2;
	char command_data :1;
	char sksv:1;
	unsigned char field_data[2];					/* Byte 16,17 */
};

static void st_scsi_loader_status_update_slot(int fd, struct st_changer * changer, struct st_slot * slot_base, int start_element, int nb_elements, enum scsi_loader_element_type type);


void st_scsi_loader_info(int fd, struct st_changer * changer) {
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

	if (ioctl(fd, SG_IO, &header))
		return;

	ssize_t length;
	changer->vendor = malloc(9);
	strncpy(changer->vendor, result_inquiry.vendor_identification, 8);
	changer->vendor[8] = '\0';
	for (length = 7; length >= 0 && changer->vendor[length] == ' '; length--)
		changer->vendor[length] = '\0';

	changer->model = malloc(17);
	strncpy(changer->model, result_inquiry.product_identification, 16);
	changer->model[16] = '\0';
	for (length = 15; length >= 0 && changer->model[length] == ' '; length--)
		changer->model[length] = '\0';

	changer->revision = malloc(5);
	strncpy(changer->revision, result_inquiry.product_revision_level, 4);
	changer->revision[4] = '\0';
	for (length = 3; length >= 0 && changer->revision[length] == ' '; length--)
		changer->revision[length] = '\0';

	changer->barcode = 0;
	if (result_inquiry.bar_code)
		changer->barcode = 1;


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

	if (ioctl(fd, SG_IO, &header))
		return;

	changer->serial_number = malloc(13);
	strncpy(changer->serial_number, result_serial_number.unit_serial_number, 12);
	changer->serial_number[12] = '\0';
}

int st_scsi_loader_medium_removal(int fd, int allow) {
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
	header.dxferp = 0;
	header.timeout = 1200000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int failed = ioctl(fd, SG_IO, &header);

	return failed;
}

int st_scsi_loader_move(int fd, struct st_changer * ch, struct st_slot * from, struct st_slot * to) {
	struct scsi_request_sense sense;
	struct {
		unsigned char operation_code;
		unsigned char reserved0;
		unsigned short transport_element_address;
		unsigned short source_address;
		unsigned short destination_address;
		unsigned char reserved1[2];
		unsigned char invert:1;
		unsigned char reserved2:7;
		unsigned char reserved3;
	} __attribute__((packed)) command = {
		.operation_code = 0xA5,
		.reserved0 = 0,
		.transport_element_address = htobe16(ch->transport_address),
		.source_address = htobe16(from->address),
		.destination_address = htobe16(to->address),
		.reserved1 = { 0, 0 },
		.invert = 0,
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
	header.dxferp = 0;
	header.timeout = 1200000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	ioctl(fd, SG_IO, &header);

	return sense.sense_key;
}

int st_scsi_loader_ready(int fd) {
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
	header.dxferp = 0;
	header.timeout = 1200000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	ioctl(fd, SG_IO, &header);

	return sense.sense_key;
}

void st_scsi_loader_status_new(int fd, struct st_changer * changer) {
	struct scsi_request_sense sense;
	struct {
		unsigned char mode_data_length;
		unsigned char reserved0[3];

		unsigned char page_code:6;
		unsigned char reserved1:1;
		unsigned char page_saved:1;
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
		unsigned char disable_block_descriptors:1;
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
		.disable_block_descriptors = 0,
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

	changer->nb_slots = result.number_of_storage_elements;
	unsigned short nb_io_slots = result.number_of_import_export_elements;
	if (nb_io_slots > 0)
		changer->nb_slots += nb_io_slots;
	changer->nb_slots += changer->nb_drives;

	if (changer->nb_slots > 0)
		changer->slots = calloc(sizeof(struct st_slot), changer->nb_slots);

	unsigned int i;
	for (i = 0; i < changer->nb_slots; i++) {
		struct st_slot * slot = changer->slots + i;

		slot->id = -1;
		slot->changer = changer;

		slot->drive = 0;
		if (i < changer->nb_drives) {
			slot->drive = changer->drives + i;
			changer->drives[i].slot = slot;
		}

		slot->tape = 0;

		slot->is_import_export_slot = 0;
	}

    st_scsi_loader_status_update_slot(fd, changer, changer->slots + changer->nb_drives, result.first_storage_element_address, result.number_of_storage_elements, scsi_loader_element_type_storage_element);
    st_scsi_loader_status_update_slot(fd, changer, changer->slots, result.first_data_transfer_element_address, result.number_of_data_transfer_elements, scsi_loader_element_type_data_transfer);
    if (result.number_of_import_export_elements > 0)
        st_scsi_loader_status_update_slot(fd, changer, changer->slots + result.number_of_medium_transport_elements + result.number_of_storage_elements, result.first_import_export_element_address, result.number_of_import_export_elements, scsi_loader_element_type_import_export_element);

    changer->transport_address = result.medium_transport_element_address;
}

void st_scsi_loader_status_update_slot(int fd, struct st_changer * changer, struct st_slot * slot_base, int start_element, int nb_elements, enum scsi_loader_element_type type) {
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
		unsigned char volume_tag:1;
		unsigned char reserved0:3;
		unsigned short starting_element_address;
		unsigned short number_of_elements;
		unsigned char device_id:1;
		unsigned char current_data:1;
		unsigned char reserved1:6;
		unsigned char allocation_length[3];
		unsigned char reserved2;
		unsigned char reserved3:7;
		unsigned char serial_number_request:1;
	} __attribute__((packed)) command = {
		.operation_code = 0xB8,
		.type = type,
		.volume_tag = changer->barcode ? 1 : 0,
		.reserved0 = 0,
		.starting_element_address = htobe16(start_element),
		.number_of_elements = htobe16(nb_elements),
		.device_id = 0,
		.current_data = 0,
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

	int failed = ioctl(fd, SG_IO, &header);
	if (failed) {
		free(result);
		return;
	}

	result->number_of_elements = be16toh(result->number_of_elements);

	unsigned char * ptr = (unsigned char *) (result + 1);
	unsigned short i;

	struct scsi_loader_element_status * element_header = (struct scsi_loader_element_status *) (ptr);
	element_header->element_descriptor_length = be16toh(element_header->element_descriptor_length);
	element_header->byte_count_of_descriptor_data_available = be32toh(element_header->byte_count_of_descriptor_data_available);
	ptr += sizeof(struct scsi_loader_element_status);

	for (i = 0; i < result->number_of_elements; i++) {
		struct st_slot * slot;
		switch (element_header->type) {
			case scsi_loader_element_type_all_elements:
				break;

			case scsi_loader_element_type_data_transfer: {
					struct scsi_loader_data_transfer_element * data_transfer_element = (struct scsi_loader_data_transfer_element *) ptr;
					data_transfer_element->element_address = be16toh(data_transfer_element->element_address);
					data_transfer_element->source_storage_element_address = be16toh(data_transfer_element->source_storage_element_address);

					slot = slot_base + i;
					slot->full = data_transfer_element->full;
					if (slot->full) {
						strncpy(slot->volume_name, data_transfer_element->primary_volume_tag_information, 36);
						slot->volume_name[36] = '\0';

						int j;
						for (j = 35; j >= 0 && slot->volume_name[j] == ' '; j--)
							slot->volume_name[j] = '\0';
					} else
						*slot->volume_name = '\0';
					slot->is_import_export_slot = 1;
					slot->address = data_transfer_element->element_address;
					slot->src_address = 0;
				}
				break;

			case scsi_loader_element_type_import_export_element: {
					struct scsi_loader_import_export_element * import_export_element = (struct scsi_loader_import_export_element *) ptr;
					import_export_element->element_address = be16toh(import_export_element->element_address);
					import_export_element->source_storage_element_address = be16toh(import_export_element->source_storage_element_address);

					slot = slot_base + i;
					slot->full = import_export_element->full;
					if (slot->full) {
						strncpy(slot->volume_name, import_export_element->primary_volume_tag_information, 36);
						slot->volume_name[36] = '\0';

						int j;
						for (j = 35; j >= 0 && slot->volume_name[j] == ' '; j--)
							slot->volume_name[j] = '\0';
					} else
						*slot->volume_name = '\0';
					slot->is_import_export_slot = 1;
					slot->address = import_export_element->element_address;
					slot->src_address = 0;
				}
				break;

			case scsi_loader_element_type_medium_transport_element: {
					struct scsi_loader_medium_transport_element * transport_element = (struct scsi_loader_medium_transport_element *) ptr;
					transport_element->element_address = be16toh(transport_element->element_address);
					transport_element->source_storage_element_address = be16toh(transport_element->source_storage_element_address);
				}
				break;

			case scsi_loader_element_type_storage_element: {
					struct scsi_loader_storage_element * storage_element = (struct scsi_loader_storage_element *) ptr;
					storage_element->element_address = be16toh(storage_element->element_address);
					storage_element->source_storage_element_address = be16toh(storage_element->source_storage_element_address);

					slot = slot_base + i;
					slot->full = storage_element->full;

					if (slot->full) {
						strncpy(slot->volume_name, storage_element->primary_volume_tag_information, 36);
						slot->volume_name[36] = '\0';

						int j;
						for (j = 35; j >= 0 && slot->volume_name[j] == ' '; j--)
							slot->volume_name[j] = '\0';
					} else
						*slot->volume_name = '\0';
					slot->is_import_export_slot = 0;
					slot->address = storage_element->element_address;
					slot->src_address = 0;
				}
				break;
		}

		ptr += element_header->element_descriptor_length;
	}

	free(result);
}


void st_scsi_tape_info(int fd, struct st_drive * drive) {
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

	if (ioctl(fd, SG_IO, &header))
		return;

	ssize_t length;
	drive->vendor = malloc(9);
	strncpy(drive->vendor, result_inquiry.vendor_identification, 8);
	drive->vendor[8] = '\0';
	for (length = 7; length >= 0 && drive->vendor[length] == ' '; length--)
		drive->vendor[length] = '\0';

	drive->model = malloc(17);
	strncpy(drive->model, result_inquiry.product_identification, 16);
	drive->model[16] = '\0';
	for (length = 15; length >= 0 && drive->model[length] == ' '; length--)
		drive->model[length] = '\0';

	drive->revision = malloc(5);
	strncpy(drive->revision, result_inquiry.product_revision_level, 4);
	drive->revision[4] = '\0';
	for (length = 3; length >= 0 && drive->revision[length] == ' '; length--)
		drive->revision[length] = '\0';

	struct {
		unsigned char peripheral_device_type:5;
		unsigned char peripheral_device_qualifier:3;
		unsigned char page_code;
		unsigned char reserved;
		unsigned char page_length;
		char unit_serial_number[10];
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

	if (ioctl(fd, SG_IO, &header))
		return;

	drive->serial_number = malloc(13);
	strncpy(drive->serial_number, result_serial_number.unit_serial_number, 12);
	drive->serial_number[12] = '\0';
}

int st_scsi_tape_locate(int fd, off_t position) {
	struct {
		unsigned char op_code;
		unsigned char immed:1;
		unsigned char cp:1;
		unsigned char bt:1;
		unsigned char reserved0:2;
		unsigned char obsolete:3;
		unsigned char reserved1;
		int block_address; // should be a bigger endian integer
		unsigned char reserved2;
		unsigned char partition;
		unsigned char control;
	} __attribute__((packed)) command = {
		.op_code = 0x2B,
		.cp = 0,
		.bt = 0,
		.block_address = htobe32(position),
		.partition = 0,
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
	header.dxferp = 0;
	header.timeout = 60000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);
	if (status)
		return 1;

	return 0;
}

int st_scsi_tape_position(int fd, struct st_tape * tape) {
	struct {
		unsigned char op_code;
		unsigned char service_action:5;
		unsigned char obsolete:3;
		unsigned char reserved[5];
		unsigned short parameter_length; // should be a bigger endian integer
		unsigned char control;
	} __attribute__((packed)) command = {
		.op_code = 0x34,
		.service_action = 0,
		.parameter_length = 0,
	};

	struct {
		unsigned char reserved0:1;
		unsigned char perr:1;
		unsigned char block_position_unknown:1;
		unsigned char reserved1:1;
		unsigned char bycu:1;
		unsigned char bcu:1;
		unsigned char end_of_partition:1;
		unsigned char begginning_of_partition:1;
		unsigned char partition_number;
		unsigned char reserved2[2];
		unsigned int first_block_location;
		unsigned int last_block_location;
		unsigned char reserved3;
		unsigned int number_of_blocks_in_buffer;
		unsigned int number_of_bytes_in_buffer;
	} __attribute__((packed)) result;

	struct scsi_request_sense sense;
	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(&result, 0, sizeof(result));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(result);;
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = (unsigned char *) &result;
	header.timeout = 60000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);
	if (status)
		return 1;

	if (result.block_position_unknown)
		return -1;

	result.first_block_location = be32toh(result.first_block_location);
	result.last_block_location = be32toh(result.last_block_location);
	result.number_of_blocks_in_buffer = be32toh(result.number_of_blocks_in_buffer);
	result.number_of_bytes_in_buffer = be32toh(result.number_of_bytes_in_buffer);

	tape->end_position = result.first_block_location;

	return 0;
}

int st_scsi_tape_read_position(int fd, off_t * position) {
	if (!position)
		return 1;

	struct {
		unsigned char operation_code;
		unsigned char service_action:5;
		unsigned char obsolete:3;
		unsigned char reserved[5];
		unsigned short parameter_length;	// must be a bigger endian integer
		unsigned char control;
	} __attribute__((packed)) command = {
		.operation_code = 0x34,
		.service_action = 0,
		.parameter_length = 0,
		.control = 0,
	};

	struct {
		unsigned char reserved0:1;
		unsigned char perr:1;
		unsigned char block_position_unknown:1;
		unsigned char reserved1:1;
		unsigned char byte_count_unknown:1;
		unsigned char block_count_unknown:1;
		unsigned char end_of_partition:1;
		unsigned char begin_of_partition:1;
		unsigned char partition_number;
		unsigned char reserved2[2];
		unsigned int first_block_location;
		unsigned int last_block_location;
		unsigned char reserved3;
		unsigned int number_of_blocks_in_buffer;
		unsigned int number_of_bytes_in_buffer;
	} __attribute__((packed)) result;

	struct scsi_request_sense sense;
	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(result);
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = &result;
	header.timeout = 60000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);
	if (status)
		return 2;

	*position = be32toh(result.first_block_location);

	return 0;
}

int st_scsi_tape_read_mam(int fd, struct st_tape * tape) {
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
		.operation_code = 0x8C,
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

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(buffer);
	header.cmdp = (unsigned char *) &command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = buffer;
	header.timeout = 2000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	tape->mam_ok = 0;

	int status = ioctl(fd, SG_IO, &header);
	if (status)
		return status;

	struct scsi_mam {
		enum scsi_mam_attribute attribute_identifier:16;
		unsigned char format:2;
		unsigned char reserved:5;
		unsigned char read_only:1;
		unsigned short attribute_length;
		union {
			unsigned char int8;
			unsigned short be16;
			unsigned long long be64;
			char text[160];
		} attribute_value;
	} __attribute__((packed));

	unsigned int data_available = be32toh(*(unsigned int *) buffer);
	unsigned char * ptr = buffer + 4;

	for (ptr = buffer + 4; ptr < buffer + data_available;) {
		struct scsi_mam * attr = (struct scsi_mam *) ptr;
		attr->attribute_identifier = be16toh(attr->attribute_identifier);
		attr->attribute_length = be16toh(attr->attribute_length);

		ptr += attr->attribute_length + 5;

		if (attr->attribute_length == 0)
			continue;

		char * space;
		switch (attr->attribute_identifier) {
			case scsi_mam_remaining_capacity:
				tape->available_block = be64toh(attr->attribute_value.be64);
				tape->available_block <<= 10;
				tape->available_block /= (tape->block_size >> 10);
				break;

			case scsi_mam_load_count:
				tape->load_count = be64toh(attr->attribute_value.be64);
				break;

			case scsi_mam_medium_serial_number:
				strncpy(tape->medium_serial_number, attr->attribute_value.text, 32);
				space = strchr(tape->medium_serial_number, ' ');
				if (space)
					*space = '\0';
				break;

			default:
				break;
		}
	}

	tape->mam_ok = 1;

	return 0;
}

int st_scsi_tape_size_available(int fd, struct st_tape * tape) {
	struct {
		unsigned char page_code:6;
		unsigned char reserved0:2;
		unsigned char reserved1;
		unsigned short page_length;
		struct {
			unsigned short parameter_code;
			unsigned char lp:1;
			unsigned char lbin:1;
			unsigned char tmc:2;
			unsigned char etc:1;
			unsigned char tsd:1;
			unsigned char ds:1;
			unsigned char du:1;
			unsigned char parameter_length;
			unsigned int value;
		} parameters[4];
	} __attribute__((packed)) result;

	struct {
		unsigned char op_code;
		unsigned char sp:1;
		unsigned char ppc:1;
		unsigned char reserved0:3;
		unsigned char obsolete:3;
		unsigned char page_code:5;
		enum {
			log_sense_page_control_maximum_value = 0x0,
			log_sense_page_control_current_value = 0x1,
			log_sense_page_control_maximum_value2 = 0x2,
			log_sense_page_control_power_on_value = 0x3,
		} page_control:2;
		unsigned char reserved1[2];
		unsigned short parameter_pointer;
		unsigned short allocation_length;
		unsigned char control;
	} __attribute__((packed)) command = {
		.op_code = 0x4D,
		.sp = 0,
		.ppc = 0,
		.page_code = 0x11,
		.page_control = log_sense_page_control_current_value,
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
	header.timeout = 60000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);
	if (status || result.page_code != 0x31)
		return 1;

	result.page_length = be16toh(result.page_length);
	unsigned short i;
	for (i = 0; i < 4; i++) {
		result.parameters[i].parameter_code = be16toh(result.parameters[i].parameter_code);
		result.parameters[i].value = be32toh(result.parameters[i].value);
	}

	tape->available_block = result.parameters[0].value << 10;
	tape->available_block /= (tape->block_size >> 10);
	tape->available_block -= tape->end_position;

	return 0;
}

