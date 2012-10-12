/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sat, 13 Oct 2012 00:06:16 +0200                         *
\*************************************************************************/

// NULL
#include <stddef.h>

#include <libstone/checksum.h>
#include <libstone/database.h>
#include <libstone/job.h>
#include <libstone/plugin.h>

bool st_plugin_check(const struct st_plugin * plugin) {
	if (plugin == NULL)
		return false;

	if (plugin->checksum > 0 && plugin->checksum != STONE_CHECKSUM_API_LEVEL)
		return false;

	if (plugin->database > 0 && plugin->database != STONE_DATABASE_API_LEVEL)
		return false;

	if (plugin->job > 0 && plugin->job != STONE_JOB_API_LEVEL)
		return false;

	return true;
}

