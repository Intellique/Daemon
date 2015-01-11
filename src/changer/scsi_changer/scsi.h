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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

#ifndef __SCSICHANGER_SCSI_H__
#define __SCSICHANGER_SCSI_H__

// bool
#include <stdbool.h>

struct st_changer;
struct st_value;

struct scsi_changer_slot {
	int address;
	int src_address;

	struct st_slot * src_slot;
};

void scsi_changer_scsi_loader_check_slot(struct st_changer * changer, const char * device, struct st_slot * slot);
bool scsi_changer_scsi_check_changer(struct st_changer * changer, const char * path);
bool scsi_changer_scsi_check_drive(struct st_drive * drive, const char * path);
int scsi_changer_scsi_loader_ready(const char * device);
int scsi_changer_scsi_medium_removal(const char * device, bool allow);
int scsi_changer_scsi_move(const char * device, int transport_address, struct st_slot * from, struct st_slot * to);
void scsi_changer_scsi_new_status(struct st_changer * changer, const char * device, struct st_value * available_drives, int * transport_address);

#endif

