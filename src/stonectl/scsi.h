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

#ifndef __STONECTL_SCSI_H__
#define __STONECTL_SCSI_H__

// bool
#include <stdbool.h>

struct st_value;

/**
 * \brief Check if \a drive is in \a changer
 */
bool stctl_scsi_drive_in_changer(struct st_value * changer, struct st_value * drive);

/**
 * \brief Inquiry the changer
 *
 * \param[in] filename : a generic scsi filename which used by a changer
 * \param[out] changer : an already allocated changer
 * \returns 0 if ok
 */
int stctl_scsi_loaderinfo(const char * filename, struct st_value * changer);

void stctl_scsi_loader_status_new(struct st_value * changer);

/**
 * \brief Inquiry the drive
 *
 * \param[in] filename : a generic scsi filename which used by a tape drive
 * \param[out] drive : an already allocated drive
 * \returns 0 if ok
 */
int stctl_scsi_tapeinfo(const char * filename, struct st_value * drive);

#endif

