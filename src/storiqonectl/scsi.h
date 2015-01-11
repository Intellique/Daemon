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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

#ifndef __STORIQONECTL_SCSI_H__
#define __STORIQONECTL_SCSI_H__

// bool
#include <stdbool.h>

struct so_changer;
struct so_drive;
struct so_value;

/**
 * \brief Inquiry the changer
 *
 * \param[in] filename : a generic scsi filename which used by a changer
 * \param[out] changer : an already allocated changer
 * \returns 0 if ok
 */
int soctl_scsi_loaderinfo(const char * filename, struct so_changer * changer, struct so_value * available_drives);

/**
 * \brief Inquiry the drive
 *
 * \param[in] filename : a generic scsi filename which used by a tape drive
 * \param[out] drive : an already allocated drive
 * \returns 0 if ok
 */
int soctl_scsi_tapeinfo(const char * filename, struct so_drive * drive);

#endif

