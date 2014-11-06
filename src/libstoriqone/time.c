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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// localtime_r, time
#include <time.h>

#include <libstoriqone/time.h>

int so_time_cmp(struct timespec * ta, struct timespec * tb) {
	if (ta->tv_nsec < 0 || ta->tv_nsec > 1000000000L)
		so_time_fix(ta);

	if (tb->tv_nsec < 0 || tb->tv_nsec > 1000000000L)
		so_time_fix(tb);

	if (ta->tv_sec == tb->tv_sec) {
		if (ta->tv_nsec < tb->tv_nsec)
			return -1;
		return ta->tv_sec == tb->tv_sec ? 0 : 1;
	}

	return ta->tv_sec < tb->tv_sec ? -1 : 1;
}

size_t so_time_convert(time_t * clock, const char * format, char * buffer, size_t buffer_length) {
	struct tm lnow;
	if (clock == NULL) {
		time_t lclock = time(NULL);
		localtime_r(&lclock, &lnow);
	} else {
		localtime_r(clock, &lnow);
	}

	return strftime(buffer, buffer_length, format, &lnow);
}

long long int so_time_diff(struct timespec * ta, struct timespec * tb) {
	if (ta->tv_nsec < 0 || ta->tv_nsec > 1000000000L)
		so_time_fix(ta);

	if (tb->tv_nsec < 0 || tb->tv_nsec > 1000000000L)
		so_time_fix(tb);

	return 1000000000L * (ta->tv_sec - tb->tv_sec) + ta->tv_nsec - tb->tv_nsec;
}

void so_time_fix(struct timespec * time) {
	if (time->tv_nsec < 0 || time->tv_nsec > 1000000000L) {
		time->tv_sec += time->tv_nsec / 1000000000L;
		time->tv_nsec %= 1000000000L;

		if (time->tv_nsec < 0) {
			time->tv_sec--;
			time->tv_nsec += 1000000000L;
		}
	}
}

