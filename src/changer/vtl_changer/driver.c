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

#include <libstoriqone-changer/changer.h>

#include "device.h"

#include "config.h"

#include <changer-vtlchanger.chcksum>

static struct so_changer_driver sochgr_vtl_changer_driver = {
	.name = "vtl changer",

	.src_checksum = STORIQONE_CHANGER_VTLCHANGER_SRCSUM,
};

static void sochgr_vtl_changer_driver_init(void) __attribute__((constructor));


static void sochgr_vtl_changer_driver_init() {
	bindtextdomain("storiqone-changer-vtl", LOCALE_DIR);

	sochgr_vtl_changer_driver.device = sochgr_vtl_changer_get_device();
	sochgr_changer_register(&sochgr_vtl_changer_driver);
}
