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
*  Last modified: Wed, 25 Jan 2012 10:36:49 +0100                         *
\*************************************************************************/

#ifndef SCSI_H__
#define SCSI_H__

#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/tape.h>

void st_scsi_loaderinfo(int fd, struct st_changer * changer);
int st_scsi_mtx_move(int fd, struct st_changer * ch, struct st_slot * from, struct st_slot * to);
void st_scsi_mtx_status_new(int fd, struct st_changer * changer);
int st_scsi_tape_postion(int fd, struct st_tape * tape);
int st_scsi_tape_read_mam(int fd, struct st_tape * tape);
int st_scsi_tape_size_available(int fd, struct st_tape * tape);
void st_scsi_tapeinfo(int fd, struct st_drive * drive);

#endif

