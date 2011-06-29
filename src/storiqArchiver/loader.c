/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Wed, 29 Jun 2011 11:26:18 +0200                       *
\***********************************************************************/

// dlclose, dlerror, dlopen
#include <dlfcn.h>
// glob
#include <glob.h>
// snprintf
#include <stdio.h>
// access
#include <unistd.h>

#include "config.h"
#include "loader.h"

static void * _sa_loader_load_file(const char * filename);

static short _sa_loader_loaded = 0;


void * sa_loader_load(const char * module, const char * name) {
	if (!module || !name)
		return 0;

	char path[256];
	snprintf(path, 256, "%s/%s/lib%s.so", MODULE_PATH, module, name);

	return _sa_loader_load_file(path);
}

void * _sa_loader_load_file(const char * filename) {
	if (access(filename, R_OK | X_OK)) {
		return 0;
	}

	_sa_loader_loaded = 0;

	void * cookie = dlopen(filename, RTLD_NOW);
	if (!cookie) {
		return 0;
	} else if (!_sa_loader_loaded) {
		dlclose(cookie);
		return 0;
	}

	return cookie;
}

void sa_loader_register_ok() {
	_sa_loader_loaded = 1;
}

