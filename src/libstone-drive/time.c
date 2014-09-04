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

// clock_gettime
#include <time.h>

#include <libstone/drive.h>

#include "time.h"

static struct timespec last_start;

__asm__(".symver stdr_time_start_v1, stdr_time_start@@LIBSTONE_DRIVE_1.2");
void stdr_time_start_v1() {
	clock_gettime(CLOCK_MONOTONIC, &last_start);
}

__asm__(".symver stdr_time_stop_v1, stdr_time_stop@@LIBSTONE_DRIVE_1.2");
void stdr_time_stop_v1(struct st_drive * drive) {
	struct timespec finish;
	clock_gettime(CLOCK_MONOTONIC, &finish);

	drive->operation_duration += (finish.tv_sec - last_start.tv_sec) + (finish.tv_nsec - last_start.tv_nsec) / 1000000000.0;
}

