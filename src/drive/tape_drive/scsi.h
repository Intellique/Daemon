/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

#ifndef __TAPEDRIVE_SCSI_H__
#define __TAPEDRIVE_SCSI_H__

// bool
#include <stdbool.h>

struct st_drive;
struct st_media_format;

bool tape_drive_scsi_check_drive(struct st_drive * drive, const char * path);
bool tape_drive_scsi_check_support(struct st_media_format * format, bool for_writing, const char * path);
int tape_drive_scsi_read_density(struct st_drive * drive, const char * path);
int tape_drive_scsi_read_medium_serial_number(int fd, char * medium_serial_number, size_t length);
int tape_drive_scsi_read_mam(int fd, struct st_media * media);
int tape_drive_scsi_size_available(int fd, struct st_media * media);

#endif
