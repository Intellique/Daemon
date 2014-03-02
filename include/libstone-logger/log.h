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

#ifndef __STONELOGGER_LOG_H__
#define __STONELOGGER_LOG_H__

struct lgr_log_driver {
	const char * name;

	void * cookie;
	const unsigned int api_level;
	const char * src_checksum;
};

/**
 * \brief Each log driver should call this function only one time
 *
 * \param driver : a static allocated structure
 * \code
 * \_\_attribute\_\_((constructor))
 * static void log_myLog_init() {
 *    lgr_log_register_driver(&log_myLog_module);
 * }
 * \endcode
 */
void lgr_log_register_driver(struct lgr_log_driver * driver);

#endif

