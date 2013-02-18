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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Tue, 25 Dec 2012 22:58:15 +0100                            *
\****************************************************************************/

#ifndef __STONE_LIBRARY_CHANGER_H__
#define __STONE_LIBRARY_CHANGER_H__

// bool
#include <stdbool.h>

struct st_database_connection;
struct st_drive;
struct st_media;
struct st_media_format;
struct st_slot;

enum st_changer_status {
	st_changer_error,
	st_changer_exporting,
	st_changer_idle,
	st_changer_importing,
	st_changer_loading,
	st_changer_unknown,
	st_changer_unloading,
};

/**
 * \struct st_changer
 * \brief Common structure for all changers
 */
struct st_changer {
	/**
	 * \brief Filename of device
	 */
	char * device;
	/**
	 * \brief Status of changer
	 */
	enum st_changer_status status;
	/**
	 * \brief Can use this \a changer
	 */
	bool enabled;

	/**
	 * \brief Model of this \a changer
	 */
	char * model;
	/**
	 * \brief Vendor of this \a changer
	 */
	char * vendor;
	/**
	 * \brief Revision of this \a changer
	 */
	char * revision;
	/**
	 * \brief Serial number of this \a changer
	 */
	char * serial_number;
	/**
	 * \brief This changer has got a barcode reader
	 */
	bool barcode;

	/**
	 * \brief Drives into this \a changer
	 */
	struct st_drive * drives;
	/**
	 * \brief Number of drives into this \a changer
	 */
	unsigned int nb_drives;

	/**
	 * \brief Slots into this \a changer
	 */
	struct st_slot * slots;
	/**
	 * \brief Number of slots into this \a changer
	 */
	unsigned int nb_slots;

	/**
	 * \struct st_changer_ops
	 * \brief Operations associated to this \a changer
	 */
	struct st_changer_ops {
		/**
		 * \brief Find a free drive and acquire \a his lock
		 *
		 * \param[in] : a \a changer
		 * \returns a locked drive
		 * \attention Do not forget to unlock drive after using it
		 */
		struct st_drive * (*find_free_drive)(struct st_changer * ch, struct st_media_format * format, bool for_reading, bool for_writing);
		void (*free)(struct st_changer * ch);
		/**
		 * \brief Load a media
		 *
		 * \param[in] ch : a \a changer
		 * \param[in] from : load this media
		 * \param[in] to : a destination drive
		 * \returns 0 if ok
		 */
		int (*load_media)(struct st_changer * ch, struct st_media * from, struct st_drive * to);
		/**
		 * \brief Load a media by the slot which contains it
		 *
		 * \param[in] ch : a \a changer
		 * \param[in] from : load this media
		 * \param[in] to : a destination drive
		 * \returns 0 if ok
		 */
		int (*load_slot)(struct st_changer * ch, struct st_slot * from, struct st_drive * to);
		int (*shut_down)(struct st_changer * ch);
		/**
		 * \brief Unload media from drive
		 *
		 * \param[in] ch : a \a changer
		 * \param[in] from : \a drive
		 * \returns 0 if ok
		 */
		int (*unload)(struct st_changer * ch, struct st_drive * from);
	} * ops;
	void * data;

	/**
	 * \brief Private data used by database plugin
	 */
	void * db_data;
};

const char * st_changer_status_to_string(enum st_changer_status status);
enum st_changer_status st_changer_string_to_status(const char * status);

#endif

