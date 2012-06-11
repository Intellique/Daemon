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
*  Last modified: Mon, 11 Jun 2012 23:31:28 +0200                         *
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

typedef struct Inquiry {
	unsigned char PeripheralDeviceType:5; /* Byte 0 Bits 0-4 */
	unsigned char PeripheralQualifier:3;  /* Byte 0 Bits 5-7 */
	unsigned char DeviceTypeModifier:7;   /* Byte 1 Bits 0-6 */
	char RMB:1;                           /* Byte 1 Bit 7 */
	unsigned char ANSI_ApprovedVersion:3; /* Byte 2 Bits 0-2 */
	unsigned char ECMA_Version:3;         /* Byte 2 Bits 3-5 */
	unsigned char ISO_Version:2;          /* Byte 2 Bits 6-7 */
	unsigned char ResponseDataFormat:4;   /* Byte 3 Bits 0-3 */
	unsigned char :2;                     /* Byte 3 Bits 4-5 */
	char TrmIOP:1;                        /* Byte 3 Bit 6 */
	char AENC:1;                          /* Byte 3 Bit 7 */
	unsigned char AdditionalLength;       /* Byte 4 */
	unsigned char :8;                     /* Byte 5 */
	char ADDR16:1;                        /* Byte 6 bit 0 */
	char Obs6_1:1;                        /* Byte 6 bit 1 */
	char Obs6_2:1;/* obsolete */          /* Byte 6 bit 2 */
	char MChngr:1;/* Media Changer */     /* Byte 6 bit 3 */
	char MultiP:1;                        /* Byte 6 bit 4 */
	char VS:1;                            /* Byte 6 bit 5 */
	char EncServ:1;                       /* Byte 6 bit 6 */
	char BQue:1;                          /* Byte 6 bit 7 */
	char SftRe:1;                         /* Byte 7 Bit 0 */
	char CmdQue:1;                        /* Byte 7 Bit 1 */
char :1;                              /* Byte 7 Bit 2 */
	  char Linked:1;                        /* Byte 7 Bit 3 */
	  char Sync:1;                          /* Byte 7 Bit 4 */
	  char WBus16:1;                        /* Byte 7 Bit 5 */
	  char WBus32:1;                        /* Byte 7 Bit 6 */
	  char RelAdr:1;                        /* Byte 7 Bit 7 */
	  unsigned char VendorIdentification[8];      /* Bytes 8-15 */
	  unsigned char ProductIdentification[16];    /* Bytes 16-31 */
	  unsigned char ProductRevisionLevel[4];      /* Bytes 32-35 */
	  unsigned char FullProductRevisionLevel[19]; /* bytes 36-54 */
	  unsigned char VendorFlags;                  /* byte 55 */
} Inquiry_T;

typedef struct RequestSense {
	unsigned char ErrorCode:7;				/* Byte 0 Bits 0-6 */
	char Valid:1;					/* Byte 0 Bit 7 */
	unsigned char SegmentNumber;				/* Byte 1 */
	unsigned char SenseKey:4;				/* Byte 2 Bits 0-3 */
	unsigned char :1;					/* Byte 2 Bit 4 */
	char ILI:1;					/* Byte 2 Bit 5 */
	char EOM:1;					/* Byte 2 Bit 6 */
	char Filemark:1;					/* Byte 2 Bit 7 */
	unsigned char Information[4];				/* Bytes 3-6 */
	unsigned char AdditionalSenseLength;			/* Byte 7 */
	unsigned char CommandSpecificInformation[4];		/* Bytes 8-11 */
	unsigned char AdditionalSenseCode;			/* Byte 12 */
	unsigned char AdditionalSenseCodeQualifier;		/* Byte 13 */
	unsigned char :8;					/* Byte 14 */
	unsigned char BitPointer:3;                           /* Byte 15 */
	char BPV:1;
	unsigned char :2;
	char CommandData :1;
	char SKSV:1;      
	unsigned char FieldData[2];       	 		/* Byte 16,17 */
} RequestSense_T;


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



int st_scsi_tape_locate(int fd, off_t position) {
	RequestSense_T sense;

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

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
	RequestSense_T sense;
	unsigned char buffer[20];
	bzero(buffer, 20);

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

	unsigned char com[10] = { 0x34, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));
	memset(buffer, 0, 20);

	header.interface_id = 'S';
	header.cmd_len = sizeof(com);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = 20;
	header.cmdp = com;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = buffer;
	header.timeout = 60000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);
	if (status)
		return 1;

	if (buffer[0] & 0x4)
		return -1;

	tape->end_position = (((unsigned int) buffer[4] << 24) + ((unsigned int) buffer[5] << 16) + ((unsigned int) buffer[6] <<  8) + buffer[7]);

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

	RequestSense_T sense;
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
	RequestSense_T sense;
	unsigned char buffer[1024];
	unsigned char com[16] = { 0x8C, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (sizeof(buffer) >> 8) & 0xFF, sizeof(buffer) & 0xFF, 0, 0 };

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmd_len = sizeof(com);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(buffer);
	header.cmdp = com;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = buffer;
	header.timeout = 2000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	tape->mam_ok = 0;

	int status = ioctl(fd, SG_IO, &header);
	if (status)
		return status;

	unsigned int data_available = ((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]) - 4;
	unsigned char * ptr = buffer + 4;

	for (ptr = buffer + 4; ptr < buffer + data_available;) {
		enum scsi_mam_attribute attr_id = (ptr[0] << 8) | ptr[1];
		// unsigned char read_only = ptr[2] & 0x80;
		// unsigned char format = ptr[2] & 0x3;
		unsigned short attr_length = (ptr[3] << 8) | ptr[4];

		unsigned char * attr = ptr + 5;
		ptr += 5 + attr_length;

		if (attr_length == 0)
			continue;

		unsigned int i;
		char * space;
		switch (attr_id) {
			case scsi_mam_remaining_capacity:
				tape->available_block = 0;
				for (i = 0; i < attr_length; i++) {
					tape->available_block <<= 8;
					tape->available_block += (unsigned char) attr[i];
				}
				tape->available_block <<= 10;
				tape->available_block /= (tape->block_size >> 10);
				break;

			case scsi_mam_load_count:
				tape->load_count = 0;
				for (i = 0; i < attr_length; i++) {
					tape->load_count <<= 8;
					tape->load_count += (unsigned char) attr[i];
				}
				break;

			case scsi_mam_medium_serial_number:
				strncpy(tape->medium_serial_number, (char *) attr, 32);
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
	RequestSense_T sense;
	unsigned char buffer[2048];
	bzero(buffer, 2048);

	struct {
		unsigned char op_code;
		unsigned char sp:1;
		unsigned char ppc:1;
		unsigned char reserved0:3;
		unsigned char obsolete:3;
		unsigned char page_code:5;
		unsigned char pc:2;
		unsigned char reserved1[2];
		unsigned short parameter_pointer;
		unsigned short allocation_length;
		unsigned char control;
	} __attribute__((packed)) command = {
		.op_code = 0x4D,
		.sp = 0,
		.ppc = 0,
		.page_code = 0x11,
		.pc = 1,
		.parameter_pointer = 0,
		.allocation_length = htobe16(2048),
		.control = 0,
	};

	// unsigned char com1[10] = { 0x4D, 0, 0x31, 0, 0, 0, 0, 2048 >> 8 & 0xFF, 2048 & 0xFF, 0 };

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
	header.timeout = 60000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);
	if (status || ((buffer[0] & 0x3F) != 0x31))
		return 1;

	if (buffer[8] == buffer[24] && buffer[9] == buffer[25] && buffer[10] == buffer[26] && buffer[11] == buffer[27] && tape->end_position > 0)
		return 0;

	tape->available_block = ((unsigned int) buffer[8] << 24) + ((unsigned int) buffer[9] << 16) + ((unsigned int) buffer[10] <<  8) + buffer[11];
	// check for buggy tape drive, they report always the same value for available size and total size
	if (buffer[8] == buffer[24] && buffer[9] == buffer[25] && buffer[10] == buffer[26] && buffer[11] == buffer[27] && tape->end_position > 0) {
		// update available only for buggy tape drive
		// tape->available_block is in MBytes
		tape->available_block <<= 10;
		tape->available_block /= (tape->block_size >> 10);
		tape->available_block -= tape->end_position;
	} else {
		// tape->available_block is in KBytes
		tape->available_block /= (tape->block_size >> 10);
	}

	return 0;
}

void st_scsi_tapeinfo(int fd, struct st_drive * drive) {
	Inquiry_T inq;
	RequestSense_T sense;

	unsigned char hdr_inq[sizeof(struct sg_header) + sizeof(inq)];
	unsigned char com1[6] = { 0x12, 0, 0, 0, sizeof(hdr_inq), 0 };

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmd_len = sizeof(com1);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(inq);
	header.cmdp = com1;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = &inq;
	header.timeout = 60000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);
	if (status)
		return;

	ssize_t length = sizeof(inq.VendorIdentification);
	drive->vendor = malloc(length + 1);
	strncpy(drive->vendor, (char *) inq.VendorIdentification, length);
	drive->vendor[length] = '\0';
	for (length--; drive->vendor[length] ==  ' '; length--)
		drive->vendor[length] = '\0';
	drive->vendor = realloc(drive->vendor, strlen(drive->vendor) + 1);

	length = sizeof(inq.ProductIdentification);
	drive->model = malloc(length + 1);
	strncpy(drive->model, (char *) inq.ProductIdentification, length);
	drive->model[length] = '\0';
	for (length--; drive->model[length] ==  ' '; length--)
		drive->model[length] = '\0';
	drive->model = realloc(drive->model, strlen(drive->model) + 1);

	drive->revision = malloc(5);
	strncpy(drive->revision, (char *) inq.ProductRevisionLevel, 4);
	drive->revision[4] = '\0';

	unsigned char com2[6] = { 0x12, 1, 0x80, 0, 30, 0 };
	char buffer[30];

	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmd_len = sizeof(com2);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = 30;
	header.cmdp = com2;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = &buffer;
	header.timeout = 60000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	status = ioctl(fd, SG_IO, &header);
	if (status)
		return;

	length = buffer[3];
	drive->serial_number = malloc(length + 1);
	strncpy(drive->serial_number, buffer + 4, length);
	drive->serial_number[length] = '\0';
}

