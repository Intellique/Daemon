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
*  Last modified: Thu, 31 Jan 2013 10:36:41 +0100                            *
\****************************************************************************/

#ifndef __STONECONFIG_SCSI_H__
#define __STONECONFIG_SCSI_H__

// bool
#include <stdbool.h>

#include <libstone/library/changer.h>
#include <libstone/library/drive.h>

/**
 * \brief Check if \a drive is in \a changer
 */
bool stcfg_scsi_drive_in_changer(struct st_drive * drive, struct st_changer * changer);

/**
 * \brief Inquiry the changer
 *
 * \param[in] filename : a generic scsi filename which used by a changer
 * \param[out] changer : an already allocated changer
 * \returns 0 if ok
 */
int stcfg_scsi_loaderinfo(const char * filename, struct st_changer * changer);

void stcfg_scsi_loader_status_new(const char * filename, struct st_changer * changer);

/**
 * \brief Inquiry the drive
 *
 * \param[in] filename : a generic scsi filename which used by a tape drive
 * \param[out] drive : an already allocated drive
 * \returns 0 if ok
 */
int stcfg_scsi_tapeinfo(const char * filename, struct st_drive * drive);

#endif

