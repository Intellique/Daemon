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
*  Last modified: Mon, 04 Jun 2012 09:42:29 +0200                         *
\*************************************************************************/

// CU_add_suite, CU_cleanup_registry, CU_cleanup_registry
#include <CUnit/CUnit.h>
// glob, globfree
#include <glob.h>
// printf
#include <stdio.h>
// calloc, realloc
#include <stdlib.h>
// time
#include <time.h>
// _exit
#include <unistd.h>

#include <stone/library/changer.h>
#include <stone/library/drive.h>

#include "scsi.h"
#include "test.h"

static int test_stoneconfig_finished(void);
static int test_stoneconfig_init(void);
static void test_stoneconfig_driver_0(void);
static void test_stoneconfig_loader_0(void);
static void test_stoneconfig_loader_1(void);

static struct {
	void (*function)(void);
	char * name;
} test_functions[] = {
	{ test_stoneconfig_loader_0, "loader config #0" },
	{ test_stoneconfig_loader_1, "loader config #1" },
    { test_stoneconfig_driver_0, "drive config #0" },

	{ 0, 0 },
};


static struct st_changer * changers = 0;
static unsigned int nb_fake_changers = 0;
static unsigned int nb_real_changers = 0;
static struct st_drive * drives = 0;
static unsigned int nb_drives = 0;


int test_stoneconfig_finished() {
    unsigned int i;
    for (i = 0; i < nb_real_changers + nb_fake_changers; i++) {
        struct st_changer * ch = changers + i;

        if (ch->device)
            free(ch->device);
        ch->device = 0;
        if (ch->model)
            free(ch->model);
        ch->model = 0;
        if (ch->vendor)
            free(ch->vendor);
        ch->vendor = 0;
        if (ch->revision)
            free(ch->revision);
        ch->revision = 0;
        if (ch->serial_number)
            free(ch->serial_number);
        ch->serial_number = 0;

        unsigned int j;
        for (j = 0; j < ch->nb_drives; j++) {
            struct st_drive * dr = ch->drives + j;

            if (dr->device)
                free(dr->device);
            dr->device = 0;
            if (dr->scsi_device)
                free(dr->scsi_device);
            dr->scsi_device = 0;
            if (dr->model)
                free(dr->model);
            dr->model = 0;
            if (dr->vendor)
                free(dr->vendor);
            dr->vendor = 0;
            if (dr->revision)
                free(dr->revision);
            dr->revision = 0;
            if (dr->serial_number)
                free(dr->serial_number);
            dr->serial_number = 0;

            dr->changer = 0;
        }

        if (ch->drives)
            free(ch->drives);
        ch->drives = 0;
    }

    if (nb_real_changers + nb_fake_changers > 0)
        free(changers);

    return 0;
}

int test_stoneconfig_init() {
	glob_t gl;
	gl.gl_offs = 0;
	glob("/sys/class/scsi_device/*/device/scsi_tape", GLOB_DOOFFS, 0, &gl);

	if (gl.gl_pathc == 0) {
		globfree(&gl);
		return 0;
	}

	drives = calloc(gl.gl_pathc, sizeof(struct st_drive));
	nb_drives = gl.gl_pathc;

	unsigned int i;
	for (i = 0; i < gl.gl_pathc; i++) {
		char * ptr = strrchr(gl.gl_pathv[i], '/');
		*ptr = '\0';

		char link[256];
		ssize_t length = readlink(gl.gl_pathv[i], link, 256);
		link[length] = '\0';

		ptr = strrchr(link, '/') + 1;
		int host = 0, target = 0, channel = 0, bus = 0;
		sscanf(ptr, "%d:%d:%d:%d", &host, &target, &channel, &bus);

		char path[256];
		strcpy(path, gl.gl_pathv[i]);
		strcat(path, "/generic");
		length = readlink(path, link, 256);
		link[length] = '\0';

		char scsi_device[12];
		ptr = strrchr(link, '/');
		strcpy(scsi_device, "/dev");
		strcat(scsi_device, ptr);

		strcpy(path, gl.gl_pathv[i]);
		strcat(path, "/tape");
		length = readlink(path, link, 256);
		link[length] = '\0';

		char device[12];
		ptr = strrchr(link, '/') + 1;
		strcpy(device, "/dev/n");
		strcat(device, ptr);

		drives[i].id = -1;
		drives[i].device = strdup(device);
		drives[i].scsi_device = strdup(scsi_device);
		drives[i].status = ST_DRIVE_UNKNOWN;
		drives[i].model = 0;
		drives[i].vendor = 0;
		drives[i].revision = 0;
		drives[i].serial_number = 0;

		drives[i].host = host;
		drives[i].target = target;
		drives[i].channel = channel;
		drives[i].bus = bus;

		drives[i].changer = 0;
		drives[i].slot = 0;

		drives[i].data = 0;
		drives[i].file_position = 0;
		drives[i].nb_files = 0;
		drives[i].block_number = 0;

		drives[i].is_bottom_of_tape = 0;
		drives[i].is_end_of_file = 0;
		drives[i].is_writable = 0;
		drives[i].is_online = 0;
		drives[i].is_door_opened = 0;

		drives[i].block_size = 0;
		drives[i].density_code = 0;
		drives[i].operation_duration = 0;
		drives[i].last_clean = time(0);
	}
	globfree(&gl);

	gl.gl_offs = 0;
	glob("/sys/class/scsi_changer/*/device", GLOB_DOOFFS, 0, &gl);

	changers = calloc(nb_drives, sizeof(struct st_changer));
	nb_real_changers = gl.gl_pathc;

	for (i = 0; i < gl.gl_pathc; i++) {
		char link[256];
		ssize_t length = readlink(gl.gl_pathv[i], link, 256);
		link[length] = '\0';

		char * ptr = strrchr(link, '/') + 1;
		int host = 0, target = 0, channel = 0, bus = 0;
		sscanf(ptr, "%d:%d:%d:%d", &host, &target, &channel, &bus);

		char path[256];
		strcpy(path, gl.gl_pathv[i]);
		strcat(path, "/generic");
		length = readlink(path, link, 256);
		link[length] = '\0';

		char device[12];
		ptr = strrchr(link, '/');
		strcpy(device, "/dev");
		strcat(device, ptr);

		changers[i].id = -1;
		changers[i].device = strdup(device);
		changers[i].status = ST_CHANGER_UNKNOWN;
		changers[i].model = 0;
		changers[i].vendor = 0;
		changers[i].revision = 0;
		changers[i].serial_number = 0;
		changers[i].barcode = 0;

		changers[i].host = host;
		changers[i].target = target;
		changers[i].channel = channel;
		changers[i].bus = bus;

		changers[i].drives = 0;
		changers[i].nb_drives = 0;
		changers[i].slots = 0;
		changers[i].nb_slots = 0;

		changers[i].data = 0;
		changers[i].lock = 0;
		changers[i].transport_address = 0;
	}
	globfree(&gl);

	// link drive to real changer
	unsigned int nb_changer_without_drive = 0;
	for (i = 0; i < nb_real_changers; i++) {
		unsigned j;
		for (j = 0; j < nb_drives; j++) {
			if (changers[i].host == drives[j].host && changers[i].target == drives[j].target && changers[i].channel == drives[j].channel) {
				drives[j].changer = changers + i;

				changers[i].drives = realloc(changers[i].drives, (changers[i].nb_drives + 1) * sizeof(struct st_drive));
				changers[i].drives[changers[i].nb_drives] = drives[j];
				changers[i].nb_drives++;
			}
		}

		if (changers[i].nb_drives == 0)
			nb_changer_without_drive++;
	}

	// link stand-alone drive to fake changer
	nb_fake_changers = 0;
	for (i = nb_real_changers; i < nb_drives; i++) {
		unsigned j;
		for (j = 0; j < nb_drives; j++) {
			if (!drives[j].changer) {
				drives[j].changer = changers + i;

				changers[i].id = -1;
				changers[i].device = strdup("");
				changers[i].status = ST_CHANGER_UNKNOWN;
				changers[i].model = strdup(drives[j].model);
				changers[i].vendor = strdup(drives[j].vendor);
				changers[i].revision = strdup(drives[j].revision);
				changers[i].serial_number = strdup(drives[j].serial_number);
				changers[i].barcode = 0;

				changers[i].drives = malloc(sizeof(struct st_drive));
				*changers[i].drives = drives[j];
				changers[i].nb_drives = 1;

				nb_fake_changers++;
			}
		}
	}

	return 0;
}

void test_stoneconfig_add_suite() {
	CU_pSuite suite = CU_add_suite("Stone-config: loader-info", test_stoneconfig_init, test_stoneconfig_finished);
	if (!suite) {
		CU_cleanup_registry();
		printf("Error while adding suite stone-config because %s\n", CU_get_error_msg());
		_exit(3);
	}

	int i;
	for (i = 0; test_functions[i].name; i++) {
		if (!CU_add_test(suite, test_functions[i].name, test_functions[i].function)) {
			CU_cleanup_registry();
			printf("Error while adding test function '%s' stone-config because %s\n", test_functions[i].name, CU_get_error_msg());
			_exit(3);
		}
	}
}


void test_stoneconfig_driver_0() {
	if (nb_real_changers < 1 && changers->nb_drives < 1)
		return;

    struct st_drive * dr = changers->drives;
    int status = stcfg_scsi_tapeinfo(dr->scsi_device, dr);
	CU_ASSERT_EQUAL(status, 0);
}

void test_stoneconfig_loader_0() {
	if (nb_real_changers < 1)
		return;

	int status = stcfg_scsi_loaderinfo(changers->device, changers);
	CU_ASSERT_EQUAL(status, 0);
}

void test_stoneconfig_loader_1() {
	if (nb_real_changers < 1)
		return;

	int status = stcfg_scsi_loaderinfo("/dev/foo", changers);
	CU_ASSERT_EQUAL(status, 1);
}

