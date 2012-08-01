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
*  Last modified: Mon, 30 Jul 2012 21:08:01 +0200                         *
\*************************************************************************/

#ifndef __ST_STONED_LIBRARY_SCSI_H__
#define __ST_STONED_LIBRARY_SCSI_H__

#include <libstone/library/changer.h>
#include <libstone/library/drive.h>
#include <libstone/library/media.h>
#include <libstone/library/slot.h>

struct st_scsislot {
	int address;
	int src_address;
};

void st_scsi_loader_check_slot(int fd, struct st_changer * changer, struct st_slot * slot);
void st_scsi_loader_info(int fd, struct st_changer * changer);
int st_scsi_loader_medium_removal(int fd, int allow);
int st_scsi_loader_move(int fd, int transport_address, struct st_slot * from, struct st_slot * to);
int st_scsi_loader_ready(int fd);
void st_scsi_loader_status_new(int fd, struct st_changer * changer, int * transport_address);

int st_scsi_tape_format(int fd, int quick);
void st_scsi_tape_info(int fd, struct st_drive * drive);
int st_scsi_tape_locate(int fd, off_t position);
int st_scsi_tape_position(int fd, struct st_media * media);
int st_scsi_tape_read_mam(int fd, struct st_media * media);
int st_scsi_tape_read_medium_serial_number(int fd, char * medium_serial_number, size_t length);
int st_scsi_tape_read_position(int fd, off_t * position);
int st_scsi_tape_size_available(int fd, struct st_media * media);

#endif

