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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// bindtextdomain
#include <libintl.h>

#include <libstoriqone-drive/drive.h>

#include "device.h"

#include <drive-tapedrive.chcksum>

#include "config.h"

static struct so_drive_driver sodr_tape_drive_driver = {
	.name = "tape drive",

	.src_checksum = STORIQONE_DRIVE_TAPEDRIVE_SRCSUM,
};

static void tape_driver_init(void) __attribute__((constructor));


static void tape_driver_init() {
	bindtextdomain("storiqone-drive-tape", LOCALE_DIR);

	sodr_tape_drive_driver.device = sodr_tape_drive_get_device();
	sodr_drive_register(&sodr_tape_drive_driver);
}
