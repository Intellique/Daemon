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
*  Last modified: Sat, 04 Aug 2012 14:01:43 +0200                         *
\*************************************************************************/

#ifndef __STONED_LIBRARY_CHANGER_H__
#define __STONED_LIBRARY_CHANGER_H__

#include <libstone/library/changer.h>

struct st_pool;

/**
 * \brief Select a \a media by his \a pool
 *
 * This function will select a media from \a pool. But it will also exclude medias contained into \a previous_media.
 *
 * \param[in] pool : \a pool, should not be null
 * \param[in] previous_medias : previous medias returned by this function
 * \param[in] nb_medias : number of medias
 * \returns a locked slot which contains a media from \a pool
 */
struct st_slot * st_changer_find_media_by_pool(struct st_pool * pool, struct st_media ** previous_medias, unsigned int nb_medias);

/**
 * \brief Retreive slot which contains \a media
 *
 * \param[in] media : a media
 * \returns a locked slot or NULL if \a media is not found or the slot which contains \e media is already locked
 */
struct st_slot * st_changer_find_slot_by_media(struct st_media * media);

int st_changer_setup(void);

#endif

