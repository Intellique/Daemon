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
*  Copyright (C) 2010, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Tue, 28 Sep 2010 10:41:54 +0200                       *
\***********************************************************************/

#ifndef __STORIQARCHIVER_CONF_H__
#define __STORIQARCHIVER_CONF_H__

/**
 * \brief conf_checkPid
 * \param pid : pid
 * \return 1 is the daemon is alive or
 *         0 if is dead or
 *         -1 if another process used this pid
 */
int conf_checkPid(int pid);
/**
 * \brief conf_deletePid
 * \param pidFile : file with pid
 * \return 0 if ok
 */
int conf_deletePid(const char * pidFile);
/**
 * \brief conf_readPid read pid file
 * \param pidFile : file with pid
 * \return the pid or -1 if not found
 */
int conf_readPid(const char * pidFile);
/**
 * \brief conf_writePid write pid into file
 * \param pidFile : file with pid
 * \param pid : pid
 * \return 0 if ok
 */
int conf_writePid(const char * pidFile, int pid);


/**
 * \brief read config file
 * \param confFile : config file
 * \return 0 if ok
 *         -1 if error
 */
int conf_readConfig(const char * confFile);

#endif

