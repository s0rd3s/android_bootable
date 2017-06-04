/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/

#ifndef MT_ROOTS_H_
#define MT_ROOTS_H_

#include "common.h"
#include "mt_roots_ubi.h"

int mt_setup_install_mounts(void);
void mt_load_volume_table(void);
Volume* mt_volume_for_path(const char* path);

#endif

