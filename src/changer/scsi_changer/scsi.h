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

#ifndef __SCSICHANGER_SCSI_H__
#define __SCSICHANGER_SCSI_H__

// bool
#include <stdbool.h>

struct so_changer;
struct so_value;

struct sochgr_scsi_changer_slot {
	int address;
	int src_address;

	struct so_slot * src_slot;
};

void sochgr_scsi_changer_scsi_loader_check_slot(struct so_changer * changer, const char * device, struct so_slot * slot);
bool sochgr_scsi_changer_scsi_check_changer(struct so_changer * changer, const char * path);
bool sochgr_scsi_changer_scsi_check_drive(struct so_drive * drive, const char * path);
int sochgr_scsi_changer_scsi_loader_ready(const char * device);
int sochgr_scsi_changer_scsi_medium_removal(const char * device, bool allow);
int sochgr_scsi_changer_scsi_move(const char * device, int transport_address, struct so_slot * from, struct so_slot * to);
void sochgr_scsi_changer_scsi_new_status(struct so_changer * changer, const char * device, struct so_value * available_drives, int * transport_address);
void sochgr_scsi_changer_scsi_update_status(struct so_changer * changer, const char * device);

#endif
