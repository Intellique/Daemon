
#ifndef SCSI_H__
#define SCSI_H__

#include <storiqArchiver/library.h>

void sa_scsi_loaderinfo(int fd, struct sa_changer * changer);
void sa_scsi_mtx_status_new(int fd, struct sa_changer * changer);

void sa_realchanger_setup(struct sa_changer * changer);

#endif

