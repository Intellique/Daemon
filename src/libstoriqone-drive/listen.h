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

#ifndef __LIBSTORIQONE_DRIVE_LISTEN_H__
#define __LIBSTORIQONE_DRIVE_LISTEN_H__

// bool
#include <stdbool.h>

struct so_database_connection;
struct sodr_peer;
struct so_value;

void sodr_listen_configure(struct so_value * config);
struct so_value * sodr_listen_get_socket_config(void);
bool sodr_listen_is_locked(void);
unsigned int sodr_listen_nb_clients(void);
void sodr_listen_remove_peer(struct sodr_peer * peer);
void sodr_listen_reset_peer(void);
void sodr_listen_set_db_connection(struct so_database_connection * db);
void sodr_listen_set_peer_id(const char * id);

#endif
