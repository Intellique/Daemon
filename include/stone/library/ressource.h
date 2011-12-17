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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sat, 17 Dec 2011 17:39:50 +0100                         *
\*************************************************************************/

#ifndef __STORIQARCHIVER_LIBRARY_RESSOURCE_H__
#define __STORIQARCHIVER_LIBRARY_RESSOURCE_H__

struct sa_ressource {
    struct sa_ressource_ops {
        int (*free)(struct sa_ressource * res);
        int (*lock)(struct sa_ressource * res);
        int (*trylock)(struct sa_ressource * res);
        void (*unlock)(struct sa_ressource * res);
    } * ops;
    void * data;
    int locked;
};

int sa_ressource_lock(int nb_res, struct sa_ressource * res1, struct sa_ressource * res2, ...);
struct sa_ressource * sa_ressource_new(void);

#endif

