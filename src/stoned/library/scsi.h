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
*  Last modified: Mon, 10 Sep 2012 18:53:16 +0200                         *
\*************************************************************************/

#ifndef SCSI_H__
#define SCSI_H__

// bool
#include <stdbool.h>

#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/tape.h>

void st_scsi_loader_check_slot(int fd, struct st_changer * changer, struct st_slot * slot);
void st_scsi_loader_info(int fd, struct st_changer * changer);
int st_scsi_loader_medium_removal(int fd, bool allow);
int st_scsi_loader_move(int fd, struct st_changer * ch, struct st_slot * from, struct st_slot * to);
int st_scsi_loader_ready(int fd);
void st_scsi_loader_status_new(int fd, struct st_changer * changer);

void st_scsi_tape_info(int fd, struct st_drive * drive);
int st_scsi_tape_locate(int fd, off_t position);
int st_scsi_tape_position(int fd, struct st_tape * tape);
int st_scsi_tape_read_mam(int fd, struct st_tape * tape);
int st_scsi_tape_read_medium_serial_number(int fd, char * buffer);
int st_scsi_tape_read_position(int fd, off_t * position);
int st_scsi_tape_size_available(int fd, struct st_tape * tape);

#endif

