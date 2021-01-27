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

#ifndef __LIBSTORIQONE_BACKUP_H__
#define __LIBSTORIQONE_BACKUP_H__

// bool
#include <stdbool.h>
// time_t
#include <time.h>

struct so_value;

/**
 * \struct so_backup
 * \brief Represents a backup of database
 */
struct so_backup {
	/**
	 * \brief backup time
	 */
	time_t timestamp;
	/**
	 * \brief number of medias at this backup
	 */
	long nb_medias;
	/**
	 * \brief number of archives at this backup
	 */
	long nb_archives;

	/**
	 * \struct so_backup_volume
	 * \brief volume's information related to this backup
	 *
	 * \var so_backup::volumes
	 * \brief volumes in current backup
	 */
	struct so_backup_volume {
		/**
		 * \brief Media of current volume
		 */
		struct so_media * media;
		/**
		 * \brief File position in current media
		 *
		 * \note Usefull when media is a tape
		 */
		unsigned int position;

		/**
		 * \brief size of this volume
		 */
		size_t size;

		/**
		 * \brief result of last check
		 */
		time_t checktime;
		/**
		 * \brief Last volume check time.
		 *
		 * \note Time is store as Unix timestamp.
		 *
		 * \note If \a check_time equals to \b 0 then it means
		 * than this volume has never been checked
		 */
		bool checksum_ok;

		/**
		 * \brief An hashtable which contains digests of this volume.
		 *
		 * In this hashtable, keys are algorithm and values are digests.
		 */
		struct so_value * digests;

		/**
		 * \brief private data used by database
		 */
		struct so_value * db_data;
	} * volumes;
	/**
	 * \brief Number of volumes of archive
	 */
	unsigned int nb_volumes;

	/**
	 * \brief Job which create this backup
	 *
	 * \warning MAY BE NULL
	 */
	struct so_job * job;

	/**
	 * \brief private data used by database
	 */
	struct so_value * db_data;
};

#endif
