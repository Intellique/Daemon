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
*  Last modified: Tue, 31 Jan 2012 11:36:51 +0100                         *
\*************************************************************************/

// ssize_t
#include <sys/types.h>

// sg_io_hdr_t
#include <scsi/scsi.h>
#include <scsi/sg.h>
// calloc, malloc
#include <stdlib.h>
// memset, strdup, strncpy
#include <string.h>
// ioctl
#include <sys/ioctl.h>

#include "scsi.h"

typedef struct ElementModeSensePageHeader {
	unsigned char PageCode;               /* byte 0 */
	unsigned char ParameterLengthList;    /* byte 1; */
	unsigned char MediumTransportStartHi; /* byte 2,3 */
	unsigned char MediumTransportStartLo;
	unsigned char NumMediumTransportHi;   /* byte 4,5 */
	unsigned char NumMediumTransportLo;   /* byte 4,5 */
	unsigned char StorageStartHi;         /* byte 6,7 */
	unsigned char StorageStartLo;         /* byte 6,7 */
	unsigned char NumStorageHi;           /* byte 8,9 */
	unsigned char NumStorageLo;           /* byte 8,9 */
	unsigned char ImportExportStartHi;    /* byte 10,11 */
	unsigned char ImportExportStartLo;    /* byte 10,11 */
	unsigned char NumImportExportHi;      /* byte 12,13 */
	unsigned char NumImportExportLo;      /* byte 12,13 */
	unsigned char DataTransferStartHi;    /* byte 14,15 */
	unsigned char DataTransferStartLo;    /* byte 14,15 */
	unsigned char NumDataTransferHi;      /* byte 16,17 */
	unsigned char NumDataTransferLo;      /* byte 16,17 */
	unsigned char Reserved1;              /* byte 18, 19 */
	unsigned char Reserved2;              /* byte 18, 19 */
} ElementModeSensePage_T;

typedef struct ElementStatusDataHeader {
	unsigned char FirstElementAddressReported[2]; /* Bytes 0-1 */
	unsigned char NumberOfElementsAvailable[2];	  /* Bytes 2-3 */
	unsigned char :8;					          /* Byte 4 */
	unsigned char ByteCountOfReportAvailable[3];  /* Bytes 5-7 */
} ElementStatusDataHeader_T;

typedef enum ElementTypeCode {
	AllElementTypes        = 0,
	MediumTransportElement = 1,
	StorageElement         = 2,
	ImportExportElement    = 3,
	DataTransferElement    = 4,
} ElementTypeCode_T;

typedef struct ElementStatusPage {
	ElementTypeCode_T ElementTypeCode:8;			/* Byte 0 */
	unsigned char :6;					/* Byte 1 Bits 0-5 */
	unsigned char AVolTag:1;					/* Byte 1 Bit 6 */
	unsigned char PVolTag:1;					/* Byte 1 Bit 7 */
	unsigned char ElementDescriptorLength[2];		/* Bytes 2-3 */
	unsigned char :8;					/* Byte 4 */
	unsigned char ByteCountOfDescriptorDataAvailable[3];	/* Bytes 5-7 */
} ElementStatusPage_T;

typedef struct Element2StatusPage {
	ElementTypeCode_T ElementTypeCode:8;			/* Byte 0 */
	unsigned char VolBits ; /* byte 1 */
#define E2_PVOLTAG 0x80
#define E2_AVOLTAG 0x40
	unsigned char ElementDescriptorLength[2];		/* Bytes 2-3 */
	unsigned char :8;					/* Byte 4 */
	unsigned char ByteCountOfDescriptorDataAvailable[3];	/* Bytes 5-7 */
} Element2StatusPage_T;

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

typedef struct TransportElementDescriptor {
	unsigned char ElementAddress[2];			/* Bytes 0-1 */
	unsigned char Full:1;					/* Byte 2 Bit 0 */
	unsigned char :1;					/* Byte 2 Bit 1 */
	unsigned char Except:1;					/* Byte 2 Bit 2 */
	unsigned char :5;					/* Byte 2 Bits 3-7 */
	unsigned char :8;					/* Byte 3 */
	unsigned char AdditionalSenseCode;			/* Byte 4 */
	unsigned char AdditionalSenseCodeQualifier;		/* Byte 5 */
	unsigned char :8;					/* Byte 6 */
	unsigned char :8;					/* Byte 7 */
	unsigned char :8;					/* Byte 8 */
	unsigned char :6;					/* Byte 9 Bits 0-5 */
	unsigned char SValid:1;					/* Byte 9 Bit 6 */
	unsigned char Invert:1;					/* Byte 9 Bit 7 */
	unsigned char SourceStorageElementAddress[2];		/* Bytes 10-11 */
	unsigned char PrimaryVolumeTag[36];          /* barcode */
	unsigned char AlternateVolumeTag[36];   
	unsigned char Reserved[4];				/* 4 extra bytes? */
} TransportElementDescriptor_T;

enum Mam_attribute {
	Mam_remaining_capacity  = 0x0000,
	Mam_maximum_capacity    = 0x0001,
	Mam_load_count          = 0x0003,
	Mam_mam_space_remaining = 0x0004,

	Mam_device_at_last_load           = 0x020A,
	Mam_device_at_last_load_2         = 0x020B,
	Mam_device_at_last_load_3         = 0x020C,
	Mam_device_at_last_load_4         = 0x020D,
	Mam_total_written_in_medium_life  = 0x0220,
	Mam_total_read_in_medium_life     = 0x0221,
	Mam_total_written_in_current_load = 0x0222,
	Mam_total_read_current_load       = 0x0223,

	Mam_medium_manufacturer      = 0x0400,
	Mam_medium_serial_number     = 0x0401,
	Mam_medium_manufacturer_date = 0x0406,
	Mam_mam_capacity             = 0x0407,
	Mam_medium_type              = 0x0408,
	Mam_medium_type_information  = 0x0409,

	Mam_application_vendor           = 0x0800,
	Mam_application_name             = 0x0801,
	Mam_application_version          = 0x0802,
	Mam_user_medium_text_label       = 0x0803,
	Mam_date_and_time_last_written   = 0x0804,
	Mam_text_localization_identifier = 0x0805,
	Mam_barcode                      = 0x0806,
	Mam_owning_host_textual_name     = 0x0807,
	Mam_media_pool                   = 0x0808,

	Mam_unique_cardtrige_identity = 0x1000,
};

static void st_scsi_mtx_status_update_slot(int fd, struct st_changer * changer, int start_element, int nb_elements, int num_bytes, enum ElementTypeCode type);


void st_scsi_loaderinfo(int fd, struct st_changer * changer) {
	Inquiry_T inq;
	RequestSense_T sense;

	unsigned char hdr_inq[sizeof(struct sg_header) + sizeof(inq)];
	unsigned char command[6] = { 0x12, 0, 0, 0, sizeof(hdr_inq), 0 };

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(inq);
	header.cmdp = command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = &inq;
	header.timeout = 60000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	int status = ioctl(fd, SG_IO, &header);
	if (status)
		return;

	ssize_t length = sizeof(inq.VendorIdentification);
	changer->vendor = malloc(length + 1);
	strncpy(changer->vendor, (char *) inq.VendorIdentification, length);
	changer->vendor[length] = '\0';
	for (length--; changer->vendor[length] ==  ' '; length--)
		changer->vendor[length] = '\0';

	length = sizeof(inq.ProductIdentification);
	changer->model = malloc(length + 1);
	strncpy(changer->model, (char *) inq.ProductIdentification, length);
	changer->model[length] = '\0';
	for (length--; changer->model[length] ==  ' '; length--)
		changer->model[length] = '\0';

	changer->revision = malloc(5);
	strncpy(changer->revision, (char *) inq.ProductRevisionLevel, 4);
	changer->revision[4] = '\0';

	changer->barcode = 0;
	if (inq.AdditionalLength > 50 && inq.VendorFlags & 1)
		changer->barcode = 1;

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
	changer->serial_number = malloc(length + 1);
	strncpy(changer->serial_number, buffer + 4, length);
	changer->serial_number[length] = '\0';
}

int st_scsi_mtx_move(int fd, struct st_changer * ch, struct st_slot * from, struct st_slot * to) {
	Inquiry_T inq;
	RequestSense_T sense;

	unsigned char command[12];
	command[0] = 0xA5;
	command[1] = command[8] = command[9] = command[10] = command[11] = 0;

	command[2] = ch->transport_address >> 8;
	command[3] = ch->transport_address;

	command[4] = from->address >> 8;
	command[5] = from->address;

	command[6] = to->address >> 8;
	command[7] = to->address;

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmd_len = sizeof(command);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = 12;
	header.cmdp = command;
	header.sbp = (unsigned char *) &sense;
	header.dxferp = &inq;
	header.timeout = 1200000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	ioctl(fd, SG_IO, &header);

	return sense.ErrorCode;
}

void st_scsi_mtx_status_new(int fd, struct st_changer * changer) {
	RequestSense_T s1;
	unsigned char b1[136];
	static unsigned char c1[6] = { 0x1A, 0x08, 0x1D, 0, 136, 0 };

	sg_io_hdr_t h1;
	memset(&h1, 0, sizeof(h1));
	memset(&s1, 0, sizeof(s1));
	memset(b1, 0, sizeof(b1));

	h1.interface_id = 'S';
	h1.cmd_len = sizeof(c1);
	h1.mx_sb_len = sizeof(s1);
	h1.dxfer_len = sizeof(b1);
	h1.cmdp = c1;
	h1.sbp = (unsigned char *) &s1;
	h1.dxferp = b1;
	h1.timeout = 60000;
	h1.dxfer_direction = SG_DXFER_FROM_DEV;

	ioctl(fd, SG_IO, &h1);

	ElementModeSensePage_T * sense_page = (ElementModeSensePage_T *) (b1 + 4 + b1[3]);

	changer->nb_slots = ((int) sense_page->NumStorageHi << 8) + sense_page->NumStorageLo;
	unsigned int nb_io_slots = ((int) sense_page->NumImportExportHi << 8) + sense_page->NumImportExportLo;
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

	//unsigned int nb_drives = ((int) sense_page->NumDataTransferHi << 8) + sense_page->NumMediumTransportLo;
	unsigned int num_bytes = (sizeof(ElementStatusDataHeader_T) + 4 * sizeof(ElementStatusPage_T) + changer->nb_slots * sizeof(TransportElementDescriptor_T));

	int start = ((int) sense_page->StorageStartHi << 8) + sense_page->StorageStartLo;
	st_scsi_mtx_status_update_slot(fd, changer, start, changer->nb_slots, num_bytes, StorageElement);

	start = ((int) sense_page->DataTransferStartHi << 8) + sense_page->DataTransferStartLo;
	st_scsi_mtx_status_update_slot(fd, changer, start, changer->nb_drives, num_bytes, DataTransferElement);

	if (nb_io_slots > 0) {
		start = ((int) sense_page->ImportExportStartHi << 8) + sense_page->ImportExportStartLo;
		st_scsi_mtx_status_update_slot(fd, changer, start, nb_io_slots, num_bytes, ImportExportElement);
	}

	changer->transport_address = ((int) sense_page->MediumTransportStartHi << 8) + sense_page->MediumTransportStartLo;
}

void st_scsi_mtx_status_update_slot(int fd, struct st_changer * changer, int start_element, int nb_elements, int num_bytes, enum ElementTypeCode type) {
	struct {
		int word1;
		int word2;
	} idlun;
	ioctl(fd, SCSI_IOCTL_GET_IDLUN, &idlun);
	//int id = idlun.word1 & 0xFF;
	int lun = idlun.word1 >> 8 & 0xFF;

	unsigned char command[12] = { 0xB8, };
	command[6] = command[10] = command[11] = 0;

	command[1] = (lun << 5) | (changer->barcode ? 0x10 : 0) | type;

	command[2] = start_element >> 8;
	command[3] = start_element & 0xFF;

	command[4] = nb_elements >> 8;
	command[5] = nb_elements & 0xFF;

	command[7] = num_bytes >> 16;
	command[8] = num_bytes >> 8;
	command[9] = num_bytes & 0xFF;

	sg_io_hdr_t header;
	RequestSense_T sense;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmdp = command;
	header.cmd_len = sizeof(command);
	char * buffer = header.dxferp = malloc(num_bytes);
	header.dxfer_len = num_bytes;
	header.sbp = (unsigned char *) &sense;
	header.mx_sb_len = sizeof(RequestSense_T);
	header.timeout = 60000;
	header.dxfer_direction = SG_DXFER_FROM_DEV;

	ioctl(fd, SG_IO, &header);

	ElementStatusDataHeader_T * elt_status_data_hdr = (ElementStatusDataHeader_T *) buffer;
	buffer += sizeof(ElementStatusDataHeader_T);
	int nb_found_element = elt_status_data_hdr->NumberOfElementsAvailable[0] << 8 | elt_status_data_hdr->NumberOfElementsAvailable[1];

	while (nb_found_element > 0) {
		Element2StatusPage_T * status = (Element2StatusPage_T *) buffer;
		buffer += sizeof(ElementStatusPage_T);
		int trans_elt_desc_length = status->ElementDescriptorLength[0] << 8 | status->ElementDescriptorLength[1];

		int bytes_available = status->ByteCountOfDescriptorDataAvailable[0] << 16 | status->ByteCountOfDescriptorDataAvailable[1] << 8 | status->ByteCountOfDescriptorDataAvailable[2];
		if (bytes_available <= 0)
			nb_found_element--;

		while (bytes_available > 0) {
			TransportElementDescriptor_T * trans_elt_desc = (TransportElementDescriptor_T *) buffer;
			buffer += trans_elt_desc_length;
			bytes_available -= trans_elt_desc_length;

			int address = trans_elt_desc->ElementAddress[0] << 8 | trans_elt_desc->ElementAddress[1];
			int src_address = trans_elt_desc->SourceStorageElementAddress[0] << 8 | trans_elt_desc->SourceStorageElementAddress[1];
			int index_slots = address;
			switch (type) {
				case DataTransferElement:
					index_slots -= start_element;
					break;

				case ImportExportElement:
					index_slots = changer->nb_slots - nb_found_element;
					break;

				case StorageElement:
					index_slots += changer->nb_drives - start_element;
					break;

				default:
					break;
			}
			struct st_slot * sl = changer->slots + index_slots;

			sl->full = trans_elt_desc->Full > 0;
			if (sl->full) {
				strncpy(sl->volume_name, (char *) trans_elt_desc->PrimaryVolumeTag, 36);
				sl->volume_name[36] = '\0';

				int i;
				for (i = 35; i >= 0 && sl->volume_name[i] == ' '; i--)
					sl->volume_name[i] = '\0';
			} else
				*sl->volume_name = '\0';
			if (type == ImportExportElement)
				sl->is_import_export_slot = 1;
			sl->address = address;
			sl->src_address = type == DataTransferElement ? src_address : 0;

			nb_found_element--;
		}
	}

	free(header.dxferp);
}

int st_scsi_tape_postion(int fd, struct st_tape * tape) {
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
		enum Mam_attribute attr_id = (ptr[0] << 8) | ptr[1];
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
			case Mam_remaining_capacity:
				tape->available_block = 0;
				for (i = 0; i < attr_length; i++) {
					tape->available_block <<= 8;
					tape->available_block += (unsigned char) attr[i];
				}
				tape->available_block <<= 10;
				tape->available_block /= (tape->block_size >> 10);
				break;

			case Mam_load_count:
				tape->load_count = 0;
				for (i = 0; i < attr_length; i++) {
					tape->load_count <<= 8;
					tape->load_count += (unsigned char) attr[i];
				}
				break;

			case Mam_medium_serial_number:
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

	unsigned char com1[10] = { 0x4D, 0, 0x31, 0, 0, 0, 0, 2048 >> 8 & 0xFF, 2048 & 0xFF, 0 };

	sg_io_hdr_t header;
	memset(&header, 0, sizeof(header));
	memset(&sense, 0, sizeof(sense));

	header.interface_id = 'S';
	header.cmd_len = sizeof(com1);
	header.mx_sb_len = sizeof(sense);
	header.dxfer_len = sizeof(buffer);
	header.cmdp = com1;
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

