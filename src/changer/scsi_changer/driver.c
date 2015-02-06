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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#include <libstone-changer/changer.h>

#include "device.h"

#include <changer-scsichanger.chcksum>

static struct st_changer_driver scsi_changer_driver = {
	.name = "scsi changer",

	.api_level    = 0,
	.src_checksum = STONE_CHANGER_SCSICHANGER_SRCSUM,
};

static void scsi_changer_driver_init(void) __attribute__((constructor));


static void scsi_changer_driver_init() {
	scsi_changer_driver.device = scsi_changer_get_device();
	stchgr_changer_register(&scsi_changer_driver);
}

